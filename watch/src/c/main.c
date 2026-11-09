// Find My Anything: finds Bluetooth devices by signal strength.
// The phone (Android companion app) scans; the watch shows the list and guides the search.
// Settings live on the watch (flash). The phone app and the settings page in the
// Pebble app both send changes; the newest (stamp) wins and the watch passes it on.
#include <pebble.h>
#include "icons.h"
#include "i18n.h"
#include "beeper.h"

// Colour or black & white (Pebble 2, Pebble 2 Duo): COL(colour, black & white)
#if defined(PBL_COLOR)
#define COL(hex, bw) GColorFromHEX(hex)
#else
#define COL(hex, bw) (bw)
#endif
// Only the Time 2 and the Pebble 2 Duo have a speaker; the others vibrate instead
#if defined(PBL_SPEAKER)
#define HAS_SPEAKER 1
#else
#define HAS_SPEAKER 0
#endif

// 1 = fake data for emulator screenshots, no phone needed
#ifndef FMA_DEMO
#define FMA_DEMO 0
#endif

// ---- Protocol (docs/protokoll.md, Android: Protocol.kt) ----
enum { CMD_LIST = 1, CMD_SEARCH = 2, CMD_STOP = 3 };
enum { TYPE_LIST = 1, TYPE_SEARCH = 2 };
#define ST_BT_OFF         (1 << 0)
#define ST_NO_PERMISSION  (1 << 1)
#define ST_SCREEN_OFF     (1 << 2)
#define ST_CLASSIC_ACTIVE (1 << 3)
#define ST_LOCATION_OFF   (1 << 4)
#define ST_SCAN_ERROR     (1 << 6)
#define FL_BONDED     (1 << 2)

#define MAX_DEVICES 40
#define TEXT_LEN 25
#define KEEPALIVE_MS 60000
#define IDLE_BEFORE_SORT_MS 5000   // no reordering right after a button press or touch
#define SORT_HYSTERESIS 6          // dB a device must be stronger to move up
#define HEADER_H PBL_IF_ROUND_ELSE(32, 24)
#define ROW_H 46

// ---- Settings (19 bytes, version 2, docs/protokoll.md) ----
enum { SORT_SIGNAL, SORT_NAME, SORT_NEWEST };
enum { SOUND_STYLE_BEEP, SOUND_STYLE_CLICK };
enum { LIGHT_AUTO, LIGHT_SEARCH, LIGHT_ALWAYS };
enum { BIG_PERCENT, BIG_DBM };
#define LANG_AUTO 255
// Only single bytes, so the struct has no padding and is the wire format.
typedef struct {
  uint8_t version, sort, resort_sec, start_filter, feedback, volume, sound, light, big_number, lang, vibe;
  uint8_t classic;       // phone: also search classic Bluetooth
  int8_t min_rssi;       // phone: hide weaker signals, -127 = off
  uint8_t hide_unnamed;  // phone: hide devices without a name
  uint8_t max_age_10s;   // phone: keep devices out of reach for n x 10 s
  uint8_t stamp[4];      // unix time of the last change, little endian; the newer one wins
} Settings;
#define SETTINGS_VERSION 2
#define SETTINGS_V1_LEN 11
#define PERSIST_SETTINGS 2
static Settings s_cfg = {SETTINGS_VERSION, SORT_SIGNAL, 10, 0, 1, 30, SOUND_STYLE_BEEP, LIGHT_AUTO, BIG_PERCENT,
                         LANG_AUTO, 0, 1, -127, 0, 6, {0, 0, 0, 0}};
static bool s_settings_out;       // send our settings with the next message
static bool s_last_had_settings;  // the message in flight carries them

typedef struct {
  uint32_t id;
  int8_t rssi;
  uint8_t cat;
  uint8_t flags;
  uint8_t age;
  char name[TEXT_LEN];
  char detail[TEXT_LEN];
} Device;

typedef struct {
  uint32_t id;
  bool have;           // got a reading from the phone
  int rssi, raw, peak;
  int age10;           // tenths of a second since the phone last heard the device
  int dist;            // cm, -1 = unknown
  int trend10;         // dB/s x10, positive = closer
  int rate10;          // readings per second x10
  uint8_t cat;
  char name[41];
  char detail[41];
  int64_t last_msg;    // when the watch last got an update
} Search;

// ---- State ----
static Device s_live[MAX_DEVICES];
static int s_live_count;
static Device s_build[MAX_DEVICES];
static int s_build_count, s_build_seq = -1, s_build_next;
static uint32_t s_order[MAX_DEVICES];  // display order (ids), kept stable between snapshots
static int s_order_count;
static uint8_t s_view[MAX_DEVICES];    // live indices after filter, in display order
static int s_view_count, s_total;
static int64_t s_last_interaction, s_last_sort;
static bool s_programmatic;
static int32_t s_status;
static char s_status_text[64];
static bool s_got_data;
static int64_t s_start_ms;
static int s_filter;
#define FILTER_COUNT 6

// Touch gesture in progress (raw touch events; the list itself scrolls natively)
static struct {
  bool down, drag, in_search;
  int16_t x0, y0, x, y;
  int64_t t0;
} s_touch;
static int64_t s_touch_up_ms;
static bool s_nav_by_touch;         // the list was last moved by the finger, not by buttons
static bool s_commit_pending;       // a complete snapshot waits until the finger is off the list
static bool s_build_ready;
static int s_pending_total;
static uint32_t s_shown_ids[MAX_DEVICES];
static int s_shown_count = -1;

static Search s_search;
static bool s_in_search;
static int s_feedback = 1;  // bit0 = sound, bit1 = vibration
static bool s_light_forced;
static char s_toast[48];
static int64_t s_toast_until;

static int s_current_cmd = CMD_LIST;
static int s_pending_cmd;
static uint32_t s_pending_target;
static int s_last_cmd;
static uint32_t s_last_target;
static bool s_outbox_busy;

static Window *s_list_window, *s_search_window;
static MenuLayer *s_menu;
static Layer *s_empty_layer, *s_search_layer;
static AppTimer *s_retry_timer, *s_keepalive_timer, *s_beep_timer, *s_ui_timer;

// ---- Helpers ----
static int64_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (int64_t)s * 1000 + ms;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int iabs(int v) { return v < 0 ? -v : v; }

// 0..100 from -100..-40 dBm
static int signal_pct(int rssi) { return clampi((rssi + 100) * 100 / 60, 0, 100); }

// Traffic light: green = right next to it, red = far away
static GColor heat_color(int rssi) {
#if defined(PBL_COLOR)
  if (rssi >= -50) return GColorFromHEX(0x00FF00);
  if (rssi >= -60) return GColorFromHEX(0xAAFF00);
  if (rssi >= -70) return GColorFromHEX(0xFFFF00);
  if (rssi >= -80) return GColorFromHEX(0xFFAA00);
  if (rssi >= -90) return GColorFromHEX(0xFF5500);
  return GColorFromHEX(0xFF0000);
#else
  return GColorWhite;  // black & white: the band length, the bars and the word carry it
#endif
}

static const char *proximity(int rssi) {
  if (rssi >= -50) return tr(S_PX_0);
  if (rssi >= -60) return tr(S_PX_1);
  if (rssi >= -70) return tr(S_PX_2);
  if (rssi >= -80) return tr(S_PX_3);
  if (rssi >= -90) return tr(S_PX_4);
  return tr(S_PX_5);
}

static const char *filter_name(int f) { return tr((StrId)(S_F_ALL + clampi(f, 0, FILTER_COUNT - 1))); }
static const char *feedback_name(int f) { return tr((StrId)(S_FB_OFF + (f & 3))); }

// Without a speaker "sound" and "sound + vibration" become vibration
static int fix_feedback(int f) {
  f &= 3;
  if (!HAS_SPEAKER && (f & 1)) f = 2;
  return f;
}

static int bars_for(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -80) return 2;
  if (rssi >= -90) return 1;
  return 0;
}

