package ch.sideffects.findmyanything

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothClass
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.location.LocationManager
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.ParcelUuid
import android.os.PowerManager
import android.os.SystemClock
import android.util.Log
import androidx.core.content.ContextCompat
import androidx.core.content.IntentCompat
import io.rebble.pebblekit2.client.DefaultPebbleSender
import io.rebble.pebblekit2.common.model.PebbleDictionary
import io.rebble.pebblekit2.common.model.PebbleDictionaryItem
import io.rebble.pebblekit2.common.model.TransmissionResult
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.android.asCoroutineDispatcher
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.ByteArrayOutputStream
import java.util.UUID
import kotlin.math.roundToInt

data class DeviceRow(
    val id: Int,
    val name: String,
    val detail: String,
    val category: Int,
    val rssi: Int,
    val ageSec: Int,
    val classic: Boolean,
    val bonded: Boolean,
    val address: String,
)

data class SearchState(
    val id: Int,
    val name: String,
    val detail: String,
    val category: Int,
    val rssi: Int,
    val peak: Int,
    val ageMs: Long,
    val trend: Double,
    val distanceCm: Int?,
    val rate: Double,
    val address: String,
)

data class UiState(
    val scanning: Boolean = false,
    val watchActive: Boolean = false,
    val status: Int = 0,
    val statusText: String? = null,
    val devices: List<DeviceRow> = emptyList(),
    val search: SearchState? = null,
)

/**
 * Scans for everything nearby (BLE + classic) and feeds the watch and the phone UI.
 * All state lives on one background thread ([handler]).
 */
@SuppressLint("MissingPermission")
object FinderEngine {
    private const val TAG = "FindMyAnything"
    private const val WATCH_TIMEOUT_MS = 150_000L

    enum class Mode { LIST, SEARCH }

    private lateinit var app: Context
    private val thread = HandlerThread("findmyanything").apply { start() }
    private val handler = Handler(thread.looper)
    private val scope = CoroutineScope(SupervisorJob() + handler.asCoroutineDispatcher())

    private val store = DeviceStore()
    private var mode = Mode.LIST
    private var targetId: Int? = null
    private var peak = Double.NaN

    private var watchActive = false
    private var lastWatchContact = 0L
    private var uiActive = false
    private var scanning = false

    private var sender: DefaultPebbleSender? = null
    private var sendJob: Job? = null
    private var tickJob: Job? = null
    private var uiJob: Job? = null

    private var cbAll: Cb? = null
    private var cbFiltered: Cb? = null
    private var filteredTargetAddress: String? = null
    private var wildcardFilters = true
    private val scanStarts = ArrayDeque<Long>()
    private var scanError: String? = null
    private var pendingFilterRestart = false
    private var lastDiscoveryStart = 0L
    private var bondedNames: Map<String, String> = emptyMap()
    private var bondedRefreshed = 0L
    private var receiversRegistered = false

    private val _ui = MutableStateFlow(UiState())
    val ui: StateFlow<UiState> = _ui

    fun init(context: Context) {
        if (::app.isInitialized) return
        app = context.applicationContext
        handler.post { Assigned.load(app) }
    }

    // ---- Public API (any thread) ----

    fun watchOpened() = handler.post {
        Log.i(TAG, "watchapp opened")
        watchActive = true
        lastWatchContact = now()
        updateActive()
    }

    fun watchClosed() = handler.post { closeWatch("app closed") }

    fun watchCommand(cmd: Int, target: Int?) = handler.post {
        lastWatchContact = now()
        if (!watchActive) {
            watchActive = true
            updateActive()
        }
        when (cmd) {
            Protocol.CMD_LIST -> setList()
            Protocol.CMD_SEARCH -> if (target != null) setSearch(target)
            Protocol.CMD_STOP -> closeWatch("stop command")
        }
    }

    fun uiStarted() = handler.post { uiActive = true; updateActive() }
    fun uiStopped() = handler.post { uiActive = false; updateActive() }
    fun uiSearch(id: Int) = handler.post { setSearch(id) }
    fun uiList() = handler.post { setList() }
    fun uiRestart() = handler.post {
        if (scanning) { stopScanning(); }
        scanStarts.clear()
        updateActive()
    }

    // ---- Mode handling ----

    private fun now() = SystemClock.elapsedRealtime()

