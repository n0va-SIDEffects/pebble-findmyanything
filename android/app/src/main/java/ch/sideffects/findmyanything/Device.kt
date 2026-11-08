package ch.sideffects.findmyanything

import kotlin.math.exp

/** One device as we track it, across address changes. */
class Tracked(val id: Int, var address: String, var info: Classified) {
    var ble = false
    var classic = false
    var connectable = false
    var bonded = false
    /** Resolvable private address: the device changes it regularly. */
    var rotating = false
    var firstSeen = 0L
    var lastSeen = 0L
    var raw = -127
    var smooth = Double.NaN
    private var lastSampleAt = 0L
    private val lastRaw = ArrayDeque<Int>()
    /** (time, smoothed rssi) for trend; times of samples for rate. */
    private val history = ArrayDeque<Pair<Long, Double>>()
    private val sampleTimes = ArrayDeque<Long>()

    val displayName: String get() = info.name ?: info.label
    val displayDetail: String
        get() = when {
            info.name == null -> info.detail.ifEmpty { Category.label(info.category) }
            info.detail.isNotEmpty() -> info.detail
            else -> info.label
        }

    fun addSample(now: Long, rssi: Int, tauMs: Double = 1000.0) {
        raw = rssi
        lastRaw.addLast(rssi)
        while (lastRaw.size > 3) lastRaw.removeFirst()
        val med = lastRaw.sorted()[lastRaw.size / 2].toDouble()
        if (smooth.isNaN() || now - lastSampleAt > 8000) {
            smooth = med
        } else {
            val dt = (now - lastSampleAt).coerceAtLeast(1).toDouble()
            val alpha = (1.0 - exp(-dt / tauMs)).coerceIn(0.15, 1.0)
            smooth += alpha * (med - smooth)
        }
        lastSampleAt = now
        lastSeen = now
        history.addLast(now to smooth)
        while (history.isNotEmpty() && now - history.first().first > 6000) history.removeFirst()
        sampleTimes.addLast(now)
        while (sampleTimes.isNotEmpty() && now - sampleTimes.first() > 3000) sampleTimes.removeFirst()
    }

    /** Samples per second over the last 3 s. */
    fun rate(now: Long): Double {
        while (sampleTimes.isNotEmpty() && now - sampleTimes.first() > 3000) sampleTimes.removeFirst()
        return sampleTimes.size / 3.0
    }

    /** Slope of the smoothed RSSI over the last 3 s, in dB per second (positive = getting closer). */
    fun trend(now: Long): Double {
        val pts = history.filter { now - it.first <= 3000 }
        if (pts.size < 3) return 0.0
        val span = pts.last().first - pts.first().first
        if (span < 1000) return 0.0
        val t0 = pts.first().first
        val n = pts.size.toDouble()
        var sx = 0.0; var sy = 0.0; var sxx = 0.0; var sxy = 0.0
        for ((t, y) in pts) {
            val x = (t - t0) / 1000.0
            sx += x; sy += y; sxx += x * x; sxy += x * y
        }
        val den = n * sxx - sx * sx
        return if (den == 0.0) 0.0 else (n * sxy - sx * sy) / den
    }

    /** Rough distance in cm if the device reports its transmit power, else null. */
    fun distanceCm(): Int? {
        val m1 = info.measured1m ?: return null
        if (smooth.isNaN()) return null
        val meters = Math.pow(10.0, (m1 - smooth) / 25.0)
        return (meters * 100).toInt().coerceIn(1, 99_999)
    }
}

/** All devices seen recently. Only touched from the engine thread. */
class DeviceStore {
    private var nextId = 1
    private val byAddress = HashMap<String, Tracked>()
    val byId = HashMap<Int, Tracked>()

    private fun isRpa(address: String): Boolean {
        val first = address.substringBefore(':').toIntOrNull(16) ?: return false
        return (first and 0xC0) == 0x40
    }

    fun onBle(
        address: String, rssi: Int, ad: AdInfo, systemName: String?, bondedName: String?,
        connectable: Boolean, now: Long, tauMs: Double,
    ): Tracked {
        val info = Classifier.classifyBle(ad, systemName, bondedName)
        var t = byAddress[address]
        if (t == null) {
            val rotating = isRpa(address)
            if (rotating) {
                // Same kind of device that just went quiet under another address? Then it rotated.
                t = byId.values
                    .filter { it.ble && it.rotating && it.info.signature == info.signature && now - it.lastSeen in 1500..60_000 }
                    .maxByOrNull { it.lastSeen }
                t?.address = address
            }
            if (t == null) {
                t = Tracked(nextId++, address, info)
                t.firstSeen = now
                t.rotating = rotating
                byId[t.id] = t
            }
            byAddress[address] = t
        }
        // Keep a richer classification (a scan response may lack the manufacturer data)
        if (info.category != Category.OTHER || t.info.category == Category.OTHER || !t.ble) {
            t.info = if (info.name == null && t.info.name != null) info.copy(name = t.info.name) else info
        } else if (info.name != null && t.info.name == null) {
            t.info = t.info.copy(name = info.name)
        }
        t.ble = true
        t.connectable = t.connectable || connectable
        t.bonded = bondedName != null
        t.addSample(now, rssi, tauMs)
        return t
    }

    fun onClassic(address: String, rssi: Int, info: Classified, bonded: Boolean, now: Long): Tracked {
        var t = byAddress[address]
        if (t == null) {
            t = Tracked(nextId++, address, info)
            t.firstSeen = now
            byId[t.id] = t
            byAddress[address] = t
        } else if (t.info.category == Category.OTHER && info.category != Category.OTHER) {
            t.info = info.copy(name = t.info.name ?: info.name)
        } else if (t.info.name == null && info.name != null) {
            t.info = t.info.copy(name = info.name)
        }
        t.classic = true
        t.bonded = t.bonded || bonded
        t.addSample(now, rssi, 1000.0)
        return t
    }

    fun prune(now: Long, keepId: Int?) {
        val dead = byId.values.filter { it.id != keepId && now - it.lastSeen > 180_000 }.map { it.id }.toSet()
        if (dead.isEmpty()) return
        dead.forEach { byId.remove(it) }
        byAddress.entries.removeAll { it.value.id in dead }
    }

    /** Devices seen in the last minute: fresh ones first, each group by signal strength. */
    fun snapshot(now: Long): List<Tracked> =
        byId.values.filter { now - it.lastSeen <= 60_000 && !it.smooth.isNaN() }
            .sortedWith(compareBy<Tracked> { now - it.lastSeen > 15_000 }.thenByDescending { it.smooth })

    fun clear() {
        byAddress.clear(); byId.clear()
    }
}
