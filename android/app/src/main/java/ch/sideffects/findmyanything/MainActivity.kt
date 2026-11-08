package ch.sideffects.findmyanything

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import io.rebble.pebblekit2.client.DefaultPebbleAndroidAppPicker
import io.rebble.pebblekit2.client.DefaultPebbleSender
import kotlinx.coroutines.launch
import kotlin.math.roundToInt

class MainActivity : ComponentActivity() {
    private val resumeCount = mutableIntStateOf(0)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        FinderEngine.init(this)
        setContent {
            MaterialTheme(colorScheme = darkColorScheme()) {
                Surface(modifier = Modifier.fillMaxSize()) {
                    App(resumeCount.intValue)
                }
            }
        }
    }

    override fun onStart() {
        super.onStart()
        FinderEngine.uiStarted()
    }

    override fun onResume() {
        super.onResume()
        resumeCount.intValue++
    }

    override fun onStop() {
        FinderEngine.uiStopped()
        super.onStop()
    }

    private fun granted(p: String) = ContextCompat.checkSelfPermission(this, p) == PackageManager.PERMISSION_GRANTED

    fun runtimePermissions(): Array<String> {
        val l = mutableListOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
        if (Build.VERSION.SDK_INT >= 31) {
            l += Manifest.permission.BLUETOOTH_SCAN
            l += Manifest.permission.BLUETOOTH_CONNECT
        }
        if (Build.VERSION.SDK_INT >= 33) l += Manifest.permission.POST_NOTIFICATIONS
        return l.toTypedArray()
    }

    fun basicGranted() = runtimePermissions().filter { it != Manifest.permission.POST_NOTIFICATIONS }.all { granted(it) }
    fun backgroundGranted() = Build.VERSION.SDK_INT < 29 || granted(Manifest.permission.ACCESS_BACKGROUND_LOCATION)
    fun batteryOk() = getSystemService(PowerManager::class.java)?.isIgnoringBatteryOptimizations(packageName) ?: true
    fun pebbleAppFound() = try {
        DefaultPebbleAndroidAppPicker.getInstance(this).getAllEligibleApps().isNotEmpty()
    } catch (_: Exception) {
        false
    }

    fun openBatterySettings() {
        try {
            startActivity(Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:$packageName")))
        } catch (_: Exception) {
            startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))
        }
    }

    fun openAppSettings() {
        startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:$packageName")))
    }
}

private fun catColor(c: Int): Color = when (c) {
    Category.TRACKER -> Color(0xFFFF5555)
    Category.PHONE -> Color(0xFF55AAFF)
    Category.COMPUTER -> Color(0xFF5555FF)
    Category.AUDIO -> Color(0xFFFFAA00)
    Category.WEARABLE -> Color(0xFF55FF55)
    Category.MEDIA -> Color(0xFFAA55FF)
    Category.BEACON -> Color(0xFF00AAAA)
    Category.SMARTHOME -> Color(0xFFAAFF55)
    Category.INPUT -> Color(0xFFFF55FF)
    Category.VEHICLE -> Color(0xFFFFFF55)
    Category.HEALTH -> Color(0xFFFF5599)
    else -> Color(0xFFAAAAAA)
}

@Composable
private fun App(resumeTick: Int) {
    val act = androidx.compose.ui.platform.LocalContext.current as MainActivity
    val state by FinderEngine.ui.collectAsState()
    val scope = rememberCoroutineScope()
    val permLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        FinderEngine.uiRestart()
    }
    val bgLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) {
        FinderEngine.uiRestart()
    }
    // Re-read setup state each time the activity resumes
    val setup = remember(resumeTick, state.status) {
        listOf(
            Triple("Bluetooth- und Standort-Rechte", act.basicGranted(), 0),
            Triple("Standort «Immer erlauben» (für die Suche mit der Uhr)", act.backgroundGranted(), 1),
            Triple("Akku-Optimierung aus", act.batteryOk(), 2),
            Triple("Pebble-App installiert", act.pebbleAppFound(), 3),
        )
    }

    Column(modifier = Modifier.fillMaxSize().safeDrawingPadding().padding(12.dp)) {
        Text("Find My Anything", fontSize = 26.sp, fontWeight = FontWeight.Bold)
        val sub = buildString {
            append(if (state.scanning) "Suche läuft" else "Suche gestoppt")
            if (state.watchActive) append(" · Uhr verbunden")
            append(" · ${state.devices.size} Geräte")
        }
        Text(sub, color = Color.Gray, fontSize = 13.sp)
        state.statusText?.takeIf { it.isNotEmpty() }?.let {
            Text(it, color = Color(0xFFFF7777), fontSize = 14.sp, modifier = Modifier.padding(top = 4.dp))
        }

        if (setup.any { !it.second }) {
            Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
                Column(Modifier.padding(12.dp)) {
                    Text("Einrichtung", fontWeight = FontWeight.Bold)
                    for ((label, ok, which) in setup) {
                        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(top = 6.dp)) {
                            Text(if (ok) "✓" else "✗", color = if (ok) Color(0xFF55DD55) else Color(0xFFFF7777),
                                modifier = Modifier.width(22.dp))
                            Text(label, modifier = Modifier.weight(1f), fontSize = 14.sp)
                            if (!ok && which != 3) {
                                OutlinedButton(onClick = {
                                    when (which) {
                                        0 -> permLauncher.launch(act.runtimePermissions())
                                        1 -> if (!act.basicGranted()) permLauncher.launch(act.runtimePermissions())
                                             else if (Build.VERSION.SDK_INT >= 29) bgLauncher.launch(Manifest.permission.ACCESS_BACKGROUND_LOCATION)
                                        2 -> act.openBatterySettings()
                                    }
                                }) { Text("Erlauben") }
                            }
                        }
                    }
                    TextButtonRow(act)
                }
            }
        }

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(vertical = 6.dp)) {
            Button(onClick = {
                scope.launch {
                    val s = DefaultPebbleSender(act)
                    try { s.startAppOnTheWatch(Protocol.WATCHAPP_UUID) } finally { s.close() }
                }
            }) { Text("Auf der Uhr öffnen") }
            OutlinedButton(onClick = { FinderEngine.uiRestart() }) { Text("Neu starten") }
        }

        val search = state.search
        if (search != null) {
            SearchPanel(search)
        } else {
            HorizontalDivider()
            LazyColumn(modifier = Modifier.fillMaxSize()) {
                items(state.devices, key = { it.id }) { d -> DeviceItem(d) }
            }
        }
    }
}

