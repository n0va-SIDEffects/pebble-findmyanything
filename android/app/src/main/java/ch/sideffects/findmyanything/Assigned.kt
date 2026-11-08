package ch.sideffects.findmyanything

import android.content.Context

/** Bluetooth SIG assigned numbers (company IDs, 16-bit UUIDs, appearance), loaded from assets. */
object Assigned {
    @Volatile private var companies: Map<Int, String> = emptyMap()
    @Volatile private var uuids: Map<Int, String> = emptyMap()
    @Volatile private var appearances: Map<Int, String> = emptyMap()
    @Volatile private var loaded = false

    fun load(context: Context) {
        if (loaded) return
        synchronized(this) {
            if (loaded) return
            companies = read(context, "companies.tsv")
            uuids = read(context, "uuids16.tsv")
            appearances = read(context, "appearance.tsv")
            loaded = true
        }
    }

    private fun read(context: Context, file: String): Map<Int, String> {
        val map = HashMap<Int, String>()
        try {
            context.assets.open(file).bufferedReader(Charsets.UTF_8).useLines { lines ->
                lines.forEach { line ->
                    val tab = line.indexOf('\t')
                    if (tab > 0) {
                        line.substring(0, tab).toIntOrNull(16)?.let { map[it] = line.substring(tab + 1) }
                    }
                }
            }
        } catch (_: Exception) {
        }
        return map
    }

    fun company(id: Int): String? = companies[id]
    fun uuid16(id: Int): String? = uuids[id]

    /** Appearance name, falling back to the category (upper 10 bits). */
    fun appearance(value: Int): String? = appearances[value] ?: appearances[value and 0xFFC0]

    private val suffix = Regex(
        """[,.]?\s+(Inc\.?|Co\.?|Ltd\.?|LLC|L\.L\.C\.|GmbH|AG|A/S|ASA|S\.A\.|SA|Corporation|Corp\.?|AB|Oy|OY|B\.V\.|BV|d\.o\.o\.|SE|plc|Limited|Pty|\(publ\)|Holdings|International|Technologies|Technology|Electronics|Industries|Semiconductor|Systems|Services)(?=$|[\s,.(]).*$""",
        RegexOption.IGNORE_CASE,
    )

    /** "Samsung Electronics Co. Ltd." -> "Samsung". */
    fun shortCompany(name: String): String {
        var s = name.trim()
        repeat(3) {
            val t = s.replace(suffix, "").trim().trimEnd(',', '.')
            if (t.length >= 2) s = t
        }
        return s
    }
}