static void copy_text(char *dst, size_t cap, const uint8_t *src, size_t n) {
  if (n > cap - 1) n = cap - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

static void toast(const char *text) {
  snprintf(s_toast, sizeof(s_toast), "%s", text);
  s_toast_until = now_ms() + 1800;
  if (s_search_layer) layer_mark_dirty(s_search_layer);
}

static void mark_interaction(void) { s_last_interaction = now_ms(); }

// ---- Light ----
static void apply_light(void) {
  bool on = s_light_forced || s_cfg.light == LIGHT_ALWAYS || (s_cfg.light == LIGHT_SEARCH && s_in_search);
  light_enable(on);  // false hands the backlight back to the system
}

// ---- Outbox: one message at a time, latest command wins ----
static void try_send(void);

static void retry_cb(void *ctx) {
  s_retry_timer = NULL;
  try_send();
}

static void schedule_retry(int ms) {
  if (!s_retry_timer) s_retry_timer = app_timer_register(ms, retry_cb, NULL);
}

static void try_send(void) {
  if (FMA_DEMO || (!s_pending_cmd && !s_settings_out) || s_outbox_busy) return;
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) {
    schedule_retry(500);
    return;
  }
  if (s_pending_cmd) {
    dict_write_uint8(it, MESSAGE_KEY_Cmd, (uint8_t)s_pending_cmd);
    if (s_pending_cmd == CMD_SEARCH) dict_write_uint32(it, MESSAGE_KEY_Target, s_pending_target);
  }
  bool with_cfg = s_settings_out;
  if (with_cfg) dict_write_data(it, MESSAGE_KEY_Settings, (const uint8_t *)&s_cfg, sizeof(Settings));
  if (app_message_outbox_send() == APP_MSG_OK) {
    s_outbox_busy = true;
    s_last_cmd = s_pending_cmd;
    s_last_target = s_pending_target;
    s_last_had_settings = with_cfg;
    s_pending_cmd = 0;
    s_settings_out = false;
  } else {
    schedule_retry(500);
  }
}

static void send_cmd(int cmd, uint32_t target) {
  s_pending_cmd = cmd;
  s_pending_target = target;
  try_send();
}

static void outbox_sent(DictionaryIterator *it, void *ctx) {
  s_outbox_busy = false;
  try_send();
}

static void outbox_failed(DictionaryIterator *it, AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "outbox failed %d", (int)reason);
  s_outbox_busy = false;
  if (s_last_had_settings) s_settings_out = true;
  if (!s_pending_cmd) {  // nothing newer queued: repeat the failed one
    s_pending_cmd = s_last_cmd;
    s_pending_target = s_last_target;
  }
  schedule_retry(2000);
}

static void keepalive_cb(void *ctx) {
  s_keepalive_timer = app_timer_register(KEEPALIVE_MS, keepalive_cb, NULL);
  send_cmd(s_current_cmd, s_search.id);
}

// ---- List data ----
static bool filter_match(uint8_t cat) {
  switch (s_filter) {
    case 0: return true;
    case 1: return cat == CAT_TRACKER;
    case 2: return cat == CAT_AUDIO;
    case 3: return cat == CAT_PHONE || cat == CAT_COMPUTER;
    case 4: return cat == CAT_WEARABLE || cat == CAT_HEALTH;
    default:
      return !(cat == CAT_TRACKER || cat == CAT_AUDIO || cat == CAT_PHONE || cat == CAT_COMPUTER ||
               cat == CAT_WEARABLE || cat == CAT_HEALTH);
  }
}

static int live_index(uint32_t id) {
  for (int i = 0; i < s_live_count; i++) {
    if (s_live[i].id == id) return i;
  }
  return -1;
}

static int ci_cmp(const char *a, const char *b) {
  for (;; a++, b++) {
    int ca = (unsigned char)*a, cb = (unsigned char)*b;
    if (ca >= 'A' && ca <= 'Z') ca += 32;
    if (cb >= 'A' && cb <= 'Z') cb += 32;
    if (ca != cb || !ca) return ca - cb;
  }
}

// Should a be listed before b?
static bool before(const Device *a, const Device *b, bool hysteresis) {
  switch (s_cfg.sort) {
    case SORT_NAME: return ci_cmp(a->name, b->name) < 0;
    case SORT_NEWEST: return a->id > b->id;  // the phone numbers devices in the order it finds them
    default: {
      bool sa = a->age > 15, sb = b->age > 15;  // devices out of reach sink to the bottom
      if (sa != sb) return !sa;
      return a->rssi > b->rssi + (hysteresis ? SORT_HYSTERESIS : 0);
    }
  }
}

// Insertion sort of the display order. With hysteresis a device only moves up
// when it is clearly stronger, so the list stays calm.
static void sort_order(bool hysteresis) {
  int idx[MAX_DEVICES];
  int n = 0;
  for (int i = 0; i < s_order_count; i++) {
    int li = live_index(s_order[i]);
    if (li >= 0) idx[n++] = li;
  }
  for (int i = 1; i < n; i++) {
    int cur = idx[i], j = i;
    while (j > 0 && before(&s_live[cur], &s_live[idx[j - 1]], hysteresis)) {
      idx[j] = idx[j - 1];
      j--;
    }
    idx[j] = cur;
  }
  for (int i = 0; i < n; i++) s_order[i] = s_live[idx[i]].id;
  s_order_count = n;
  s_last_sort = now_ms();
}

static void rebuild_view(void) {
  s_view_count = 0;
  for (int i = 0; i < s_order_count; i++) {
    int li = live_index(s_order[i]);
    if (li >= 0 && filter_match(s_live[li].cat)) s_view[s_view_count++] = (uint8_t)li;
  }
}

static uint32_t selected_id(void) {
  if (!s_menu || s_view_count == 0) return 0;
  int row = menu_layer_get_selected_index(s_menu).row;
  if (row < 0 || row >= s_view_count) return 0;
  return s_live[s_view[row]].id;
}

static void refresh_list(uint32_t keep_id) {
  if (!s_menu) return;
  layer_set_hidden(s_empty_layer, s_view_count > 0);
  layer_mark_dirty(s_empty_layer);
  bool same = s_shown_count == s_view_count;
  for (int r = 0; same && r < s_view_count; r++) same = s_shown_ids[r] == s_live[s_view[r]].id;
  if (same) {  // same rows in the same order: just repaint the values
    layer_mark_dirty(menu_layer_get_layer(s_menu));
    return;
  }
  for (int r = 0; r < s_view_count; r++) s_shown_ids[r] = s_live[s_view[r]].id;
  s_shown_count = s_view_count;

  ScrollLayer *sl = menu_layer_get_scroll_layer(s_menu);
  GPoint off = scroll_layer_get_content_offset(sl);
  s_programmatic = true;
  menu_layer_reload_data(s_menu);
  if (keep_id) {
    int cur = menu_layer_get_selected_index(s_menu).row;
    for (int r = 0; r < s_view_count; r++) {
      if (s_shown_ids[r] == keep_id) {
        if (r != cur) menu_layer_set_selected_index(s_menu, (MenuIndex){0, r}, MenuRowAlignNone, false);
        break;
      }
    }
  }
  // After touch scrolling the finger decides what is visible, not the selection
  if (s_nav_by_touch) scroll_layer_set_content_offset(sl, off, false);
  s_programmatic = false;
}

static void resort_now(void) {
  uint32_t keep = selected_id();
  sort_order(false);
  rebuild_view();
  refresh_list(keep);
}

static void parse_entries(const uint8_t *p, uint16_t len) {
  uint16_t i = 0;
  while (i + 9 <= len && s_build_count < MAX_DEVICES) {
    Device *d = &s_build[s_build_count];
    d->id = (uint32_t)p[i] | ((uint32_t)p[i + 1] << 8) | ((uint32_t)p[i + 2] << 16) | ((uint32_t)p[i + 3] << 24);
    d->rssi = (int8_t)p[i + 4];
    d->cat = p[i + 5];
    d->flags = p[i + 6];
    d->age = p[i + 7];
    uint8_t nl = p[i + 8];
    i += 9;
    if (i + nl + 1 > len) break;
    copy_text(d->name, TEXT_LEN, p + i, nl);
    i += nl;
    uint8_t dl = p[i++];
    if (i + dl > len) break;
    copy_text(d->detail, TEXT_LEN, p + i, dl);
    i += dl;
    s_build_count++;
  }
}

