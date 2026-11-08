package ch.sideffects.findmyanything

import android.bluetooth.BluetoothClass
import android.content.res.Resources

/** Parsed BLE advertisement (legacy or extended, incl. scan response). */
class AdInfo {
    var name: String? = null
    var txPower: Int? = null
    var appearance: Int? = null
    val uuids16 = sortedSetOf<Int>()
    val uuidsLong = sortedSetOf<String>()
    val serviceData16 = LinkedHashMap<Int, ByteArray>()
    val manufacturer = LinkedHashMap<Int, ByteArray>()
    var length = 0
}

/** What the app shows about a device. */
data class Classified(
    val name: String?,
    val label: String,
    val detail: String,
    val category: Int,
    /** Expected RSSI at 1 m, if the device tells us its transmit power. */
    val measured1m: Int?,
    /** Stable fingerprint to re-identify a device after its address rotates. */
    val signature: String,
)

object AdParser {
    private const val BASE_SUFFIX = "-0000-1000-8000-00805f9b34fb"

    fun parse(bytes: ByteArray?): AdInfo {
        val info = AdInfo()
        if (bytes == null) return info
        var i = 0
        while (i < bytes.size) {
            val len = bytes[i].toInt() and 0xFF
            if (len == 0 || i + len > bytes.size - 1) break
            val type = bytes[i + 1].toInt() and 0xFF
            val d = bytes.copyOfRange(i + 2, i + 1 + len)
            when (type) {
                0x02, 0x03, 0x14 -> {
                    var j = 0
                    while (j + 1 < d.size) { info.uuids16.add(u16(d, j)); j += 2 }
                }
                0x04, 0x05, 0x1F -> {
                    var j = 0
                    while (j + 3 < d.size) {
                        val v = u16(d, j).toLong() or (u16(d, j + 2).toLong() shl 16)
                        if (v <= 0xFFFF) info.uuids16.add(v.toInt()) else info.uuidsLong.add("%08x".format(v))
                        j += 4
                    }
                }
                0x06, 0x07, 0x15 -> {
                    var j = 0
                    while (j + 15 < d.size) { addUuid128(info, d, j); j += 16 }
                }
                0x08, 0x09 -> {
                    val n = String(d, Charsets.UTF_8).trim { it <= ' ' || it == '\u0000' }
                    if (n.isNotEmpty() && (type == 0x09 || info.name == null)) info.name = n
                }
                0x0A -> if (d.isNotEmpty()) info.txPower = d[0].toInt()
                0x16 -> if (d.size >= 2) info.serviceData16[u16(d, 0)] = d.copyOfRange(2, d.size)
                0x19 -> if (d.size >= 2) info.appearance = u16(d, 0)
                0xFF -> if (d.size >= 2) info.manufacturer[u16(d, 0)] = d.copyOfRange(2, d.size)
            }
            i += len + 1
        }
        info.length = i
        return info
    }

    private fun u16(d: ByteArray, o: Int) = (d[o].toInt() and 0xFF) or ((d[o + 1].toInt() and 0xFF) shl 8)

    private fun addUuid128(info: AdInfo, d: ByteArray, o: Int) {
        val sb = StringBuilder()
        for (k in 15 downTo 0) {
            sb.append("%02x".format(d[o + k].toInt() and 0xFF))
            if (k == 12 || k == 10 || k == 8 || k == 6) sb.append('-')
        }
        val s = sb.toString()
        if (s.endsWith(BASE_SUFFIX) && s.startsWith("0000")) {
            s.substring(4, 8).toIntOrNull(16)?.let { info.uuids16.add(it); return }
        }
        info.uuidsLong.add(s)
    }
}

object Classifier {
    /** Texts in the chosen language; set by the engine. */
    @Volatile var res: Resources? = null

    private fun s(id: Int, vararg args: Any): String = res?.getString(id, *args) ?: ""

    private const val APPLE = 0x004C
    private const val MICROSOFT = 0x0006

