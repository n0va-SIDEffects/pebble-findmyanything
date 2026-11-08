package ch.sideffects.findmyanything

import android.content.Context
import android.content.res.Configuration
import android.content.res.Resources
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.util.Locale

/** Everything the user can change. Watch-related values are sent to the watch as one blob. */
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
) {
    /** Concrete language code for texts on phone and watch. */
    fun resolvedLanguage(): String {
        if (language != "auto") return language
        val sys = Resources.getSystem().configuration.locales[0]?.language ?: "en"
        return if (sys in LANGS) sys else "en"
    }

    /** 11 bytes, layout in docs/protokoll.md and watch/src/c/main.c (Settings). */
    fun watchBlob(): ByteArray = byteArrayOf(
        1, sort.toByte(), resortSec.toByte(), startFilter.toByte(), feedback.toByte(), volume.toByte(),
        sound.toByte(), light.toByte(), bigNumber.toByte(), LANGS.indexOf(resolvedLanguage()).coerceAtLeast(0).toByte(),
        vibe.toByte(),
    )

    companion object {
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
        )
    }

    fun update(context: Context, change: (AppSettings) -> AppSettings) {
        val s = change(current)
        if (s == current) return
        context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putInt("sort", s.sort).putInt("resortSec", s.resortSec).putInt("startFilter", s.startFilter)
            .putInt("bigNumber", s.bigNumber).putInt("feedback", s.feedback).putInt("sound", s.sound)
            .putInt("volume", s.volume).putInt("vibe", s.vibe).putInt("light", s.light)
            .putBoolean("classic", s.classic).putInt("minRssi", s.minRssi)
            .putBoolean("hideUnnamed", s.hideUnnamed).putInt("maxAgeSec", s.maxAgeSec)
            .putString("language", s.language)
            .apply()
        _flow.value = s
        FinderEngine.settingsChanged()
    }

    /** A context whose resources speak the chosen language. */
    fun localized(base: Context, settings: AppSettings = read(base)): Context {
        val cfg = Configuration(base.resources.configuration)
        cfg.setLocale(Locale.forLanguageTag(settings.resolvedLanguage()))
        return base.createConfigurationContext(cfg)
    }
}
