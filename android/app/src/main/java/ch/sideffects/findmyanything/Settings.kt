package ch.sideffects.findmyanything

import android.content.Context
import android.content.res.Configuration
import android.content.res.Resources
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.util.Locale

/**
 * Everything the user can change. All of it travels to the watch as one block
 * (19 bytes, docs/protokoll.md); the watch keeps it and passes changes from the
 * settings page in the Pebble app back here. [stamp] (unix seconds of the last
 * change) decides: the newer one wins.
 */
data class AppSettings(
    // Watch: list
    val sort: Int = 0,            // 0 signal, 1 name, 2 newest
    val resortSec: Int = 10,
    val startFilter: Int = 0,     // 0 all, 1 tracker, 2 audio, 3 phone & pc, 4 wearables, 5 other
    val bigNumber: Int = 0,       // 0 signal 0-100, 1 dBm
    // Watch: sound and vibration
    val feedback: Int = 1,        // bit0 sound, bit1 vibration
    val sound: Int = 0,           // 0 soft beep, 1 Geiger click
    val volume: Int = 30,
    val vibe: Int = 0,            // 0 short, 1 strong
    // Watch: backlight
    val light: Int = 0,           // 0 auto, 1 on during search, 2 always on in the app
    // Phone
    val classic: Boolean = true,
    val minRssi: Int = -127,      // -127 = show all
    val hideUnnamed: Boolean = false,
    val maxAgeSec: Int = 60,
    val language: String = "auto",
    val stamp: Long = 0,
) {
    /** Concrete language code for texts on phone and watch. */
    fun resolvedLanguage(): String {
        if (language != "auto") return language
        val sys = Resources.getSystem().configuration.locales[0]?.language ?: "en"
        return if (sys in LANGS) sys else "en"
    }

    /** 19 bytes, layout in docs/protokoll.md and watch/src/c/main.c (Settings). */
    fun watchBlob(): ByteArray {
        val li = LANGS.indexOf(language).let { if (it < 0) LANG_AUTO else it }
        val st = stamp.coerceIn(0, 0xFFFFFFFFL)
        return byteArrayOf(
            VERSION.toByte(), sort.toByte(), resortSec.toByte(), startFilter.toByte(), feedback.toByte(),
            volume.toByte(), sound.toByte(), light.toByte(), bigNumber.toByte(), li.toByte(), vibe.toByte(),
            (if (classic) 1 else 0).toByte(), minRssi.coerceIn(-127, -40).toByte(), (if (hideUnnamed) 1 else 0).toByte(),
            (maxAgeSec / 10).coerceIn(3, 30).toByte(),
            (st and 0xFF).toByte(), ((st shr 8) and 0xFF).toByte(), ((st shr 16) and 0xFF).toByte(),
            ((st shr 24) and 0xFF).toByte(),
        )
    }

    companion object {
        const val VERSION = 2
        const val BLOB_LEN = 19
        const val LANG_AUTO = 255

        /** Settings block from the watch; null if it is not version 2. */
        fun fromBlob(b: ByteArray): AppSettings? {
            if (b.size < BLOB_LEN || b[0].toInt() != VERSION) return null
            fun u(i: Int) = b[i].toInt() and 0xFF
            val li = u(9)
            return AppSettings(
                sort = u(1).coerceIn(0, 2), resortSec = u(2).coerceIn(2, 120), startFilter = u(3).coerceIn(0, 5),
                feedback = u(4) and 3, volume = u(5).coerceIn(0, 100), sound = u(6).coerceIn(0, 1),
                light = u(7).coerceIn(0, 2), bigNumber = u(8).coerceIn(0, 1),
                language = if (li < LANGS.size) LANGS[li] else "auto", vibe = u(10).coerceIn(0, 1),
                classic = u(11) != 0, minRssi = b[12].toInt().coerceIn(-127, -40), hideUnnamed = u(13) != 0,
                maxAgeSec = u(14).coerceIn(3, 30) * 10,
                stamp = u(15).toLong() or (u(16).toLong() shl 8) or (u(17).toLong() shl 16) or (u(18).toLong() shl 24),
            )
        }

        /** Order matters: the watch gets the index. */
        val LANGS = listOf("en", "de", "fr", "it", "es", "pt", "nl")
        val LANG_NAMES = listOf("English", "Deutsch", "Français", "Italiano", "Español", "Português", "Nederlands")
    }
}