    private val companyCategory = mapOf(
        0x0087 to Category.WEARABLE, // Garmin
        0x006B to Category.WEARABLE, // Polar
        0x0157 to Category.WEARABLE, // Huami / Amazfit
        0x02B2 to Category.WEARABLE, 0x0304 to Category.WEARABLE, // Oura
        0x03FF to Category.HEALTH, // Withings
        0x009E to Category.AUDIO, // Bose
        0x0057 to Category.AUDIO, // Harman / JBL
        0x012D to Category.AUDIO, // Sony
        0x00CC to Category.AUDIO, // Beats
        0x0494 to Category.AUDIO, // Sennheiser
        0x0067 to Category.AUDIO, 0x0089 to Category.AUDIO, // GN (Jabra, hearing aids)
        0x07C9 to Category.AUDIO, // Skullcandy
        0x0103 to Category.AUDIO, // Bang & Olufsen
        0x05A7 to Category.AUDIO, // Sonos
        0x0450 to Category.AUDIO, // Teenage Engineering
        0x067C to Category.TRACKER, // Tile
        0x08C3 to Category.TRACKER, // Chipolo
        0x022B to Category.VEHICLE, // Tesla
        0x011F to Category.VEHICLE, // Volkswagen
        0x01DA to Category.INPUT, // Logitech
        0x0499 to Category.SMARTHOME, // Ruuvi
        0x0171 to Category.MEDIA, // Amazon
        0x00C4 to Category.MEDIA, // LG
    )

    /** Manufacturer IDs worth a dedicated screen-off scan filter. */
    val FILTER_COMPANIES = listOf(
        APPLE, MICROSOFT, 0x0075, 0x00E0, 0x018E, 0x0087, 0x006B, 0x0157, 0x02B2, 0x0304, 0x03FF,
        0x009E, 0x0057, 0x012D, 0x00CC, 0x0494, 0x0067, 0x0089, 0x07C9, 0x0103, 0x05A7, 0x067C,
        0x08C3, 0x022B, 0x01DA, 0x0499, 0x0171, 0x00C4, 0x038F, 0x027D, 0x079A, 0x072F, 0x0CCB,
        0x0059, 0x02E5, 0x0008,
    )

    /** 16-bit service data UUIDs worth a dedicated screen-off scan filter. */
    val FILTER_SERVICE_DATA = listOf(
        0xFEAA, 0xFE2C, 0xFD5A, 0xFD59, 0xFD69, 0xFEED, 0xFEEC, 0xFD84, 0xFE33, 0xFE95, 0xFEE0,
        0xFE1F, 0xFEBE, 0xFE07, 0xFE61, 0xFE0F, 0xFE9F, 0xFD44, 0xFCB2, 0xFD6F,
    )

    private val airpodsModels = mapOf(
        0x2002 to "AirPods", 0x200F to "AirPods 2", 0x2013 to "AirPods 3", 0x2019 to "AirPods 4",
        0x201B to "AirPods 4 ANC", 0x200E to "AirPods Pro", 0x2014 to "AirPods Pro 2",
        0x2024 to "AirPods Pro 2", 0x2027 to "AirPods Pro 3", 0x200A to "AirPods Max",
        0x201F to "AirPods Max", 0x2003 to "Powerbeats 3", 0x200B to "Powerbeats Pro",
        0x200C to "Beats Solo Pro", 0x2005 to "BeatsX", 0x2006 to "Beats Solo 3",
        0x2009 to "Beats Studio 3", 0x2010 to "Beats Flex", 0x2011 to "Beats Studio Buds",
        0x2012 to "Beats Fit Pro", 0x2016 to "Beats Studio Buds+", 0x2017 to "Beats Studio Pro",
    )

    private val msDeviceTypes = mapOf(
        1 to (0 to Category.MEDIA), 6 to (0 to Category.PHONE), 7 to (0 to Category.PHONE),
        8 to (R.string.c_android_device to Category.PHONE), 9 to (R.string.c_windows_pc to Category.COMPUTER),
        11 to (0 to Category.PHONE), 12 to (R.string.c_linux_pc to Category.COMPUTER),
        13 to (0 to Category.OTHER), 14 to (0 to Category.COMPUTER),
        15 to (R.string.c_windows_laptop to Category.COMPUTER), 16 to (R.string.c_windows_tablet to Category.COMPUTER),
    )