static int32_t get_int(DictionaryIterator *it, uint32_t key, int32_t def) {
  Tuple *t = dict_find(it, key);
  if (!t) return def;
  switch (t->length) {
    case 1: return t->type == TUPLE_INT ? t->value->int8 : t->value->uint8;
    case 2: return t->type == TUPLE_INT ? t->value->int16 : t->value->uint16;
    default: return t->value->int32;
  }
}

static void commit_list(int total) {
  uint32_t keep = selected_id();
  bool first = s_order_count == 0;
  memcpy(s_live, s_build, sizeof(Device) * s_build_count);
  s_live_count = s_build_count;
  s_total = total;

  // Keep the order of devices already shown; new ones go to the end
  uint32_t order[MAX_DEVICES];
  int n = 0;
  for (int i = 0; i < s_order_count; i++) {
    if (live_index(s_order[i]) >= 0) order[n++] = s_order[i];
  }
  for (int i = 0; i < s_live_count && n < MAX_DEVICES; i++) {
    bool known = false;
    for (int k = 0; k < n; k++) {
      if (order[k] == s_live[i].id) { known = true; break; }
    }
    if (!known) order[n++] = s_live[i].id;
  }
  memcpy(s_order, order, sizeof(uint32_t) * n);
  s_order_count = n;

  int64_t now = now_ms();
  bool idle = now - s_last_interaction >= IDLE_BEFORE_SORT_MS;
  int every = s_cfg.resort_sec ? s_cfg.resort_sec : 10;
  if (first) sort_order(false);
  else if (idle && s_cfg.sort != SORT_SIGNAL) sort_order(false);
  else if (idle && now - s_last_sort >= every * 1000) sort_order(true);

  rebuild_view();
  refresh_list(keep);
}

static bool list_touch_busy(void) {
  return !s_in_search && s_menu && (s_touch.down || now_ms() - s_touch_up_ms < 1200);
}

static void finish_list(int total) {
  s_build_ready = true;
  if (list_touch_busy()) {
    s_commit_pending = true;
    s_pending_total = total;
    return;
  }
  s_commit_pending = false;
  commit_list(total);
}

#if defined(PBL_TOUCH)  // only a finger on the list holds a snapshot back
static void deferred_commit(void *ctx) {
  if (!s_commit_pending || !s_build_ready) return;
  if (list_touch_busy()) {
    app_timer_register(400, deferred_commit, NULL);
    return;
  }
  s_commit_pending = false;
  commit_list(s_pending_total);
}
#endif

static void handle_list(DictionaryIterator *it) {
  int seq = get_int(it, MESSAGE_KEY_Seq, 0);
  int part = get_int(it, MESSAGE_KEY_Part, 0);
  int parts = get_int(it, MESSAGE_KEY_Parts, 1);
  if (part == 0) {
    s_build_count = 0;
    s_build_seq = seq;
    s_build_next = 0;
    s_build_ready = false;
  }
  if (seq != s_build_seq || part != s_build_next) {  // lost a part: wait for the next snapshot
    s_build_seq = -1;
    return;
  }
  Tuple *data = dict_find(it, MESSAGE_KEY_Data);
  if (data) parse_entries(data->value->data, data->length);
  s_build_next++;
  if (s_build_next >= parts) finish_list(get_int(it, MESSAGE_KEY_Total, s_build_count));
}

static void handle_search(DictionaryIterator *it) {
  if (!s_in_search) return;
  s_search.last_msg = now_ms();
  s_search.rssi = get_int(it, MESSAGE_KEY_Rssi, -127);
  s_search.raw = get_int(it, MESSAGE_KEY_Raw, s_search.rssi);
  s_search.age10 = get_int(it, MESSAGE_KEY_Age, 9999);
  s_search.dist = get_int(it, MESSAGE_KEY_Dist, -1);
  s_search.trend10 = get_int(it, MESSAGE_KEY_Trend, 0);
  s_search.rate10 = get_int(it, MESSAGE_KEY_Rate, 0);
  if (dict_find(it, MESSAGE_KEY_Category)) s_search.cat = (uint8_t)get_int(it, MESSAGE_KEY_Category, 0);
  Tuple *t = dict_find(it, MESSAGE_KEY_Name);
  if (t && t->length > 1) snprintf(s_search.name, sizeof(s_search.name), "%s", t->value->cstring);
  t = dict_find(it, MESSAGE_KEY_Detail);
  if (t) snprintf(s_search.detail, sizeof(s_search.detail), "%s", t->value->cstring);
  s_search.have = s_search.rssi > -127 && s_search.age10 < 9999;
  if (s_search.have && s_search.age10 <= 30 && s_search.rssi > s_search.peak) s_search.peak = s_search.rssi;
  layer_mark_dirty(s_search_layer);
}

static uint32_t stamp_of(const Settings *c) {
  return (uint32_t)c->stamp[0] | ((uint32_t)c->stamp[1] << 8) | ((uint32_t)c->stamp[2] << 16) |
         ((uint32_t)c->stamp[3] << 24);
}

static void apply_lang(void) {
  i18n_set_lang(s_cfg.lang < LANG_COUNT ? s_cfg.lang : i18n_lang_from_locale());
}

static void sound_update(void);

// Settings from the phone (companion app or settings page) or from flash.
// Version 1 (11 bytes) comes from the 1.1 phone app and from flash of 1.1.
static void apply_settings(const uint8_t *p, uint16_t len, bool from_phone) {
  Settings n = s_cfg;
  bool v2 = len >= sizeof(Settings) && p[0] == SETTINGS_VERSION;
  if (v2) {
    memcpy(&n, p, sizeof(Settings));
  } else if (len >= SETTINGS_V1_LEN && p[0] == 1) {
    memcpy(&n, p, SETTINGS_V1_LEN);  // watch part only; phone part and stamp stay
    n.version = SETTINGS_VERSION;
  } else {
    return;
  }
  n.sort = clampi(n.sort, 0, 2);
  n.resort_sec = clampi(n.resort_sec, 2, 120);
  n.start_filter = clampi(n.start_filter, 0, FILTER_COUNT - 1);
  n.feedback &= 3;
  n.volume = clampi(n.volume, 0, 100);
  n.sound = clampi(n.sound, 0, 1);
  n.light = clampi(n.light, 0, 2);
  n.big_number = clampi(n.big_number, 0, 1);
  if (n.lang >= LANG_COUNT) n.lang = LANG_AUTO;
  n.vibe = clampi(n.vibe, 0, 1);
  n.classic = n.classic ? 1 : 0;
  n.min_rssi = clampi(n.min_rssi, -127, -40);
  n.hide_unnamed = n.hide_unnamed ? 1 : 0;
  n.max_age_10s = clampi(n.max_age_10s, 3, 30);

  if (from_phone && v2) {
    uint32_t in = stamp_of(&n), own = stamp_of(&s_cfg);
    if (in < own) {  // the sender is behind: answer with ours
      s_settings_out = true;
      return;
    }
    if (in == own) return;
    s_settings_out = true;  // pass it on to the other side on the phone
  }
  bool changed = memcmp(&n, &s_cfg, sizeof(Settings)) != 0;
  Settings old = s_cfg;
  s_cfg = n;
  apply_lang();
  if (!changed) return;
  if (from_phone) persist_write_data(PERSIST_SETTINGS, &s_cfg, sizeof(s_cfg));
  if (old.volume != n.volume && beeper_running()) beeper_set_volume(n.volume);
  sound_update();
  if (old.start_filter != n.start_filter) s_filter = n.start_filter;
  if (old.feedback != n.feedback) s_feedback = fix_feedback(n.feedback);
  if (old.sort != n.sort && s_order_count) sort_order(false);
  rebuild_view();
  refresh_list(selected_id());
  apply_light();
  if (s_menu) layer_mark_dirty(menu_layer_get_layer(s_menu));
  if (s_search_layer) layer_mark_dirty(s_search_layer);
}

