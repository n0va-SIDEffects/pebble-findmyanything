package ch.sideffects.findmyanything

import java.util.UUID

/**
 * AppMessage contract with the watchapp. Must match watch/package.json (messageKeys)
 * and watch/src/c/protocol.h. Documented in docs/protokoll.md.
 */
object Protocol {
    val WATCHAPP_UUID: UUID = UUID.fromString("cba78834-b48b-444a-b40e-0b10a7cbd608")

    // Watch -> phone
    const val KEY_CMD = 1u
    const val KEY_TARGET = 2u

    const val CMD_LIST = 1
    const val CMD_SEARCH = 2
    const val CMD_STOP = 3

    // Phone -> watch
    const val KEY_TYPE = 10u
    const val KEY_SEQ = 11u
    const val KEY_PART = 12u
    const val KEY_PARTS = 13u
    const val KEY_TOTAL = 14u
    const val KEY_DATA = 15u

    const val KEY_RSSI = 20u
    const val KEY_PEAK = 21u
    const val KEY_AGE = 22u
    const val KEY_DIST = 23u
    const val KEY_TREND = 24u
    const val KEY_NAME = 25u
    const val KEY_DETAIL = 26u
    const val KEY_RAW = 27u
    const val KEY_RATE = 28u
    const val KEY_CATEGORY = 29u

    const val KEY_STATUS = 30u
    const val KEY_STATUS_TEXT = 31u
    const val KEY_SETTINGS = 40u

    const val TYPE_LIST = 1
    const val TYPE_SEARCH = 2
    const val TYPE_STATUS = 3

    // Status bits
    const val ST_BT_OFF = 1 shl 0
    const val ST_NO_PERMISSION = 1 shl 1
    const val ST_SCREEN_OFF = 1 shl 2
    const val ST_CLASSIC_ACTIVE = 1 shl 3
    const val ST_LOCATION_OFF = 1 shl 4
    const val ST_NO_BG_LOCATION = 1 shl 5
    const val ST_SCAN_ERROR = 1 shl 6

    // Entry flags (list)
    const val FL_CLASSIC = 1 shl 0
    const val FL_CONNECTABLE = 1 shl 1
    const val FL_BONDED = 1 shl 2
    const val FL_ROTATING = 1 shl 3

    /** Max bytes of packed device entries per message. */
    const val PART_BYTES = 900
    /** Max devices sent to the watch per list snapshot. */
    const val MAX_LIST = 40
    /** UTF-8 byte limit of name/detail on the watch. */
    const val TEXT_BYTES = 24
}

/** Device categories; the numbers are shared with the watchapp. */
object Category {
    const val OTHER = 0
    const val TRACKER = 1
    const val PHONE = 2
    const val COMPUTER = 3
    const val AUDIO = 4
    const val WEARABLE = 5
    const val MEDIA = 6
    const val BEACON = 7
    const val SMARTHOME = 8
    const val INPUT = 9
    const val VEHICLE = 10
    const val HEALTH = 11

    private val IDS = intArrayOf(
        R.string.cat_0, R.string.cat_1, R.string.cat_2, R.string.cat_3, R.string.cat_4, R.string.cat_5,
        R.string.cat_6, R.string.cat_7, R.string.cat_8, R.string.cat_9, R.string.cat_10, R.string.cat_11,
    )

    /** Category name in the chosen language. */
    fun label(c: Int): String = Classifier.res?.getString(IDS[if (c in IDS.indices) c else 0]) ?: ""
}