    private class Acc {
        var label: String? = null
        var detail: String? = null
        var category = Category.OTHER
        var measured1m: Int? = null
        var sigExtra = ""
        fun set(l: String?, d: String?, c: Int) {
            if (label == null) label = l
            if (detail == null) detail = d
            if (category == Category.OTHER) category = c
        }
    }

    fun classifyBle(ad: AdInfo, systemName: String?, bondedName: String?): Classified {
        val a = Acc()
        val name = bondedName ?: ad.name ?: systemName

        ad.manufacturer[APPLE]?.let { apple(it, a) }
        ad.manufacturer[MICROSOFT]?.let { microsoft(it, a) }
        serviceData(ad, a)

        ad.appearance?.let { ap ->
            val c = appearanceCategory(ap shr 6)
            if (c != Category.OTHER) a.set(null, Assigned.appearance(ap), c)
        }
        uuidCategory(ad.uuids16)?.let { (l, c) -> a.set(null, l, c) }

        // Manufacturer, if nothing more specific matched
        val company = ad.manufacturer.keys.firstOrNull()
        if (company != null && company != APPLE && company != MICROSOFT) {
            val cn = Assigned.company(company)?.let { Assigned.shortCompany(it) }
            a.set(cn, cn, companyCategory[company] ?: Category.OTHER)
        }
        // Service UUID owner (e.g. FEED -> Tile)
        if (a.detail == null) {
            ad.uuids16.firstNotNullOfOrNull { u -> if (u >= 0xFC00) Assigned.uuid16(u) else null }
                ?.let { val s = Assigned.shortCompany(it); a.set(s, s, Category.OTHER) }
        }
        if (a.category == Category.OTHER && name != null) a.category = nameCategory(name)

        ad.txPower?.let { if (a.measured1m == null && it in -60..20) a.measured1m = it - 41 }

        val sig = buildString {
            append(a.category).append('|').append(ad.manufacturer.keys.joinToString(","))
            append('|').append(a.sigExtra).append('|').append(ad.uuids16.joinToString(","))
            append('|').append(ad.serviceData16.keys.joinToString(",")).append('|').append(ad.name ?: "")
            append('|').append(ad.length)
        }
        val label = a.label ?: Category.label(a.category)
        return Classified(
            name = name,
            label = label,
            detail = a.detail ?: if (name != null) Category.label(a.category) else "",
            category = a.category,
            measured1m = a.measured1m,
            signature = sig,
        )
    }

    fun classifyClassic(cls: BluetoothClass?, name: String?): Classified {
        val major = cls?.majorDeviceClass ?: BluetoothClass.Device.Major.UNCATEGORIZED
        var cat = when (major) {
            BluetoothClass.Device.Major.COMPUTER -> Category.COMPUTER
            BluetoothClass.Device.Major.PHONE -> Category.PHONE
            BluetoothClass.Device.Major.AUDIO_VIDEO -> {
                val dev = cls?.deviceClass ?: 0
                if (dev == BluetoothClass.Device.AUDIO_VIDEO_VIDEO_DISPLAY_AND_LOUDSPEAKER ||
                    dev == BluetoothClass.Device.AUDIO_VIDEO_VIDEO_MONITOR ||
                    dev == BluetoothClass.Device.AUDIO_VIDEO_SET_TOP_BOX ||
                    dev == BluetoothClass.Device.AUDIO_VIDEO_VCR ||
                    dev == BluetoothClass.Device.AUDIO_VIDEO_VIDEO_CAMERA
                ) Category.MEDIA else Category.AUDIO
            }
            BluetoothClass.Device.Major.PERIPHERAL -> Category.INPUT
            BluetoothClass.Device.Major.WEARABLE -> Category.WEARABLE
            BluetoothClass.Device.Major.HEALTH -> Category.HEALTH
            BluetoothClass.Device.Major.NETWORKING -> Category.SMARTHOME
            else -> Category.OTHER
        }
        if (cat == Category.OTHER && name != null) cat = nameCategory(name)
        return Classified(
            name = name,
            label = Category.label(cat),
            detail = "Classic · " + Category.label(cat),
            category = cat,
            measured1m = null,
            signature = "classic|$major|${name ?: ""}",
        )
    }