static void inbox_received(DictionaryIterator *it, void *ctx) {
  // The settings page in the Pebble app talks to us too; only the companion app counts as "phone"
  if (dict_find(it, MESSAGE_KEY_Type) || dict_find(it, MESSAGE_KEY_Status)) s_got_data = true;
  Tuple *cfg = dict_find(it, MESSAGE_KEY_Settings);
  if (cfg) apply_settings(cfg->value->data, cfg->length, true);
  if (dict_find(it, MESSAGE_KEY_SettingsReq)) s_settings_out = true;
  if (dict_find(it, MESSAGE_KEY_Status)) s_status = get_int(it, MESSAGE_KEY_Status, 0);
  Tuple *stt = dict_find(it, MESSAGE_KEY_StatusText);
  if (stt) snprintf(s_status_text, sizeof(s_status_text), "%s", stt->value->cstring);
  switch (get_int(it, MESSAGE_KEY_Type, 0)) {
    case TYPE_LIST: handle_list(it); break;
    case TYPE_SEARCH: handle_search(it); break;
    default: break;
  }
  if (s_menu) layer_mark_dirty(menu_layer_get_layer(s_menu));
  if (s_settings_out) try_send();
}

static void inbox_dropped(AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "inbox dropped %d", (int)reason);
}

// ---- List window ----
static uint16_t menu_rows(MenuLayer *ml, uint16_t section, void *ctx) { return s_view_count; }
// Round screens: the focused row in the middle is tall, the others show only the name
static int16_t menu_row_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
#if defined(PBL_ROUND)
  bool big = PBL_DISPLAY_WIDTH > 200;
  return menu_layer_is_index_selected(ml, idx) ? (big ? 64 : 58) : (big ? 40 : 34);
#else
  return ROW_H;
#endif
}
static int16_t menu_header_height(MenuLayer *ml, uint16_t section, void *ctx) { return HEADER_H; }

static void draw_phone_badge(GContext *ctx, GRect r) {
  GColor c = COL(0x55FF55, GColorWhite);
  if (!s_got_data) c = COL(0x555555, GColorWhite);
  else if (s_status & (ST_BT_OFF | ST_NO_PERMISSION | ST_SCAN_ERROR | ST_LOCATION_OFF)) c = COL(0xFF5555, GColorWhite);
  else if (s_status & ST_SCREEN_OFF) c = COL(0xFFAA00, GColorWhite);
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, r, 2, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(r.origin.x + 2, r.origin.y + 2, r.size.w - 4, r.size.h - 6), 0, GCornerNone);
}

static void menu_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  GRect b = layer_get_bounds(cell);
  graphics_context_set_fill_color(ctx, COL(0x000055, GColorBlack));
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  char buf[48];
  if (s_filter == 0) {
    if (s_total == 1) snprintf(buf, sizeof(buf), "%s", tr(S_DEVICE_ONE));
    else snprintf(buf, sizeof(buf), tr(S_DEVICES), s_total);
  } else {
    snprintf(buf, sizeof(buf), "%d/%d · %s", s_view_count, s_total, filter_name(s_filter));
  }
  graphics_context_set_text_color(ctx, GColorWhite);
#if defined(PBL_ROUND)
  int side = b.size.w / 5;  // centred, the corners are outside the circle
  graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(side, 5, b.size.w - 2 * side, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  int bx = b.size.w - side + 4, by = 8;
#else
  graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(8, -1, b.size.w - 36, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  int bx = b.size.w - 17, by = 3;
#endif
  // the classic-scan dot only where there is room for it next to the text
  if ((s_status & ST_CLASSIC_ACTIVE) && PBL_IF_ROUND_ELSE(false, true)) {
    graphics_context_set_fill_color(ctx, COL(0x55AAFF, GColorWhite));
    graphics_fill_circle(ctx, GPoint(bx - 9, by + 9), 3);
  }
  draw_phone_badge(ctx, GRect(bx, by, 9, 17));
#if defined(PBL_BW)
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, GPoint(0, b.size.h - 1), GPoint(b.size.w, b.size.h - 1));
#endif
}

// Four bars; fg is the text colour of the row (used in black & white)
static void draw_bars(GContext *ctx, int x, int y, int rssi, bool stale, GColor fg) {
  int n = bars_for(rssi);
  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 3;
    GRect r = GRect(x + i * 5, y + 13 - h, 3, h);
#if defined(PBL_COLOR)
    GColor on = stale ? GColorFromHEX(0x555555) : heat_color(rssi);
    graphics_context_set_fill_color(ctx, i < n ? on : GColorFromHEX(0x555555));
    graphics_fill_rect(ctx, r, 0, GCornerNone);
#else
    // black & white: a full bar = signal, only its foot = none
    graphics_context_set_fill_color(ctx, fg);
    graphics_fill_rect(ctx, (i < n && !stale) ? r : GRect(r.origin.x, y + 12, 3, 1), 0, GCornerNone);
#endif
  }
}

static void menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  if (idx->row >= s_view_count) return;
  const Device *d = &s_live[s_view[idx->row]];
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  bool stale = d->age > 15;
  GColor bg = hl ? COL(0x0055AA, GColorWhite) : GColorBlack;
  GColor fg = hl ? COL(0xFFFFFF, GColorBlack) : GColorWhite;
  GColor main_c = stale ? COL(0xAAAAAA, fg) : fg;
  GColor dim = hl ? COL(0xAAFFFF, GColorBlack) : COL(0xAAAAAA, GColorWhite);
  GFont f_name = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f_small = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  char det[48];
  if (stale) snprintf(det, sizeof(det), tr(S_AGO), d->age, d->detail);
  else if (d->flags & FL_BONDED) snprintf(det, sizeof(det), tr(S_BONDED), d->detail);
  else snprintf(det, sizeof(det), "%s", d->detail);
  char num[12];
  snprintf(num, sizeof(num), "%d", d->rssi);

