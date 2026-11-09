package ch.sideffects.findmyanything

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import io.rebble.pebblekit2.client.DefaultPebbleAndroidAppPicker
import io.rebble.pebblekit2.client.DefaultPebbleSender
import kotlinx.coroutines.launch
import kotlin.math.roundToInt

private const val DONATE_URL = "https://buymeacoffee.com/SIDEffects"

class MainActivity : ComponentActivity() {
    private val resumeCount = mutableIntStateOf(0)
    private var shownLanguage = ""

    override fun attachBaseContext(newBase: Context) {
        val s = SettingsStore.read(newBase)
        shownLanguage = s.resolvedLanguage()
        super.attachBaseContext(SettingsStore.localized(newBase, s))
    }

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

    /** Rebuild the screen in the new language. */
    fun languageMaybeChanged() {
        if (SettingsStore.current.resolvedLanguage() != shownLanguage) recreate()
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

    fun openUrl(url: String) {
        try { startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url))) } catch (_: Exception) {}
    }

    fun versionName(): String = try {
        packageManager.getPackageInfo(packageName, 0).versionName ?: ""
    } catch (_: Exception) {
        ""
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
    Category.HEALTH -> Color(0xFFFF55AA)
    else -> Color(0xFFAAAAAA)
}

/** Traffic light, same as the watch: green = right next to it, red = far away. */
private fun rssiColor(r: Int): Color = when {
    r >= -50 -> Color(0xFF33DD55)
    r >= -60 -> Color(0xFFAAEE33)
    r >= -70 -> Color(0xFFFFEE33)
    r >= -80 -> Color(0xFFFFAA33)
    r >= -90 -> Color(0xFFFF7733)
    else -> Color(0xFFFF4444)
}

@Composable
private fun proximity(r: Int): String = stringResource(
    when {
        r >= -50 -> R.string.px_0
        r >= -60 -> R.string.px_1
        r >= -70 -> R.string.px_2
        r >= -80 -> R.string.px_3
        r >= -90 -> R.string.px_4
        else -> R.string.px_5
    }
)

@Composable
private fun App(resumeTick: Int) {
    val act = LocalContext.current as MainActivity
    val settings by SettingsStore.flow.collectAsState()
    // The language can also change from the settings page in the Pebble app
    LaunchedEffect(settings.resolvedLanguage()) { act.languageMaybeChanged() }
    var showSettings by rememberSaveable { mutableStateOf(false) }
    BackHandler(enabled = showSettings) { showSettings = false }
    if (showSettings) SettingsScreen(onBack = { showSettings = false })
    else MainScreen(resumeTick, onSettings = { showSettings = true })
}