    private fun apple(d: ByteArray, a: Acc) {
        // Continuity: [type][len][payload]...
        val types = LinkedHashMap<Int, ByteArray>()
        var j = 0
        while (j + 1 < d.size) {
            val t = d[j].toInt() and 0xFF
            val l = d[j + 1].toInt() and 0xFF
            if (j + 2 + l > d.size) { types[t] = d.copyOfRange(j + 2, d.size); break }
            types[t] = d.copyOfRange(j + 2, j + 2 + l)
            j += 2 + l
        }
        val fm = types[0x12]
        val pp = types[0x07]
        val ib = types[0x02]
        when {
            fm != null && fm.size >= 22 -> {
                val status = fm[0].toInt() and 0xFF
                val kind = when ((status shr 4) and 0x03) {
                    1 -> "AirTag"
                    2 -> s(R.string.c_findmy_acc)
                    3 -> "AirPods"
                    else -> s(R.string.c_apple_device)
                }
                val bat = when ((status shr 6) and 0x03) {
                    0 -> s(R.string.c_bat_full); 1 -> s(R.string.c_bat_medium)
                    2 -> s(R.string.c_bat_low); else -> s(R.string.c_bat_critical)
                }
                a.sigExtra = "fm-sep-${(status shr 4) and 3}"
                a.set(kind, s(R.string.c_separated, bat), Category.TRACKER)
            }
            fm != null -> {
                a.sigExtra = "fm-near"
                a.set(s(R.string.c_findmy_device), s(R.string.c_with_owner), Category.TRACKER)
            }
            pp != null -> {
                val model = if (pp.size >= 3) (pp[1].toInt() and 0xFF) or ((pp[2].toInt() and 0xFF) shl 8) else 0
                a.sigExtra = "pp-%04x".format(model)
                a.set(airpodsModels[model] ?: "AirPods/Beats", s(R.string.c_apple_headphones), Category.AUDIO)
            }
            ib != null && ib.size >= 21 -> {
                val major = ((ib[16].toInt() and 0xFF) shl 8) or (ib[17].toInt() and 0xFF)
                val minor = ((ib[18].toInt() and 0xFF) shl 8) or (ib[19].toInt() and 0xFF)
                a.measured1m = ib[20].toInt()
                a.sigExtra = "ib-$major-$minor"
                a.set("iBeacon", "iBeacon $major/$minor", Category.BEACON)
            }
            types.containsKey(0x09) -> { a.sigExtra = "airplay"; a.set(s(R.string.c_airplay), "Apple TV/HomePod/Mac", Category.MEDIA) }
            types.containsKey(0x0B) -> { a.sigExtra = "watch"; a.set("Apple Watch", "Apple Watch", Category.WEARABLE) }
            types.containsKey(0x0E) -> { a.sigExtra = "hotspot"; a.set("iPhone", s(R.string.c_hotspot), Category.PHONE) }
            types.containsKey(0x10) -> { a.sigExtra = "nearby"; a.set("iPhone/iPad/Mac", s(R.string.c_apple_nearby), Category.PHONE) }
            types.containsKey(0x0F) -> { a.sigExtra = "action"; a.set("iPhone/iPad", "Apple Nearby Action", Category.PHONE) }
            types.containsKey(0x0C) -> { a.sigExtra = "handoff"; a.set("iPhone/iPad/Mac", "Apple Handoff", Category.PHONE) }
            types.containsKey(0x05) -> { a.sigExtra = "airdrop"; a.set("iPhone/iPad/Mac", "Apple AirDrop", Category.PHONE) }
            else -> { a.sigExtra = "apple-" + types.keys.joinToString("-"); a.set(s(R.string.c_apple_device), "Apple", Category.OTHER) }
        }
    }