#if defined(PBL_ROUND)
  int side = b.size.w / 8;  // the corners of a cell are outside the circle
  if (!hl) {
    graphics_context_set_text_color(ctx, main_c);
    graphics_draw_text(ctx, d->name, f_name, GRect(side + 8, (b.size.h - 24) / 2, b.size.w - 2 * side - 16, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }
  draw_category_icon(ctx, GRect(side - 6, (b.size.h - 20) / 2, 20, 20), d->cat, bg);
  int tx = side + 18, tw = b.size.w - 2 * tx;
  graphics_context_set_text_color(ctx, main_c);
  graphics_draw_text(ctx, d->name, f_name, GRect(tx, 4, tw, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, dim);
  graphics_draw_text(ctx, det, f_small, GRect(tx, 24, tw, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  draw_bars(ctx, b.size.w / 2 - 22, b.size.h - 19, d->rssi, stale, fg);
  graphics_context_set_text_color(ctx, main_c);
  graphics_draw_text(ctx, num, f_small, GRect(b.size.w / 2 + 2, b.size.h - 21, 40, 18), GTextOverflowModeFill,
                     GTextAlignmentLeft, NULL);
#else
  bool small = b.size.w < 180;  // 144 wide: narrower margins and number column
  int ix = small ? 4 : 6, tx = small ? 28 : 32, right = small ? 30 : 50;
  draw_category_icon(ctx, GRect(ix, 13, 20, 20), d->cat, bg);
  graphics_context_set_text_color(ctx, main_c);
  graphics_draw_text(ctx, d->name, f_name, GRect(tx, 1, b.size.w - tx - right, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, dim);
  graphics_draw_text(ctx, det, f_small, GRect(tx, 23, b.size.w - tx - right + (small ? 0 : 8), 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  draw_bars(ctx, b.size.w - (small ? 24 : 26), 6, d->rssi, stale, fg);
  graphics_context_set_text_color(ctx, main_c);
  graphics_draw_text(ctx, num, f_small, GRect(b.size.w - (small ? 34 : 46), 22, small ? 30 : 42, 18),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
#endif
}

static void open_search(const Device *d);

static void menu_select(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  if (idx->row < s_view_count) open_search(&s_live[s_view[idx->row]]);
}

static void next_filter(void) {
  uint32_t keep = selected_id();
  s_filter = (s_filter + 1) % FILTER_COUNT;
  rebuild_view();
  refresh_list(keep);
  if (s_view_count > 0 && !keep) {
    s_programmatic = true;
    menu_layer_set_selected_index(s_menu, (MenuIndex){0, 0}, MenuRowAlignTop, false);
    s_programmatic = false;
  }
  layer_mark_dirty(menu_layer_get_layer(s_menu));
  vibes_short_pulse();
}

static void menu_select_long(MenuLayer *ml, MenuIndex *idx, void *ctx) { next_filter(); }

static void menu_selection_changed(MenuLayer *ml, MenuIndex new_index, MenuIndex old_index, void *ctx) {
  if (s_programmatic) return;
  mark_interaction();
  // A touch tap selects while the finger is still down (the system handles it first)
  if (!s_touch.down && now_ms() - s_touch_up_ms > 400) s_nav_by_touch = false;
}

// Round screens: the text flows along the edge of the circle
static GTextAttributes *text_flow(void) {
#if defined(PBL_ROUND)
  static GTextAttributes *s_flow;
  if (!s_flow) {
    s_flow = graphics_text_attributes_create();
    graphics_text_attributes_enable_screen_text_flow(s_flow, 6);
  }
  return s_flow;
#else
  return NULL;
#endif
}

static void draw_multiline(GContext *ctx, const char *text, GRect r, const char *font, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(font), r, GTextOverflowModeWordWrap,
                     GTextAlignmentCenter, text_flow());
}

static void empty_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  // Time 2 as tested; smaller and round screens get the same picture, smaller
  bool big = b.size.w >= 200;
#if defined(PBL_ROUND)
  int cy = big ? 62 : 44, ty0 = big ? 102 : 72, inset = big ? 30 : 20;
#else
  int cy = big ? 44 : 32, ty0 = big ? 84 : 60, inset = big ? 8 : 4;
#endif
  int step = big ? 12 : 9;
  GPoint c = GPoint(b.size.w / 2, cy);
  GColor amber = COL(0xFFAA00, GColorWhite);
  graphics_context_set_stroke_color(ctx, amber);
  graphics_context_set_fill_color(ctx, amber);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_fill_circle(ctx, c, big ? 5 : 4);
  for (int k = 1; k <= 3; k++) {
    int r = 2 + k * step;  // Time 2: 14, 26, 38
    graphics_draw_arc(ctx, GRect(c.x - r, c.y - r, 2 * r, 2 * r), GOvalScaleModeFitCircle,
                      DEG_TO_TRIGANGLE(-45), DEG_TO_TRIGANGLE(45));
  }
  const char *title, *text;
  char buf[112];
  int64_t waited = now_ms() - s_start_ms;
  if (!s_got_data && waited < 8000) {
    title = tr(S_STARTING);
    text = tr(S_WAKING);
  } else if (!s_got_data) {
    title = tr(S_NO_PHONE);
    text = tr(S_NO_PHONE_HINT);
  } else if (s_status_text[0]) {
    title = tr(S_PHONE_SAYS);
    text = s_status_text;
  } else if (s_live_count > 0) {
    title = tr(S_EMPTY_FILTER);
    snprintf(buf, sizeof(buf), tr(S_EMPTY_FILTER_HINT), filter_name(s_filter));
    text = buf;
  } else {
    title = tr(S_NOTHING);
    text = tr(S_KEEP_SEARCHING);
  }
  // The title may wrap to two lines: measure it and put the text below
  GFont tf = fonts_get_system_font(big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  GRect tr_box = GRect(inset, ty0, b.size.w - 2 * inset, 60);
  GSize ts = graphics_text_layout_get_content_size_with_attributes(title, tf, tr_box, GTextOverflowModeWordWrap,
                                                                   GTextAlignmentCenter, text_flow());
  draw_multiline(ctx, title, tr_box, big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD, GColorWhite);
  int ty = ty0 + ts.h + 4;
  draw_multiline(ctx, text, GRect(inset + 4, ty, b.size.w - 2 * inset - 8, b.size.h - ty),
                 big ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14, COL(0xAAAAAA, GColorWhite));
}

static void list_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  s_menu = menu_layer_create(b);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = menu_rows,
    .get_cell_height = menu_row_height,
    .get_header_height = menu_header_height,
    .draw_header = menu_draw_header,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
    .select_long_click = menu_select_long,
    .selection_changed = menu_selection_changed,
  });
  menu_layer_set_normal_colors(s_menu, GColorBlack, GColorWhite);
  menu_layer_set_highlight_colors(s_menu, COL(0x0055AA, GColorWhite), COL(0xFFFFFF, GColorBlack));
  menu_layer_set_click_config_onto_window(s_menu, w);
  layer_add_child(root, menu_layer_get_layer(s_menu));

  // Round: full screen (the header would sit in the narrow top of the circle)
  s_empty_layer = layer_create(PBL_IF_ROUND_ELSE(b, GRect(0, HEADER_H, b.size.w, b.size.h - HEADER_H)));
  layer_set_update_proc(s_empty_layer, empty_update);
  layer_add_child(root, s_empty_layer);
  layer_set_hidden(s_empty_layer, s_view_count > 0);
}

static void list_unload(Window *w) {
  layer_destroy(s_empty_layer);
  s_empty_layer = NULL;
  menu_layer_destroy(s_menu);
  s_menu = NULL;
}

// ---- Search window ----
#if !defined(PBL_ROUND)
// Progress band along the screen edge, clockwise from top centre. permille 0..1000.
static void border_band(GContext *ctx, GRect b, int th, int permille, GColor color) {
  int w = b.size.w, h = b.size.h, x0 = b.origin.x, y0 = b.origin.y;
  int len = (2 * w + 2 * h) * clampi(permille, 0, 1000) / 1000;
  graphics_context_set_fill_color(ctx, color);
  int s = len < w / 2 ? len : w / 2;
  if (s > 0) graphics_fill_rect(ctx, GRect(x0 + w / 2, y0, s, th), 0, GCornerNone);
  len -= s; if (len <= 0) return;
  s = len < h ? len : h;
  graphics_fill_rect(ctx, GRect(x0 + w - th, y0, th, s), 0, GCornerNone);
  len -= s; if (len <= 0) return;
  s = len < w ? len : w;
  graphics_fill_rect(ctx, GRect(x0 + w - s, y0 + h - th, s, th), 0, GCornerNone);
  len -= s; if (len <= 0) return;
  s = len < h ? len : h;
  graphics_fill_rect(ctx, GRect(x0, y0 + h - s, th, s), 0, GCornerNone);
  len -= s; if (len <= 0) return;
  s = len < w / 2 ? len : w / 2;
  graphics_fill_rect(ctx, GRect(x0, y0, s, th), 0, GCornerNone);
}

// Small marker across the band at a position (best value)
static void border_marker(GContext *ctx, GRect b, int th, int permille, GColor color) {
  int w = b.size.w, h = b.size.h, x0 = b.origin.x, y0 = b.origin.y;
  int pos = (2 * w + 2 * h) * clampi(permille, 0, 1000) / 1000;
  GRect m;
  if (pos <= w / 2) m = GRect(x0 + w / 2 + pos - 2, y0, 4, th + 4);
  else if ((pos -= w / 2) <= h) m = GRect(x0 + w - th - 4, y0 + pos - 2, th + 4, 4);
  else if ((pos -= h) <= w) m = GRect(x0 + w - pos - 2, y0 + h - th - 4, 4, th + 4);
  else if ((pos -= w) <= h) m = GRect(x0, y0 + h - pos - 2, th + 4, 4);
  else { pos -= h; m = GRect(x0 + pos - 2, y0, 4, th + 4); }
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, m, 0, GCornerNone);
}
#endif

// Edge band: along the frame on rectangular screens, a ring on round ones
static void edge_band(GContext *ctx, GRect b, int th, int permille, GColor color) {
#if defined(PBL_ROUND)
  int32_t end = TRIG_MAX_ANGLE * clampi(permille, 0, 1000) / 1000;
  if (end <= 0) return;
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, th, 0, end);
#else
  border_band(ctx, b, th, permille, color);
#endif
}

static void edge_marker(GContext *ctx, GRect b, int th, int permille, GColor color) {
#if defined(PBL_ROUND)
  int32_t half = TRIG_MAX_ANGLE / 120;  // 3 degrees wide
  int32_t a = TRIG_MAX_ANGLE * clampi(permille, 0, 1000) / 1000;
  if (a < half) a = half;
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, th + 4, a - half, a + half);
#else
  border_marker(ctx, b, th, permille, color);
#endif
}

// The empty track: dark blue in colour, a thin line in black & white
static void edge_track(GContext *ctx, GRect b, int th) {
#if defined(PBL_COLOR)
  edge_band(ctx, b, th, 1000, GColorFromHEX(0x000055));
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, grect_inset(b, GEdgeInsets(th / 2)));
#endif
}