object SettingsStore {
    private const val PREFS = "settings"
    private val _flow = MutableStateFlow(AppSettings())
    val flow: StateFlow<AppSettings> = _flow
    @Volatile private var loaded = false

    val current: AppSettings get() = _flow.value

    fun init(context: Context) {
        if (loaded) return
        synchronized(this) {
            if (loaded) return
            _flow.value = read(context)
            loaded = true
        }
    }

    fun read(context: Context): AppSettings {
        val p = context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val d = AppSettings()
        return AppSettings(
            sort = p.getInt("sort", d.sort),
            resortSec = p.getInt("resortSec", d.resortSec),
            startFilter = p.getInt("startFilter", d.startFilter),
            bigNumber = p.getInt("bigNumber", d.bigNumber),
            feedback = p.getInt("feedback", d.feedback),
            sound = p.getInt("sound", d.sound),
            volume = p.getInt("volume", d.volume),
            vibe = p.getInt("vibe", d.vibe),
            light = p.getInt("light", d.light),
            classic = p.getBoolean("classic", d.classic),
            minRssi = p.getInt("minRssi", d.minRssi),
            hideUnnamed = p.getBoolean("hideUnnamed", d.hideUnnamed),
            maxAgeSec = p.getInt("maxAgeSec", d.maxAgeSec),
            language = p.getString("language", d.language) ?: d.language,
            // 1 for settings from before the stamp: newer than a watch that never had one
            stamp = p.getLong("stamp", 1L),
        )
    }

    private fun save(context: Context, s: AppSettings) {
        context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putInt("sort", s.sort).putInt("resortSec", s.resortSec).putInt("startFilter", s.startFilter)
            .putInt("bigNumber", s.bigNumber).putInt("feedback", s.feedback).putInt("sound", s.sound)
            .putInt("volume", s.volume).putInt("vibe", s.vibe).putInt("light", s.light)
            .putBoolean("classic", s.classic).putInt("minRssi", s.minRssi)
            .putBoolean("hideUnnamed", s.hideUnnamed).putInt("maxAgeSec", s.maxAgeSec)
            .putString("language", s.language).putLong("stamp", s.stamp)
            .apply()
        _flow.value = s
    }

    /**
     * Settings block from the watch (it passes on changes from the settings page
     * in the Pebble app). Newer: take it. Older: the watch gets ours again.
     */
    fun applyFromWatch(context: Context, blob: ByteArray) {
        init(context)
        val w = AppSettings.fromBlob(blob) ?: return
        val cur = current
        when {
            w.stamp > cur.stamp -> {
                save(context, w)
                FinderEngine.settingsChanged(fromWatch = true)
            }
            w.stamp < cur.stamp -> FinderEngine.settingsChanged(fromWatch = false)
        }
    }

    fun update(context: Context, change: (AppSettings) -> AppSettings) {
        val cur = current
        val s = change(cur)
        if (s == cur) return
        // A change must be newer than anything the watch has seen
        save(context, s.copy(stamp = maxOf(System.currentTimeMillis() / 1000, cur.stamp + 1)))
        FinderEngine.settingsChanged(fromWatch = false)
    }

    /** A context whose resources speak the chosen language. */
    fun localized(base: Context, settings: AppSettings = read(base)): Context {
        val cfg = Configuration(base.resources.configuration)
        cfg.setLocale(Locale.forLanguageTag(settings.resolvedLanguage()))
        return base.createConfigurationContext(cfg)
    }
}