    private fun setList() {
        if (mode == Mode.LIST && targetId == null) return
        mode = Mode.LIST
        targetId = null
        peak = Double.NaN
        requestFilterRestart()
    }

    private fun setSearch(id: Int) {
        if (mode == Mode.SEARCH && targetId == id) return
        mode = Mode.SEARCH
        targetId = id
        peak = Double.NaN
        Log.i(TAG, "search target $id ${store.byId[id]?.displayName}")
        requestFilterRestart()
    }

    private fun closeWatch(reason: String) {
        if (!watchActive) return
        Log.i(TAG, "watch inactive: $reason")
        watchActive = false
        if (!uiActive) setList()
        updateActive()
    }

    private fun updateActive() {
        val shouldScan = watchActive || uiActive
        if (shouldScan && !scanning) startScanning()
        if (!shouldScan && scanning) stopScanning()

        if (watchActive) {
            if (sender == null) sender = DefaultPebbleSender(app)
            if (sendJob?.isActive != true) sendJob = scope.launch { sendLoop() }
            ScannerService.start(app)
        } else {
            sendJob?.cancel(); sendJob = null
            sender?.close(); sender = null
            ScannerService.stop(app)
        }
        if (uiActive) {
            if (uiJob?.isActive != true) uiJob = scope.launch { uiLoop() }
        } else {
            uiJob?.cancel(); uiJob = null
        }
    }

    // ---- Permissions & status ----

    private fun granted(p: String) = ContextCompat.checkSelfPermission(app, p) == PackageManager.PERMISSION_GRANTED

    fun hasScanPermission(): Boolean = if (Build.VERSION.SDK_INT >= 31) {
        granted(Manifest.permission.BLUETOOTH_SCAN) && granted(Manifest.permission.ACCESS_FINE_LOCATION)
    } else {
        granted(Manifest.permission.ACCESS_FINE_LOCATION)
    }

    private fun hasConnectPermission(): Boolean =
        Build.VERSION.SDK_INT < 31 || granted(Manifest.permission.BLUETOOTH_CONNECT)