@Composable
private fun MainScreen(resumeTick: Int, onSettings: () -> Unit) {
    val act = LocalContext.current as MainActivity
    val state by FinderEngine.ui.collectAsState()
    val scope = rememberCoroutineScope()
    val permLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        FinderEngine.uiRestart()
    }
    val bgLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) {
        FinderEngine.uiRestart()
    }
    val setup = remember(resumeTick, state.status) {
        listOf(
            Triple(R.string.perm_basic, act.basicGranted(), 0),
            Triple(R.string.perm_background, act.backgroundGranted(), 1),
            Triple(R.string.battery, act.batteryOk(), 2),
            Triple(R.string.pebble_app, act.pebbleAppFound(), 3),
        )
    }

    Column(modifier = Modifier.fillMaxSize().safeDrawingPadding().padding(12.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(stringResource(R.string.app_name), fontSize = 26.sp, fontWeight = FontWeight.Bold, modifier = Modifier.weight(1f))
            TextButton(onClick = onSettings) { Text(stringResource(R.string.settings)) }
        }
        val sub = buildString {
            append(stringResource(if (state.scanning) R.string.status_scanning else R.string.status_stopped))
            if (state.watchActive) append(" · ").append(stringResource(R.string.status_watch))
            append(" · ").append(act.resources.getQuantityString(R.plurals.devices, state.devices.size, state.devices.size))
        }
        Text(sub, color = Color.Gray, fontSize = 13.sp)
        state.statusText?.takeIf { it.isNotEmpty() }?.let {
            Text(it, color = Color(0xFFFF7777), fontSize = 14.sp, modifier = Modifier.padding(top = 4.dp))
        }

        if (setup.any { !it.second }) {
            Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
                Column(Modifier.padding(12.dp)) {
                    Text(stringResource(R.string.setup), fontWeight = FontWeight.Bold)
                    for ((label, ok, which) in setup) {
                        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(top = 6.dp)) {
                            Text(if (ok) "✓" else "✗", color = if (ok) Color(0xFF55DD55) else Color(0xFFFF7777),
                                modifier = Modifier.width(22.dp))
                            Text(stringResource(label), modifier = Modifier.weight(1f), fontSize = 14.sp)
                            if (!ok && which != 3) {
                                OutlinedButton(onClick = {
                                    when (which) {
                                        0 -> permLauncher.launch(act.runtimePermissions())
                                        1 -> if (!act.basicGranted()) permLauncher.launch(act.runtimePermissions())
                                             else if (Build.VERSION.SDK_INT >= 29) bgLauncher.launch(Manifest.permission.ACCESS_BACKGROUND_LOCATION)
                                        2 -> act.openBatterySettings()
                                    }
                                }) { Text(stringResource(R.string.allow)) }
                            }
                        }
                    }
                    Text(
                        stringResource(R.string.app_info),
                        color = Color(0xFF77AAFF),
                        fontSize = 13.sp,
                        modifier = Modifier.padding(top = 8.dp).clickable { act.openAppSettings() },
                    )
                }
            }
        }

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(vertical = 6.dp)) {
            Button(onClick = {
                scope.launch {
                    val s = DefaultPebbleSender(act)
                    try { s.startAppOnTheWatch(Protocol.WATCHAPP_UUID) } finally { s.close() }
                }
            }) { Text(stringResource(R.string.open_on_watch)) }
            OutlinedButton(onClick = { FinderEngine.uiRestart() }) { Text(stringResource(R.string.restart)) }
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
private fun DeviceItem(d: DeviceRow) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.fillMaxWidth().clickable { FinderEngine.uiSearch(d.id) }.padding(vertical = 8.dp),
    ) {
        Box(Modifier.size(12.dp).clip(CircleShape).background(catColor(d.category)))
        Spacer(Modifier.width(10.dp))
        Column(Modifier.weight(1f)) {
            Text(d.name, fontWeight = FontWeight.SemiBold, maxLines = 1, overflow = TextOverflow.Ellipsis)
            val paired = stringResource(R.string.paired)
            val ago = stringResource(R.string.seen_ago, d.ageSec)
            val extra = buildString {
                append(d.detail)
                if (d.classic) append(" · Classic")
                if (d.bonded) append(" · ").append(paired)
                if (d.ageSec > 5) append(" · ").append(ago)
            }
            Text(extra, color = Color.Gray, fontSize = 12.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        Text("${d.rssi} dBm", fontSize = 14.sp, color = rssiColor(d.rssi))
    }
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
            if (lost) stringResource(R.string.no_signal) else "${s.rssi} dBm",
            fontSize = 56.sp, fontWeight = FontWeight.Bold, color = if (lost) Color.Gray else rssiColor(s.rssi),
        )
        Text(
            if (lost) stringResource(R.string.not_seen_for, (s.ageMs / 1000).toInt()) else proximity(s.rssi),
            fontSize = 18.sp,
        )
        Spacer(Modifier.height(12.dp))
        val f = ((s.rssi + 100).coerceIn(0, 60)) / 60f
        LinearProgressIndicator(
            progress = { f },
            color = if (lost) Color.Gray else rssiColor(s.rssi),
            modifier = Modifier.fillMaxWidth().height(14.dp),
        )
        Spacer(Modifier.height(12.dp))
        when {
            s.trend > 1.0 -> Text(stringResource(R.string.warmer), fontSize = 22.sp, color = Color(0xFF33DD55))
            s.trend < -1.0 -> Text(stringResource(R.string.colder), fontSize = 22.sp, color = Color(0xFFFF4444))
            else -> Text(stringResource(R.string.steady), fontSize = 22.sp)
        }
        Text(stringResource(R.string.best_dbm, s.peak), color = Color.Gray)
        s.distanceCm?.let { Text(stringResource(R.string.approx_dist, formatDist(it)), color = Color.Gray) }
        Text(stringResource(R.string.readings, "${(s.rate * 10).roundToInt() / 10.0}"), color = Color.Gray, fontSize = 12.sp)
        Spacer(Modifier.height(16.dp))
        Button(onClick = { FinderEngine.uiList() }) { Text(stringResource(R.string.back_to_list)) }
    }
}