static void draw_trend(GContext *ctx, GPoint c, int trend10) {
  if (trend10 > 10) {
    GPathInfo info = {3, (GPoint[]){{c.x - 9, c.y + 6}, {c.x + 9, c.y + 6}, {c.x, c.y - 8}}};
    GPath *p = gpath_create(&info);
    graphics_context_set_fill_color(ctx, COL(0x00FF00, GColorWhite));
    gpath_draw_filled(ctx, p);
    gpath_destroy(p);
  } else if (trend10 < -10) {
    GPathInfo info = {3, (GPoint[]){{c.x - 9, c.y - 6}, {c.x + 9, c.y - 6}, {c.x, c.y + 8}}};
    GPath *p = gpath_create(&info);
    graphics_context_set_fill_color(ctx, COL(0xFF0000, GColorWhite));
    gpath_draw_filled(ctx, p);
    gpath_destroy(p);
  }
}

// Big value: 0..100 or dBm. LECO has no minus glyph, so the minus is drawn.
static void draw_big_value(GContext *ctx, GRect b, int y, bool lost, GColor color) {
  GFont font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  char num[12];
  bool dbm = s_cfg.big_number == BIG_DBM;
  if (lost) snprintf(num, sizeof(num), "--");
  else snprintf(num, sizeof(num), "%d", dbm ? iabs(s_search.rssi) : signal_pct(s_search.rssi));
  GRect box = GRect(0, y, b.size.w, 50);
  GSize sz = graphics_text_layout_get_content_size(num, font, box, GTextOverflowModeFill, GTextAlignmentLeft);
  int minus_w = (dbm && !lost) ? 16 : 0;
  int x = (b.size.w - sz.w - minus_w - (minus_w ? 4 : 0)) / 2;
  graphics_context_set_text_color(ctx, color);
  if (minus_w) {
    graphics_context_set_fill_color(ctx, color);
    graphics_fill_rect(ctx, GRect(x, y + 24, minus_w, 5), 0, GCornerNone);
    x += minus_w + 4;
  }
  graphics_draw_text(ctx, num, font, GRect(x, y, sz.w + 4, 50), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

// Positions per screen. The Time 2 layout is the one tested on the watch; the
// others keep its order (header, big number, word, details, mode) in less room.
typedef struct {
  int th;                           // edge band
  GRect icon, name, detail;
  GTextAlignment head_align;
  int num_y;                        // top of the big number
  GPoint trend;
  GRect word, info, info2, bottom;  // info2 with height 0: not shown
  int tap_top, tap_mid;             // touch zones: light | best value | signal mode
} SearchLayout;

static SearchLayout search_layout(GRect b) {
  int w = b.size.w, h = b.size.h;
#if defined(PBL_ROUND)
  (void)h;
  if (w > 200) {  // Round 2, 260 x 260
    return (SearchLayout){10, GRect(w / 2 - 10, 20, 20, 20), GRect(40, 40, w - 80, 22), GRect(35, 60, w - 70, 18),
                          GTextAlignmentCenter, 80, GPoint(w - 52, 106), GRect(30, 126, w - 60, 30),
                          GRect(35, 156, w - 70, 22), GRect(45, 178, w - 90, 18), GRect(65, 204, w - 130, 18),
                          80, 178};
  }
  // Time Round, 180 x 180: no room for the second detail line
  return (SearchLayout){8, GRect(w / 2 - 10, 13, 20, 20), GRect(28, 31, w - 56, 22), GRect(22, 50, w - 44, 18),
                        GTextAlignmentCenter, 58, GPoint(w - 32, 84), GRect(18, 100, w - 36, 30),
                        GRect(26, 126, w - 52, 22), GRect(0, 0, 0, 0), GRect(42, 148, w - 84, 18), 58, 126};
#else
  if (h >= 200) {  // Time 2, 200 x 228
    return (SearchLayout){8, GRect(16, 16, 20, 20), GRect(42, 10, w - 56, 22), GRect(42, 30, w - 56, 18),
                          GTextAlignmentLeft, 56, GPoint(w - 30, 84), GRect(14, 108, w - 28, 30),
                          GRect(14, 140, w - 28, 22), GRect(14, 162, w - 28, 18), GRect(14, h - 36, w - 28, 18),
                          56, 150};
  }
  // 144 x 168: Time, Time Steel, Pebble 2, Pebble 2 Duo
  return (SearchLayout){6, GRect(10, 10, 20, 20), GRect(34, 4, w - 42, 22), GRect(34, 22, w - 42, 18),
                        GTextAlignmentLeft, 34, GPoint(w - 18, 60), GRect(8, 76, w - 16, 30),
                        GRect(8, 100, w - 16, 22), GRect(8, 119, w - 16, 18), GRect(8, h - 26, w - 16, 18), 34, 110};
#endif
}

static void search_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  SearchLayout L = search_layout(b);
  int64_t now = now_ms();
  bool no_phone = now - s_search.last_msg > 6000;
  bool lost = !s_search.have || s_search.age10 > 100 || no_phone;
  GColor grey = COL(0xAAAAAA, GColorWhite);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  GColor heat = lost ? COL(0x555555, GColorWhite) : heat_color(s_search.rssi);
  edge_track(ctx, b, L.th);
  if (!lost) edge_band(ctx, b, L.th, signal_pct(s_search.rssi) * 10, heat);
  if (s_search.peak > -127) edge_marker(ctx, b, L.th, signal_pct(s_search.peak) * 10, GColorWhite);

  // Header: icon, name, detail
  draw_category_icon(ctx, L.icon, s_search.cat, GColorBlack);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_search.name, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), L.name,
                     GTextOverflowModeTrailingEllipsis, L.head_align, NULL);
  graphics_context_set_text_color(ctx, grey);
  graphics_draw_text(ctx, s_search.detail, fonts_get_system_font(FONT_KEY_GOTHIC_14), L.detail,
                     GTextOverflowModeTrailingEllipsis, L.head_align, NULL);

  draw_big_value(ctx, b, L.num_y, lost, lost ? grey : heat);
  if (!lost) draw_trend(ctx, L.trend, s_search.trend10);

  char line[48];
  const char *word;
  if (no_phone && s_search.last_msg) word = tr(S_PHONE_SILENT);
  else if (!s_search.last_msg) word = tr(S_WAIT_PHONE);
  else if (!s_search.have) word = tr(S_NOT_FOUND);
  else if (lost) {
    snprintf(line, sizeof(line), tr(S_NO_SIGNAL_FOR), s_search.age10 / 10);
    word = line;
  } else word = proximity(s_search.rssi);
  graphics_context_set_text_color(ctx, lost ? grey : heat);
  graphics_draw_text(ctx, word, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), L.word,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Details: the other unit, best value, distance, rate
  char info[64], best[32];
  info[0] = '\0';
  if (s_search.have) {
    bool dbm = s_cfg.big_number == BIG_DBM;
    if (s_search.peak > -127) snprintf(best, sizeof(best), tr(S_BEST), dbm ? s_search.peak : signal_pct(s_search.peak));
    else best[0] = '\0';
    if (dbm) snprintf(info, sizeof(info), "%d %%%s%s", signal_pct(s_search.rssi), best[0] ? " · " : "", best);
    else snprintf(info, sizeof(info), "%d dBm%s%s", s_search.rssi, best[0] ? " · " : "", best);
  }
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, info, fonts_get_system_font(FONT_KEY_GOTHIC_18), L.info,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  if (L.info2.size.h) {
    char info2[72], rate[32];
    info2[0] = '\0';
    if (s_search.have && s_search.dist > 0) {
      if (s_search.dist < 100) snprintf(info2, sizeof(info2), tr(S_DIST_CM), s_search.dist);
      else snprintf(info2, sizeof(info2), tr(S_DIST_M), s_search.dist / 100, (s_search.dist % 100) / 10);
      strncat(info2, " · ", sizeof(info2) - strlen(info2) - 1);
    }
    snprintf(rate, sizeof(rate), tr(S_RATE), s_search.rate10 / 10, s_search.rate10 % 10);
    strncat(info2, rate, sizeof(info2) - strlen(info2) - 1);
    graphics_context_set_text_color(ctx, grey);
    graphics_draw_text(ctx, info2, fonts_get_system_font(FONT_KEY_GOTHIC_14), L.info2,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // Bottom line: toast, phone hint or signal mode
  const char *bottom;
  GColor bc = grey;
  if (now < s_toast_until) { bottom = s_toast; bc = GColorWhite; }
  else if (s_status_text[0]) { bottom = s_status_text; bc = COL(0xFF5555, GColorWhite); }
  else if ((s_status & ST_SCREEN_OFF) && s_search.rate10 < 15) { bottom = tr(S_SCREEN_OFF); bc = COL(0xFFAA00, GColorWhite); }
  else bottom = feedback_name(s_feedback);
  graphics_context_set_text_color(ctx, bc);
  graphics_draw_text(ctx, bottom, fonts_get_system_font(FONT_KEY_GOTHIC_14), L.bottom,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// ---- Sound and vibration: faster and higher when closer ----
// One stream stays open for the whole search and the beeps are synthesised
// into it (beeper.c). Starting the speaker for every beep restarts the audio
// path of the Time 2 each time, and that crackled.
static uint16_t blip_freq(int t /* 0..1000 */) { return (uint16_t)(900 + 900 * t / 1000); }

static void sound_update(void) {
  static bool s_logged;
  bool want = s_in_search && (s_feedback & 1) && s_cfg.volume > 0;
  if (want && !beeper_running()) {
    if (!beeper_start(s_cfg.volume) && !s_logged) {
      s_logged = true;
      APP_LOG(APP_LOG_LEVEL_WARNING, "speaker stream unavailable");
    }
  } else if (!want && beeper_running()) {
    beeper_stop();
  }
}

static void play_blip(int t /* 0..1000, 1000 = very close */) {
  if (s_cfg.volume == 0) return;
  if (!beeper_running()) sound_update();  // the speaker may have been busy at the start
  if (beeper_running()) {
    beeper_trigger(s_cfg.sound == SOUND_STYLE_CLICK ? BEEPER_CLICK : BEEPER_BEEP, blip_freq(t));
    return;
  }
#if defined(PBL_SPEAKER)
  // Fallback if the stream could not be opened (speaker busy)
  speaker_play_tone(blip_freq(t), 40, s_cfg.volume / 2, SpeakerWaveformSine);
#endif
}

static void vibe_now(void *ctx) {
  static uint32_t seg[1];
  seg[0] = s_cfg.vibe ? 80 : 35;
  VibePattern pat = {.durations = seg, .num_segments = 1};
  vibes_enqueue_custom_pattern(pat);
}

static uint32_t beep_interval(int rssi) {
  int t = clampi((rssi + 95) * 1000 / 50, 0, 1000);
  int inv = 1000 - t;
  return 140 + (uint32_t)(1860 * inv / 1000) * inv / 1000;
}

static void beep_cb(void *ctx) {
  s_beep_timer = NULL;
  if (!s_in_search) return;
  uint32_t next = 1000;
  int64_t now = now_ms();
  bool lost = !s_search.have || s_search.age10 > 50 || now - s_search.last_msg > 6000;
  // demo: no sound or vibration (rapid vibration floods the emulator, screenshots fail)
  if (!lost && s_feedback && !FMA_DEMO) {
    next = beep_interval(s_search.rssi);
    int t = clampi((s_search.rssi + 95) * 1000 / 50, 0, 1000);
    if (s_feedback & 1) play_blip(t);
    if (s_feedback & 2) {
      // The beep is heard after the stream's lead time; vibrate together with it
      if ((s_feedback & 1) && beeper_running()) app_timer_register(beeper_latency_ms(), vibe_now, NULL);
      else vibe_now(NULL);
    }
  }
  s_beep_timer = app_timer_register(next, beep_cb, NULL);
}

// Repaint once a second (age counters, lost state)
static void ui_tick(void *ctx) {
  s_ui_timer = app_timer_register(1000, ui_tick, NULL);
  if (s_in_search && s_search_layer) {
    if (FMA_DEMO != 2) layer_mark_dirty(s_search_layer);
  } else if (s_empty_layer && !layer_get_hidden(s_empty_layer)) layer_mark_dirty(s_empty_layer);
}

// ---- Search actions (buttons and touch) ----
static void cycle_feedback(void) {
  // without a speaker: silent <-> vibration
  s_feedback = HAS_SPEAKER ? (s_feedback + 1) & 3 : (s_feedback ? 0 : 2);
  sound_update();
  if ((s_feedback & 1) && speaker_is_muted()) toast(tr(S_MUTED));
  else toast(feedback_name(s_feedback));
}

static void reset_peak(void) {
  s_search.peak = s_search.have ? s_search.rssi : -127;
  toast(tr(S_PEAK_RESET));
}

static void toggle_light(void) {
  s_light_forced = !s_light_forced;
  apply_light();
  toast(s_light_forced ? tr(S_LIGHT_ON) : tr(S_LIGHT_AUTO));
}

static void search_select(ClickRecognizerRef r, void *ctx) { cycle_feedback(); }
static void search_up(ClickRecognizerRef r, void *ctx) { reset_peak(); }
static void search_down(ClickRecognizerRef r, void *ctx) { toggle_light(); }

static void search_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, search_select);
  window_single_click_subscribe(BUTTON_ID_UP, search_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, search_down);
}

static void search_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_search_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_search_layer, search_update);
  layer_add_child(root, s_search_layer);
}