    private fun microsoft(d: ByteArray, a: Acc) {
        if (d.isEmpty()) return
        when (d[0].toInt() and 0xFF) {
            0x01 -> if (d.size >= 2) {
                val type = d[1].toInt() and 0x1F
                val (labelId, cat) = msDeviceTypes[type] ?: (R.string.c_ms_device to Category.OTHER)
                val label = when (type) {
                    1 -> "Xbox"; 6 -> "iPhone"; 7 -> "iPad"; 11 -> "Windows Phone"; 13 -> "Windows IoT"; 14 -> "Surface Hub"
                    else -> s(labelId)
                }
                a.sigExtra = "cdp-$type"
                a.set(label, "Microsoft · $label", cat)
            }
            0x03 -> {
                a.sigExtra = "swiftpair"
                a.set(null, "Swift Pair, " + s(R.string.c_pairing_mode), Category.OTHER)
            }
        }
    }

    private fun serviceData(ad: AdInfo, a: Acc) {
        val sd = ad.serviceData16
        sd[0xFEAA]?.let { e ->
            val frame = if (e.isNotEmpty()) e[0].toInt() and 0xFF else -1
            when (frame) {
                0x40, 0x41 -> {
                    a.sigExtra = "fmdn"
                    a.set(s(R.string.c_find_hub), if (frame == 0x41) s(R.string.c_find_hub_foreign) else "Google Find Hub", Category.TRACKER)
                }
                0x00, 0x10 -> {
                    if (e.size >= 2) a.measured1m = e[1].toInt() - 41
                    a.set("Eddystone", s(R.string.c_eddystone), Category.BEACON)
                }
                else -> a.set("Eddystone", s(R.string.c_eddystone), Category.BEACON)
            }
        }
        if (sd.containsKey(0xFD5A)) a.set("Galaxy SmartTag", "Samsung SmartTag", Category.TRACKER)
        if (sd.containsKey(0xFD59)) a.set("Galaxy SmartTag", s(R.string.c_smarttag_setup), Category.TRACKER)
        if (sd.containsKey(0xFD69)) a.set(s(R.string.c_samsung_device), s(R.string.c_samsung_offline), Category.OTHER)
        if (sd.containsKey(0xFEED) || sd.containsKey(0xFEEC) || sd.containsKey(0xFD84) ||
            ad.uuids16.contains(0xFEED) || ad.uuids16.contains(0xFEEC) || ad.uuids16.contains(0xFD84)
        ) a.set("Tile", s(R.string.c_tile), Category.TRACKER)
        if (sd.containsKey(0xFE33) || ad.uuids16.contains(0xFE33)) a.set("Chipolo", s(R.string.c_chipolo), Category.TRACKER)
        sd[0xFE2C]?.let { a.set(null, if (it.size == 3) "Fast Pair, " + s(R.string.c_pairing_mode) else "Google Fast Pair", Category.AUDIO) }
        if (sd.containsKey(0xFE95)) a.set(null, "Xiaomi MiBeacon", Category.SMARTHOME)
        if (sd.containsKey(0xFEE0) || ad.uuids16.contains(0xFEE0)) a.set(null, "Amazfit/Mi Band", Category.WEARABLE)
        if (sd.containsKey(0xFE1F) || ad.uuids16.contains(0xFE1F)) a.set(null, "Garmin", Category.WEARABLE)
        if (sd.containsKey(0xFEBE) || ad.uuids16.contains(0xFEBE)) a.set(null, "Bose", Category.AUDIO)
        if (sd.containsKey(0xFE07) || ad.uuids16.contains(0xFE07)) a.set(null, "Sonos", Category.AUDIO)
        if (sd.containsKey(0xFE61) || ad.uuids16.contains(0xFE61)) a.set(null, "Logitech", Category.INPUT)
        if (sd.containsKey(0xFE0F) || ad.uuids16.contains(0xFE0F)) a.set(null, "Philips Hue", Category.SMARTHOME)
        if (sd.containsKey(0xFD6F)) a.set(null, s(R.string.c_contact_tracing), Category.PHONE)
    }