private fun formatDist(cm: Int): String = if (cm < 100) "$cm cm" else "${(cm / 10) / 10.0} m"

// ---- Settings ----

@Composable
private fun SettingsScreen(onBack: () -> Unit) {
    val act = LocalContext.current as MainActivity
    val s by SettingsStore.flow.collectAsState()
    fun set(change: (AppSettings) -> AppSettings) {
        SettingsStore.update(act, change)
        act.languageMaybeChanged()
    }

    Column(modifier = Modifier.fillMaxSize().safeDrawingPadding().padding(horizontal = 16.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(vertical = 8.dp)) {
            TextButton(onClick = onBack) { Text("←", fontSize = 22.sp) }
            Text(stringResource(R.string.settings), fontSize = 24.sp, fontWeight = FontWeight.Bold)
        }
        Column(Modifier.verticalScroll(rememberScrollState()).padding(bottom = 24.dp)) {
            Hint(stringResource(R.string.s_app_intro))
            Hint(stringResource(R.string.s_watch_hint))

            Section(stringResource(R.string.s_watch_list))
            Choice(
                stringResource(R.string.s_sort),
                listOf(stringResource(R.string.s_sort_signal), stringResource(R.string.s_sort_name), stringResource(R.string.s_sort_newest)),
                s.sort,
            ) { i -> set { it.copy(sort = i) } }
            val resort = listOf(5, 10, 30, 60)
            Choice(
                stringResource(R.string.s_resort),
                resort.map { stringResource(R.string.s_every, it) },
                resort.indexOf(s.resortSec).coerceAtLeast(0),
            ) { i -> set { it.copy(resortSec = resort[i]) } }
            Hint(stringResource(R.string.s_resort_hint))
            Choice(
                stringResource(R.string.s_start_filter),
                listOf(R.string.f_all, R.string.f_tracker, R.string.f_audio, R.string.f_phone_pc, R.string.f_wearable, R.string.f_other)
                    .map { stringResource(it) },
                s.startFilter,
            ) { i -> set { it.copy(startFilter = i) } }
            Choice(
                stringResource(R.string.s_big),
                listOf(stringResource(R.string.s_big_pct), "dBm"),
                s.bigNumber,
            ) { i -> set { it.copy(bigNumber = i) } }

            Section(stringResource(R.string.s_sound_section))
            Choice(
                stringResource(R.string.s_feedback),
                listOf(R.string.fb_off, R.string.fb_tone, R.string.fb_vibe, R.string.fb_both).map { stringResource(it) },
                s.feedback,
            ) { i -> set { it.copy(feedback = i) } }
            Hint(stringResource(R.string.s_feedback_hint))
            Choice(
                stringResource(R.string.s_sound),
                listOf(stringResource(R.string.s_sound_beep), stringResource(R.string.s_sound_click)),
                s.sound,
            ) { i -> set { it.copy(sound = i) } }
            VolumeSlider(stringResource(R.string.s_volume), s.volume) { v -> set { it.copy(volume = v) } }
            Hint(stringResource(R.string.s_speaker_hint))
            Choice(
                stringResource(R.string.s_vibe),
                listOf(stringResource(R.string.s_vibe_short), stringResource(R.string.s_vibe_strong)),
                s.vibe,
            ) { i -> set { it.copy(vibe = i) } }

            Section(stringResource(R.string.s_light_section))
            Choice(
                stringResource(R.string.s_light_section),
                listOf(R.string.s_light_auto, R.string.s_light_search, R.string.s_light_always).map { stringResource(it) },
                s.light,
            ) { i -> set { it.copy(light = i) } }
            Hint(stringResource(R.string.s_light_hint))

            Section(stringResource(R.string.s_phone_section))
            Toggle(stringResource(R.string.s_classic), s.classic) { v -> set { it.copy(classic = v) } }
            Hint(stringResource(R.string.s_classic_hint))
            val rssi = listOf(-127, -95, -90, -85, -80)
            Choice(
                stringResource(R.string.s_min_rssi),
                rssi.map { if (it == -127) stringResource(R.string.s_off) else stringResource(R.string.s_below, it) },
                rssi.indexOf(s.minRssi).coerceAtLeast(0),
            ) { i -> set { it.copy(minRssi = rssi[i]) } }
            Toggle(stringResource(R.string.s_hide_unnamed), s.hideUnnamed) { v -> set { it.copy(hideUnnamed = v) } }
            Hint(stringResource(R.string.s_hide_unnamed_hint))
            val ages = listOf(30, 60, 120, 300)
            Choice(
                stringResource(R.string.s_max_age),
                ages.map { if (it < 60) "$it s" else "${it / 60} min" },
                ages.indexOf(s.maxAgeSec).coerceAtLeast(0),
            ) { i -> set { it.copy(maxAgeSec = ages[i]) } }

            Section(stringResource(R.string.s_language))
            val langs = listOf("auto") + AppSettings.LANGS
            Choice(
                stringResource(R.string.s_language),
                listOf(stringResource(R.string.s_lang_auto)) + AppSettings.LANG_NAMES,
                langs.indexOf(s.language).coerceAtLeast(0),
            ) { i -> set { it.copy(language = langs[i]) } }
            Hint(stringResource(R.string.s_lang_hint))

            Section(stringResource(R.string.s_support))
            Text(stringResource(R.string.s_support_text), fontSize = 14.sp, modifier = Modifier.padding(bottom = 10.dp))
            Button(
                onClick = { act.openUrl(DONATE_URL) },
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFFFDD00), contentColor = Color.Black),
            ) { Text("☕ Buy me a coffee", fontWeight = FontWeight.SemiBold) }
            Spacer(Modifier.height(20.dp))
            Text(stringResource(R.string.s_version, act.versionName()), color = Color.Gray, fontSize = 12.sp)
        }
    }
}

