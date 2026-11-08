#pragma once
#include <pebble.h>

// Languages, same order as the Android app sends them (Settings.kt, WATCH_LANGS)
enum { LANG_EN, LANG_DE, LANG_FR, LANG_IT, LANG_ES, LANG_PT, LANG_NL, LANG_COUNT };

typedef enum {
  S_DEVICE_ONE, S_DEVICES,
  S_F_ALL, S_F_TRACKER, S_F_AUDIO, S_F_PHONE_PC, S_F_WEARABLE, S_F_OTHER,
  S_FB_OFF, S_FB_TONE, S_FB_VIBE, S_FB_BOTH,
  S_PX_0, S_PX_1, S_PX_2, S_PX_3, S_PX_4, S_PX_5,
  S_CAT_0, S_CAT_1, S_CAT_2, S_CAT_3, S_CAT_4, S_CAT_5, S_CAT_6, S_CAT_7, S_CAT_8, S_CAT_9,
  S_CAT_10, S_CAT_11,
  S_STARTING, S_WAKING, S_NO_PHONE, S_NO_PHONE_HINT, S_PHONE_SAYS, S_EMPTY_FILTER, S_EMPTY_FILTER_HINT,
  S_NOTHING, S_KEEP_SEARCHING,
  S_PHONE_SILENT, S_WAIT_PHONE, S_NOT_FOUND, S_NO_SIGNAL_FOR, S_BEST, S_DIST_CM, S_DIST_M, S_RATE,
  S_SCREEN_OFF,
  S_MUTED, S_PEAK_RESET, S_LIGHT_ON, S_LIGHT_AUTO,
  S_AGO, S_BONDED,
  S_COUNT
} StrId;

void i18n_set_lang(int lang);
int i18n_lang(void);
// Language from the watch's own locale, used until the phone sends settings
int i18n_lang_from_locale(void);
const char *tr(StrId id);