    private fun adapter(): BluetoothAdapter? =
        (app.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter

    private fun computeStatus(): Pair<Int, String?> {
        var s = 0
        var text: String? = null
        val ad = adapter()
        if (!hasScanPermission()) {
            s = s or Protocol.ST_NO_PERMISSION; text = "Rechte fehlen: Handy-App öffnen"
        }
        if (ad == null || !ad.isEnabled) {
            s = s or Protocol.ST_BT_OFF; text = "Bluetooth am Handy ist aus"
        }
        val lm = app.getSystemService(Context.LOCATION_SERVICE) as? LocationManager
        if (Build.VERSION.SDK_INT >= 28 && lm != null && !lm.isLocationEnabled) {
            s = s or Protocol.ST_LOCATION_OFF; if (text == null) text = "Standort am Handy ist aus"
        }
        if (Build.VERSION.SDK_INT >= 29 && !granted(Manifest.permission.ACCESS_BACKGROUND_LOCATION)) {
            s = s or Protocol.ST_NO_BG_LOCATION
        }
        val pm = app.getSystemService(Context.POWER_SERVICE) as? PowerManager
        if (pm != null && !pm.isInteractive) s = s or Protocol.ST_SCREEN_OFF
        try {
            if (ad?.isDiscovering == true) s = s or Protocol.ST_CLASSIC_ACTIVE
        } catch (_: SecurityException) {
        }
        scanError?.let { s = s or Protocol.ST_SCAN_ERROR; if (text == null) text = it }
        return s to text
    }

    // ---- Scanning ----

    private fun startScanning() {
        scanning = true
        registerReceivers()
        startBle()
        tickJob?.cancel()
        tickJob = scope.launch {
            while (isActive) {
                tick()
                delay(1000)
            }
        }
        Log.i(TAG, "scanning started")
    }

    private fun stopScanning() {
        scanning = false
        tickJob?.cancel(); tickJob = null
        stopBle()
        try {
            if (adapter()?.isDiscovering == true && hasScanPermission()) adapter()?.cancelDiscovery()
        } catch (_: SecurityException) {
        }
        unregisterReceivers()
        Log.i(TAG, "scanning stopped")
    }

    private fun recentStarts(): Int {
        val n = now()
        while (scanStarts.isNotEmpty() && n - scanStarts.first() > 30_000) scanStarts.removeFirst()
        return scanStarts.size
    }

    private fun settings(filtered: Boolean, ad: BluetoothAdapter): ScanSettings {
        val b = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
            .setReportDelay(0)
        if (filtered) b.setMatchMode(ScanSettings.MATCH_MODE_AGGRESSIVE)
        if (ad.isLeExtendedAdvertisingSupported) {
            b.setLegacy(false)
            b.setPhy(ScanSettings.PHY_LE_ALL_SUPPORTED)
        }
        return b.build()
    }

    private fun uuid16(u: Int): ParcelUuid =
        ParcelUuid(UUID.fromString("0000%04x-0000-1000-8000-00805f9b34fb".format(u)))

    private fun buildFilters(target: Tracked?): List<ScanFilter> {
        val list = ArrayList<ScanFilter>()
        if (wildcardFilters) {
            // Mask 0 = any advertised service UUID. Keeps working with the screen off,
            // where Android pauses unfiltered scans.
            val zero = ParcelUuid(UUID(0L, 0L))
            list += ScanFilter.Builder().setServiceUuid(zero, zero).build()
            if (Build.VERSION.SDK_INT >= 29) list += ScanFilter.Builder().setServiceSolicitationUuid(zero, zero).build()
        }
        for (c in Classifier.FILTER_COMPANIES) list += ScanFilter.Builder().setManufacturerData(c, ByteArray(0)).build()
        for (u in Classifier.FILTER_SERVICE_DATA) list += ScanFilter.Builder().setServiceData(uuid16(u), ByteArray(0)).build()
        if (target != null && target.ble && BluetoothAdapter.checkBluetoothAddress(target.address)) {
            list += ScanFilter.Builder().setDeviceAddress(target.address).build()
        }
        return list
    }

    private fun startBle() {
        val ad = adapter() ?: return
        if (!ad.isEnabled || !hasScanPermission()) return
        val scanner = ad.bluetoothLeScanner ?: return
        scanError = null
        try {
            if (cbAll == null) {
                val cb = Cb(false)
                scanner.startScan(null, settings(false, ad), cb)
                cbAll = cb
                scanStarts.addLast(now())
            }
            startFiltered()
        } catch (e: Exception) {
            Log.w(TAG, "startScan failed", e)
            scanError = "Scan-Fehler: ${e.javaClass.simpleName}"
        }
    }

    private fun startFiltered() {
        val ad = adapter() ?: return
        val scanner = ad.bluetoothLeScanner ?: return
        val target = targetId?.let { store.byId[it] }
        cbFiltered?.let { try { scanner.stopScan(it) } catch (_: Exception) {} }
        val cb = Cb(true)
        scanner.startScan(buildFilters(target), settings(true, ad), cb)
        cbFiltered = cb
        filteredTargetAddress = target?.address
        scanStarts.addLast(now())
    }

    private fun stopBle() {
        val scanner = try { adapter()?.bluetoothLeScanner } catch (_: Exception) { null }
        for (cb in listOfNotNull(cbAll, cbFiltered)) {
            try { scanner?.stopScan(cb) } catch (_: Exception) {}
        }
        cbAll = null; cbFiltered = null
        pendingFilterRestart = false
    }

    /** The filtered scan carries the target's address; restart it when the target changes. */
    private fun requestFilterRestart() {
        pendingFilterRestart = true
        maybeRestartFiltered()
    }

    private fun maybeRestartFiltered() {
        if (!pendingFilterRestart || !scanning || cbFiltered == null) return
        val target = targetId?.let { store.byId[it] }
        if (target?.address == filteredTargetAddress) { pendingFilterRestart = false; return }
        // Android blocks apps that start more than 5 scans per 30 s
        if (recentStarts() >= 4) return
        pendingFilterRestart = false
        try {
            startFiltered()
        } catch (e: Exception) {
            Log.w(TAG, "filtered restart failed", e)
        }
    }

    private class Cb(val filtered: Boolean) : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            handler.post { onBle(result) }
        }

        override fun onBatchScanResults(results: MutableList<ScanResult>) {
            val copy = results.toList()
            handler.post { copy.forEach { onBle(it) } }
        }