@Composable
private fun TextButtonRow(act: MainActivity) {
    Text(
        "App-Einstellungen öffnen",
        color = Color(0xFF77AAFF),
        fontSize = 13.sp,
        modifier = Modifier.padding(top = 8.dp).clickable { act.openAppSettings() },
    )
}

@Composable
private fun DeviceItem(d: DeviceRow) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.fillMaxWidth().clickable { FinderEngine.uiSearch(d.id) }.padding(vertical = 8.dp),
    ) {
        Box(Modifier.size(12.dp).clip(CircleShape).background(catColor(d.category)))
        Spacer(Modifier.width(10.dp))
        Column(Modifier.weight(1f)) {
            Text(d.name, fontWeight = FontWeight.SemiBold, maxLines = 1, overflow = TextOverflow.Ellipsis)
            val extra = buildString {
                append(d.detail)
                if (d.classic) append(" · Classic")
                if (d.bonded) append(" · gekoppelt")
                if (d.ageSec > 5) append(" · vor ${d.ageSec} s")
            }
            Text(extra, color = Color.Gray, fontSize = 12.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        Text("${d.rssi} dBm", fontSize = 14.sp, color = rssiColor(d.rssi))
    }
}

private fun rssiColor(r: Int): Color = when {
    r >= -55 -> Color(0xFFFF5555)
    r >= -67 -> Color(0xFFFFAA00)
    r >= -80 -> Color(0xFFFFFF55)
    else -> Color(0xFF7799FF)
}

private fun proximity(r: Int): String = when {
    r >= -50 -> "direkt daneben"
    r >= -60 -> "sehr nah"
    r >= -70 -> "nah"
    r >= -80 -> "mittel"
    r >= -90 -> "weit"
    else -> "sehr weit"
}

@Composable
private fun SearchPanel(s: SearchState) {
    Column(modifier = Modifier.fillMaxWidth().padding(top = 8.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.size(14.dp).clip(CircleShape).background(catColor(s.category)))
            Spacer(Modifier.width(8.dp))
            Text(s.name, fontSize = 20.sp, fontWeight = FontWeight.Bold, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        Text("${s.detail} · ${s.address}", color = Color.Gray, fontSize = 12.sp)
        Spacer(Modifier.height(16.dp))
        val lost = s.ageMs > 10_000
        Text(
            if (lost) "kein Signal" else "${s.rssi} dBm",
            fontSize = 56.sp, fontWeight = FontWeight.Bold, color = if (lost) Color.Gray else rssiColor(s.rssi),
        )
        Text(if (lost) "seit ${s.ageMs / 1000} s nicht gesehen" else proximity(s.rssi), fontSize = 18.sp)
        Spacer(Modifier.height(12.dp))
        val f = ((s.rssi + 100).coerceIn(0, 70)) / 70f
        LinearProgressIndicator(progress = { f }, modifier = Modifier.fillMaxWidth().height(14.dp))
        Spacer(Modifier.height(12.dp))
        val arrow = when {
            s.trend > 1.0 -> "▲ wärmer"
            s.trend < -1.0 -> "▼ kälter"
            else -> "● gleich"
        }
        Text(arrow, fontSize = 22.sp)
        Text("Bestwert ${s.peak} dBm", color = Color.Gray)
        s.distanceCm?.let { Text("ca. ${formatDist(it)} (grob)", color = Color.Gray) }
        Text("${(s.rate * 10).roundToInt() / 10.0} Messungen/s", color = Color.Gray, fontSize = 12.sp)
        Spacer(Modifier.height(16.dp))
        Button(onClick = { FinderEngine.uiList() }) { Text("Zur Liste") }
    }
}

private fun formatDist(cm: Int): String = if (cm < 100) "$cm cm" else "${(cm / 10) / 10.0} m"