    private fun uuidCategory(u: Set<Int>): Pair<String, Int>? = when {
        u.contains(0x180D) -> s(R.string.c_heart_rate) to Category.HEALTH
        u.contains(0x1812) -> s(R.string.c_input_device) to Category.INPUT
        u.any { it in 0x1843..0x1856 } -> "LE Audio" to Category.AUDIO
        u.contains(0x110B) || u.contains(0x110A) || u.contains(0x111E) -> "Audio" to Category.AUDIO
        u.contains(0x181A) || u.contains(0x1809) -> s(R.string.c_sensor) to Category.SMARTHOME
        u.contains(0x1816) || u.contains(0x1814) || u.contains(0x1826) || u.contains(0x1818) -> s(R.string.c_sport_sensor) to Category.HEALTH
        u.contains(0x1810) || u.contains(0x1808) || u.contains(0x181D) || u.contains(0x1822) -> s(R.string.c_medical) to Category.HEALTH
        else -> null
    }

    private fun appearanceCategory(cat: Int): Int = when (cat) {
        0x001 -> Category.PHONE
        0x002 -> Category.COMPUTER
        0x003, 0x007 -> Category.WEARABLE
        0x008, 0x009 -> Category.TRACKER
        0x005, 0x00A, 0x027, 0x028 -> Category.MEDIA
        0x006, 0x00F, 0x02A -> Category.INPUT
        0x00C, 0x013, 0x014, 0x015, 0x016, 0x017, 0x018, 0x019, 0x01A, 0x01B, 0x01C, 0x01D,
        0x01E, 0x01F, 0x020, 0x024 -> Category.SMARTHOME
        0x00D, 0x00E, 0x010, 0x011, 0x012, 0x029, in 0x031..0x037 -> Category.HEALTH
        0x021, 0x022, 0x025 -> Category.AUDIO
        0x023, 0x033 -> Category.VEHICLE
        else -> Category.OTHER
    }

    private val nameRules: List<Pair<Regex, Int>> = listOf(
        Regex("airpods|buds|headphone|headset|earbud|ear ?\\(|\\bwh-|\\bwf-|jbl|bose|beats|soundcore|earfun|jabra|sennheiser|speaker|soundbar|sonos|marshall|ultimate ears|boom|flip \\d|charge \\d|pods", RegexOption.IGNORE_CASE) to Category.AUDIO,
        Regex("airtag|smarttag|\\btile\\b|chipolo|\\bnut\\b|pebblebee|tracker|\\btag\\b", RegexOption.IGNORE_CASE) to Category.TRACKER,
        Regex("watch|band|\\bfit|amazfit|garmin|forerunner|fenix|venu|polar|whoop|oura|pebble|huawei gt|galaxy fit", RegexOption.IGNORE_CASE) to Category.WEARABLE,
        Regex("\\btv\\b|bravia|fire ?tv|chromecast|roku|appletv|apple tv|projector|beamer|\\[tv\\]", RegexOption.IGNORE_CASE) to Category.MEDIA,
        Regex("keyboard|mouse|\\bmx\\b|k380|controller|xbox|dualsense|dualshock|joy-con|gamepad|pencil|remote", RegexOption.IGNORE_CASE) to Category.INPUT,
        Regex("iphone|ipad|galaxy|pixel|oppo|oneplus|redmi|xiaomi|huawei|honor|nokia|motorola|moto |fairphone|phone|\\bfind x", RegexOption.IGNORE_CASE) to Category.PHONE,
        Regex("macbook|imac|laptop|desktop|thinkpad|surface|notebook|\\bpc\\b|-pc\\b|dell|lenovo|asus|rog", RegexOption.IGNORE_CASE) to Category.COMPUTER,
        Regex("\\bcar\\b|bmw|\\bvw\\b|tesla|audi|mercedes|skoda|seat|toyota|volvo|renault|ford|e-bike|ebike|scooter|vanmoof|cowboy", RegexOption.IGNORE_CASE) to Category.VEHICLE,
        Regex("thermo|hygro|sensor|lywsd|govee|ihoment|h50\\d\\d|switchbot|ruuvi|hue|lamp|light|plug|eve |nanoleaf|aqara|shelly|tado|netatmo", RegexOption.IGNORE_CASE) to Category.SMARTHOME,
        Regex("scale|waage|blood|pressure|glucose|oximeter|thermometer", RegexOption.IGNORE_CASE) to Category.HEALTH,
    )

    fun nameCategory(name: String): Int = nameRules.firstOrNull { it.first.containsMatchIn(name) }?.second ?: Category.OTHER
}