        override fun onScanFailed(errorCode: Int) {
            val cb = this
            handler.post { handleScanFailed(cb, errorCode) }
        }
    }

    private fun handleScanFailed(cb: Cb, code: Int) {
        Log.w(TAG, "scan failed filtered=${cb.filtered} code=$code")
        if (cb === cbFiltered) {
            cbFiltered = null
            if (wildcardFilters) {
                // Some controllers reject the wildcard filters; retry without them
                wildcardFilters = false
                handler.postDelayed({ if (scanning) try { startFiltered() } catch (_: Exception) {} }, 2000)
                return
            }
        }
        if (cb === cbAll) cbAll = null
        scanError = when (code) {
            ScanCallback.SCAN_FAILED_APPLICATION_REGISTRATION_FAILED -> "Scan blockiert: Bluetooth neu starten"
            ScanCallback.SCAN_FAILED_FEATURE_UNSUPPORTED -> "Scan nicht unterstützt"
            else -> "Scan-Fehler $code"
        }
    }

    private fun safeName(d: BluetoothDevice): String? = try {
        if (hasConnectPermission()) d.name else null
    } catch (_: SecurityException) {
        null
    }

    private fun onBle(r: ScanResult) {
        if (!scanning) return
        val n = now()
        val address = r.device.address ?: return
        val ad = AdParser.parse(r.scanRecord?.bytes)
        val connectable = Build.VERSION.SDK_INT >= 26 && r.isConnectable
        val tau = if (mode == Mode.SEARCH && store.byId[targetId ?: -1]?.address == address) 700.0 else 1200.0
        val t = store.onBle(address, r.rssi, ad, safeName(r.device), bondedNames[address], connectable, n, tau)
        if (t.id == targetId && t.address != filteredTargetAddress) requestFilterRestart()
    }

    private fun onClassicFound(intent: Intent) {
        if (!scanning) return
        val dev = IntentCompat.getParcelableExtra(intent, BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java) ?: return
        val rssi = intent.getShortExtra(BluetoothDevice.EXTRA_RSSI, Short.MIN_VALUE).toInt()
        if (rssi == Short.MIN_VALUE.toInt()) return
        val type = try { dev.type } catch (_: SecurityException) { BluetoothDevice.DEVICE_TYPE_UNKNOWN }
        if (type == BluetoothDevice.DEVICE_TYPE_LE) return
        val cls = IntentCompat.getParcelableExtra(intent, BluetoothDevice.EXTRA_CLASS, BluetoothClass::class.java)
        val name = bondedNames[dev.address] ?: intent.getStringExtra(BluetoothDevice.EXTRA_NAME) ?: safeName(dev)
        store.onClassic(dev.address, rssi, Classifier.classifyClassic(cls, name), bondedNames.containsKey(dev.address), now())
    }

    private fun tick() {
        val n = now()
        val ad = adapter()
        if (ad != null && ad.isEnabled && hasScanPermission()) {
            if (cbAll == null && recentStarts() < 4) startBle()
            maybeRestartFiltered()
            if (n - bondedRefreshed > 30_000 && hasConnectPermission()) {
                bondedRefreshed = n
                bondedNames = try {
                    ad.bondedDevices.associate { d -> d.address to ((if (Build.VERSION.SDK_INT >= 30) d.alias else null) ?: d.name ?: d.address) }
                } catch (_: SecurityException) { emptyMap() }
            }
            manageClassic(ad, n)
        }
        store.prune(n, targetId)
    }

    private fun manageClassic(ad: BluetoothAdapter, n: Long) {
        try {
            val target = targetId?.let { store.byId[it] }
            when (mode) {
                Mode.LIST -> if (!ad.isDiscovering && n - lastDiscoveryStart > 45_000) {
                    ad.startDiscovery(); lastDiscoveryStart = n
                }
                Mode.SEARCH -> {
                    val classicOnly = target != null && target.classic && !target.ble
                    if (classicOnly && !ad.isDiscovering) {
                        ad.startDiscovery(); lastDiscoveryStart = n
                    } else if (!classicOnly && ad.isDiscovering) {
                        ad.cancelDiscovery() // inquiry disturbs BLE timing
                    }
                }
            }
        } catch (e: SecurityException) {
            Log.w(TAG, "discovery not allowed", e)
        }
    }

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                BluetoothDevice.ACTION_FOUND -> handler.post { onClassicFound(intent) }
                BluetoothAdapter.ACTION_STATE_CHANGED -> handler.post {
                    val st = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)
                    if (st == BluetoothAdapter.STATE_OFF) { cbAll = null; cbFiltered = null }
                }
            }
        }
    }

    private fun registerReceivers() {
        if (receiversRegistered) return
        val f = IntentFilter().apply {
            addAction(BluetoothDevice.ACTION_FOUND)
            addAction(BluetoothAdapter.ACTION_STATE_CHANGED)
        }
        ContextCompat.registerReceiver(app, receiver, f, ContextCompat.RECEIVER_EXPORTED)
        receiversRegistered = true
    }

    private fun unregisterReceivers() {
        if (!receiversRegistered) return
        try { app.unregisterReceiver(receiver) } catch (_: Exception) {}
        receiversRegistered = false
    }

    // ---- Output: watch ----

    private suspend fun sendLoop() {
        var lastList = 0L
        var seq = 0
        while (scope.isActive && watchActive) {
            val n = now()
            if (n - lastWatchContact > WATCH_TIMEOUT_MS) {
                closeWatch("no contact for ${WATCH_TIMEOUT_MS / 1000} s")
                return
            }
            when (mode) {
                Mode.LIST -> if (n - lastList >= 2000) {
                    lastList = n
                    sendList(seq)
                    seq = (seq + 1) and 0xFF
                }
                Mode.SEARCH -> sendSearch()
            }
            delay(250)
        }
    }

    private fun i32(v: Int) = PebbleDictionaryItem.Int32(v)

    /** Sends one message; returns false if the watchapp is gone. */
    private suspend fun send(dict: PebbleDictionary): Boolean {
        val s = sender ?: return false
        val res = try {
            s.sendDataToPebble(Protocol.WATCHAPP_UUID, dict)
        } catch (e: Exception) {
            Log.w(TAG, "send failed", e); null
        }
        if (res == null) return true // Pebble app not reachable right now, try again later
        if (res.values.any { it == TransmissionResult.FailedDifferentAppOpen }) {
            closeWatch("different app open")
            return false
        }
        res.values.firstOrNull { it != TransmissionResult.Success }?.let { Log.d(TAG, "send result $it") }
        return true
    }

    private fun sanitize(s: String): String {
        val sb = StringBuilder()
        for (ch in s) {
            if (ch.code in 0x20..0x7E || ch.code in 0xA0..0x17F) sb.append(ch)
            else if (ch.isWhitespace()) sb.append(' ')
        }
        return sb.toString().trim()
    }

    private fun utf8(s: String, max: Int = Protocol.TEXT_BYTES): ByteArray {
        val clean = sanitize(s)
        var bytes = clean.toByteArray(Charsets.UTF_8)
        if (bytes.size <= max) return bytes
        var end = clean.length
        while (end > 0) {
            bytes = (clean.substring(0, end).trimEnd() + "…").toByteArray(Charsets.UTF_8)
            if (bytes.size <= max) return bytes
            end--
        }
        return ByteArray(0)
    }

    private fun packEntry(t: Tracked, n: Long): ByteArray {
        val o = ByteArrayOutputStream(64)
        o.write(t.id and 0xFF); o.write((t.id shr 8) and 0xFF); o.write((t.id shr 16) and 0xFF); o.write((t.id shr 24) and 0xFF)
        o.write(t.smooth.roundToInt().coerceIn(-127, 20) and 0xFF)
        o.write(t.info.category and 0xFF)
        var fl = 0
        if (t.classic && !t.ble) fl = fl or Protocol.FL_CLASSIC
        if (t.connectable) fl = fl or Protocol.FL_CONNECTABLE
        if (t.bonded) fl = fl or Protocol.FL_BONDED
        if (t.rotating) fl = fl or Protocol.FL_ROTATING
        o.write(fl)
        o.write(((n - t.lastSeen) / 1000).toInt().coerceIn(0, 255))
        val name = utf8(sanitize(t.displayName).ifEmpty { Category.label(t.info.category) })
        o.write(name.size); o.write(name)
        val det = utf8(t.displayDetail)
        o.write(det.size); o.write(det)
        return o.toByteArray()
    }

    private suspend fun sendList(seq: Int) {
        val n = now()
        val all = store.snapshot(n)
        val entries = all.take(Protocol.MAX_LIST).map { packEntry(it, n) }
        val parts = ArrayList<ByteArray>()
        var cur = ByteArrayOutputStream()
        for (e in entries) {
            if (cur.size() + e.size > Protocol.PART_BYTES) { parts += cur.toByteArray(); cur = ByteArrayOutputStream() }
            cur.write(e)
        }
        if (cur.size() > 0 || parts.isEmpty()) parts += cur.toByteArray()
        val (st, text) = computeStatus()
        for ((i, p) in parts.withIndex()) {
            val d = LinkedHashMap<UInt, PebbleDictionaryItem>()
            d[Protocol.KEY_TYPE] = i32(Protocol.TYPE_LIST)
            d[Protocol.KEY_SEQ] = i32(seq)
            d[Protocol.KEY_PART] = i32(i)
            d[Protocol.KEY_PARTS] = i32(parts.size)
            d[Protocol.KEY_TOTAL] = i32(all.size)
            if (p.isNotEmpty()) d[Protocol.KEY_DATA] = PebbleDictionaryItem.Bytes(p)
            if (i == 0) {
                d[Protocol.KEY_STATUS] = i32(st)
                d[Protocol.KEY_STATUS_TEXT] = PebbleDictionaryItem.Text(text ?: "")
            }
            if (!send(d)) return
            if (mode != Mode.LIST) return
        }
    }

    private suspend fun sendSearch() {
        val n = now()
        val (st, text) = computeStatus()
        val t = targetId?.let { store.byId[it] }
        val d = LinkedHashMap<UInt, PebbleDictionaryItem>()
        d[Protocol.KEY_TYPE] = i32(Protocol.TYPE_SEARCH)
        d[Protocol.KEY_STATUS] = i32(st)
        d[Protocol.KEY_STATUS_TEXT] = PebbleDictionaryItem.Text(text ?: "")
        if (t == null || t.smooth.isNaN()) {
            d[Protocol.KEY_AGE] = i32(9999)
            d[Protocol.KEY_RSSI] = i32(-127)
            d[Protocol.KEY_NAME] = PebbleDictionaryItem.Text("Gerät verloren")
            send(d)
            return
        }
        val age = n - t.lastSeen
        if (age < 3000 && (peak.isNaN() || t.smooth > peak)) peak = t.smooth
        d[Protocol.KEY_RSSI] = i32(t.smooth.roundToInt())
        d[Protocol.KEY_RAW] = i32(t.raw)
        d[Protocol.KEY_PEAK] = i32(if (peak.isNaN()) -127 else peak.roundToInt())
        d[Protocol.KEY_AGE] = i32((age / 100).toInt().coerceAtMost(9999))
        d[Protocol.KEY_DIST] = i32(t.distanceCm() ?: -1)
        d[Protocol.KEY_TREND] = i32((t.trend(n) * 10).roundToInt())
        d[Protocol.KEY_RATE] = i32((t.rate(n) * 10).roundToInt())
        d[Protocol.KEY_CATEGORY] = i32(t.info.category)
        d[Protocol.KEY_NAME] = PebbleDictionaryItem.Text(String(utf8(t.displayName, 40), Charsets.UTF_8))
        d[Protocol.KEY_DETAIL] = PebbleDictionaryItem.Text(String(utf8(t.displayDetail, 40), Charsets.UTF_8))
        send(d)
    }

    // ---- Output: phone UI ----

    private suspend fun uiLoop() {
        while (scope.isActive && uiActive) {
            val n = now()
            val (st, text) = computeStatus()
            val rows = store.snapshot(n).map {
                DeviceRow(
                    id = it.id, name = it.displayName, detail = it.displayDetail, category = it.info.category,
                    rssi = it.smooth.roundToInt(), ageSec = ((n - it.lastSeen) / 1000).toInt(),
                    classic = it.classic && !it.ble, bonded = it.bonded, address = it.address,
                )
            }
            val t = if (mode == Mode.SEARCH) targetId?.let { store.byId[it] } else null
            val search = t?.let {
                val age = n - it.lastSeen
                if (!it.smooth.isNaN() && age < 3000 && (peak.isNaN() || it.smooth > peak)) peak = it.smooth
                SearchState(
                    id = it.id, name = it.displayName, detail = it.displayDetail, category = it.info.category,
                    rssi = if (it.smooth.isNaN()) -127 else it.smooth.roundToInt(),
                    peak = if (peak.isNaN()) -127 else peak.roundToInt(),
                    ageMs = age, trend = it.trend(n), distanceCm = it.distanceCm(), rate = it.rate(n),
                    address = it.address,
                )
            }
            _ui.value = UiState(
                scanning = scanning, watchActive = watchActive, status = st, statusText = text,
                devices = rows, search = search,
            )
            delay(500)
        }
    }
}