static void search_unload(Window *w) {
  s_in_search = false;
  sound_update();  // close the stream before anything else goes out
  s_light_forced = false;
  apply_light();
  if (s_beep_timer) { app_timer_cancel(s_beep_timer); s_beep_timer = NULL; }
  layer_destroy(s_search_layer);
  s_search_layer = NULL;
  window_destroy(s_search_window);
  s_search_window = NULL;
  s_current_cmd = CMD_LIST;
  send_cmd(CMD_LIST, 0);
  s_last_interaction = 0;
  if (s_menu) resort_now();
}

static void open_search(const Device *d) {
  memset(&s_search, 0, sizeof(s_search));
  s_search.id = d->id;
  s_search.cat = d->cat;
  s_search.rssi = d->rssi;
  s_search.peak = -127;
  s_search.dist = -1;
  snprintf(s_search.name, sizeof(s_search.name), "%s", d->name);
  snprintf(s_search.detail, sizeof(s_search.detail), "%s", d->detail);
  s_in_search = true;
  s_current_cmd = CMD_SEARCH;
  send_cmd(CMD_SEARCH, d->id);

  s_search_window = window_create();
  window_set_background_color(s_search_window, GColorBlack);
  window_set_click_config_provider(s_search_window, search_clicks);
  window_set_window_handlers(s_search_window, (WindowHandlers){.load = search_load, .unload = search_unload});
  window_stack_push(s_search_window, true);
  apply_light();
  sound_update();
  s_beep_timer = app_timer_register(500, beep_cb, NULL);
}

// ---- Touch (Pebble Time 2, Pebble Round 2) ----
#if defined(PBL_TOUCH)
// The list is a MenuLayer: with app_touch_navigation_enable() the system scrolls
// it 1:1 under the finger (with coasting and a scrollbar), and a tap on a row
// opens it. Own handling only where the system has nothing: the search screen
// (custom layer) and a tap on the list header. Raw touch events turn the system's
// button emulation off, the native list scrolling stays.
static void list_header_tap(int16_t x, int16_t y) {
  if (!s_menu || window_stack_get_top_window() != s_list_window) return;
  GPoint off = scroll_layer_get_content_offset(menu_layer_get_scroll_layer(s_menu));
  if (y - off.y < HEADER_H) {  // header: sort now
    resort_now();
    vibes_short_pulse();
  }
}