@Composable
private fun Section(title: String) {
    Text(
        title, fontWeight = FontWeight.Bold, fontSize = 16.sp, color = Color(0xFFFFB347),
        modifier = Modifier.padding(top = 20.dp, bottom = 4.dp),
    )
}

@Composable
private fun Hint(text: String) {
    Text(text, color = Color.Gray, fontSize = 12.sp, modifier = Modifier.padding(bottom = 6.dp))
}

@Composable
private fun Choice(label: String, options: List<String>, selected: Int, onSelect: (Int) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Box {
        Column(Modifier.fillMaxWidth().clickable { open = true }.padding(vertical = 10.dp)) {
            Text(label, fontSize = 15.sp)
            Text(options.getOrElse(selected) { "" }, color = Color(0xFF77AAFF), fontSize = 14.sp)
        }
        DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            options.forEachIndexed { i, o ->
                DropdownMenuItem(
                    text = { Text(if (i == selected) "✓ $o" else o) },
                    onClick = { open = false; onSelect(i) },
                )
            }
        }
    }
}

@Composable
private fun Toggle(label: String, value: Boolean, onChange: (Boolean) -> Unit) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.fillMaxWidth().clickable { onChange(!value) }.padding(vertical = 6.dp),
    ) {
        Text(label, fontSize = 15.sp, modifier = Modifier.weight(1f))
        Switch(checked = value, onCheckedChange = onChange)
    }
}

@Composable
private fun VolumeSlider(label: String, value: Int, onDone: (Int) -> Unit) {
    var v by remember(value) { mutableFloatStateOf(value.toFloat()) }
    Column(Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Text("$label: ${v.roundToInt()} %", fontSize = 15.sp)
        Slider(
            value = v,
            onValueChange = { v = it },
            valueRange = 0f..100f,
            steps = 19,
            onValueChangeFinished = { onDone(v.roundToInt()) },
        )
    }
}