static void search_tap(int16_t x, int16_t y) {
  if (!s_search_layer) return;
  SearchLayout L = search_layout(layer_get_bounds(s_search_layer));
  if (y < L.tap_top) toggle_light();
  else if (y < L.tap_mid) reset_peak();
  else cycle_feedback();
}

static void touch_handler(const TouchEvent *e, void *ctx) {
  switch (e->type) {
    case TouchEvent_Touchdown:
      s_touch.down = !e->non_navigational;
      s_touch.drag = false;
      s_touch.in_search = s_in_search;
      s_touch.x0 = s_touch.x = e->x;
      s_touch.y0 = s_touch.y = e->y;
      s_touch.t0 = now_ms();
      if (s_touch.down && !s_in_search) s_nav_by_touch = true;
      mark_interaction();
      break;
    case TouchEvent_PositionUpdate:
      if (!s_touch.down) break;
      s_touch.x = e->x;
      s_touch.y = e->y;
      if (iabs(s_touch.x - s_touch.x0) > 12 || iabs(s_touch.y - s_touch.y0) > 12) s_touch.drag = true;
      mark_interaction();
      break;
    case TouchEvent_Liftoff: {
      if (!s_touch.down) break;
      s_touch.down = false;
      s_touch_up_ms = now_ms();
      // A tap on a list row has just opened the search (the system went first): ignore it here
      bool same_screen = s_touch.in_search == s_in_search;
      int dx = s_touch.x - s_touch.x0, dy = s_touch.y - s_touch.y0;
      if (same_screen && !s_touch.drag && s_touch_up_ms - s_touch.t0 < 600) {
        if (s_in_search) search_tap(s_touch.x0, s_touch.y0);
        else list_header_tap(s_touch.x0, s_touch.y0);
      } else if (same_screen && s_in_search && dx > 60 && iabs(dy) < 50) {
        window_stack_pop(true);  // swipe right: back to the list
      }
      mark_interaction();
      if (s_commit_pending) app_timer_register(1250, deferred_commit, NULL);
      break;
    }
    default:
      break;
  }
}

#endif  // PBL_TOUCH

// ---- Demo data for the emulator ----
#if FMA_DEMO
static void demo_fill(void) {
  static const struct { const char *n, *d; uint8_t c; int8_t r; uint8_t age; uint8_t fl; } demo[] = {
    {"AirTag", "Find My · separated", CAT_TRACKER, -54, 0, 0},
    {"Mike's Buds", "Google Fast Pair", CAT_AUDIO, -61, 0, FL_BONDED},
    {"iPhone/iPad/Mac", "Apple nearby", CAT_PHONE, -66, 0, 0},
    {"Pebble Time 2", "Wearable", CAT_WEARABLE, -68, 0, FL_BONDED},
    {"Galaxy SmartTag", "Samsung SmartTag", CAT_TRACKER, -72, 1, 0},
    {"[TV] Samsung 7", "TV/media", CAT_MEDIA, -75, 0, 0},
    {"LYWSD03MMC", "Xiaomi MiBeacon", CAT_SMARTHOME, -79, 2, 0},
    {"Windows laptop", "Microsoft", CAT_COMPUTER, -83, 0, 0},
    {"MX Keys", "Logitech", CAT_INPUT, -86, 3, 0},
    {"Tile", "Tile tracker", CAT_TRACKER, -91, 4, 0},
    {"VW ID.3", "Vehicle", CAT_VEHICLE, -93, 20, 0},
  };
  s_build_count = ARRAY_LENGTH(demo);
  for (int i = 0; i < s_build_count; i++) {
    Device *d = &s_build[i];
    d->id = i + 1; d->rssi = demo[i].r; d->cat = demo[i].c; d->flags = demo[i].fl; d->age = demo[i].age;
    snprintf(d->name, TEXT_LEN, "%s", demo[i].n);
    snprintf(d->detail, TEXT_LEN, "%s", demo[i].d);
  }
  s_got_data = true;
  s_status = ST_CLASSIC_ACTIVE;
  commit_list(s_build_count + 3);
}

// FMA_DEMO 2: list, AirTag (near), Galaxy SmartTag (medium), Tile (far), filter, 25 s each
#define DEMO_STEP_MS 25000
static void demo_open(uint32_t id) {
  if (s_in_search) window_stack_pop(false);
  int i = live_index(id);
  if (i >= 0) open_search(&s_live[i]);
}

static void demo_tour(void *ctx) {
  static int step;
  step++;
  if (step == 1) demo_open(1);
  else if (step == 2) demo_open(5);
  else if (step == 3) demo_open(10);
  else if (step == 4) {
    if (s_in_search) window_stack_pop(false);
    next_filter();
  }
  if (step < 4) app_timer_register(DEMO_STEP_MS, demo_tour, NULL);
}

static void demo_tick(void *ctx) {
  // the tour redraws rarely: in the slow emulator every redraw of the search
  // screen takes ~0.5 s and screenshots time out if it never rests
  app_timer_register(FMA_DEMO == 2 ? 4000 : 400, demo_tick, NULL);
  if (!s_in_search) return;
  static int phase;
  phase++;
  // the chosen demo device sets the scene: AirTag near and green, Tile far and red
  int base = -58;
  for (int i = 0; i < s_live_count; i++) {
    if (s_live[i].id == s_search.id) base = s_live[i].rssi;
  }
  int dist = 50;  // cm, about x1.12 per dB below -54
  for (int k = -54; k > base; k--) dist = dist * 112 / 100;
  s_search.last_msg = now_ms();
  s_search.have = true;
  s_search.age10 = 1;
  s_search.rssi = base + (phase % 5) - 2;
  s_search.trend10 = base < -85 ? -14 : 14;  // far: moving away, otherwise getting closer
  s_search.rate10 = base > -80 ? 72 : 31;
  s_search.dist = dist;
  if (s_search.rssi + 2 > s_search.peak) s_search.peak = s_search.rssi + 2;
  if (s_search_layer) layer_mark_dirty(s_search_layer);
}
#endif

// ---- App ----
static void init(void) {
  s_start_ms = now_ms();
  if (persist_exists(PERSIST_SETTINGS)) {
    uint8_t stored[32];
    int n = persist_read_data(PERSIST_SETTINGS, stored, sizeof(stored));
    if (n > 0) apply_settings(stored, (uint16_t)n, false);
  }
  apply_lang();
  s_filter = s_cfg.start_filter;
  s_feedback = fix_feedback(s_cfg.feedback);

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_sent(outbox_sent);
  app_message_register_outbox_failed(outbox_failed);
  uint32_t in_size = app_message_inbox_size_maximum();
  if (in_size > 2048) in_size = 2048;
  app_message_open(in_size, 64);

  s_list_window = window_create();
  window_set_background_color(s_list_window, GColorBlack);
  window_set_window_handlers(s_list_window, (WindowHandlers){.load = list_load, .unload = list_unload});
  window_stack_push(s_list_window, true);
  apply_light();

#if defined(PBL_TOUCH)
  app_touch_navigation_enable(true);  // native touch scrolling for the list
  touch_service_subscribe(touch_handler, NULL);
#endif

#if FMA_DEMO
  demo_fill();
  app_timer_register(400, demo_tick, NULL);
  if (FMA_DEMO == 2) app_timer_register(DEMO_STEP_MS, demo_tour, NULL);
#endif

  s_settings_out = true;  // the phone app and the settings page learn what the watch has
  send_cmd(CMD_LIST, 0);
  s_keepalive_timer = app_timer_register(KEEPALIVE_MS, keepalive_cb, NULL);
  s_ui_timer = app_timer_register(1000, ui_tick, NULL);
}

static void deinit(void) {
  beeper_stop();
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
  light_enable(false);
  if (s_keepalive_timer) app_timer_cancel(s_keepalive_timer);
  if (s_ui_timer) app_timer_cancel(s_ui_timer);
  if (s_retry_timer) app_timer_cancel(s_retry_timer);
  window_destroy(s_list_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
