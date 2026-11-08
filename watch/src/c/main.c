// Find My Anything: finds Bluetooth devices by signal strength.
// The phone (Android companion app) scans; the watch shows the list and guides the search.
#include <pebble.h>
#include "icons.h"

// 1 = fake data for emulator screenshots, no phone needed
#ifndef FMA_DEMO
#define FMA_DEMO 0
#endif

// ---- Protocol (docs/protokoll.md, Android: Protocol.kt) ----
enum { CMD_LIST = 1, CMD_SEARCH = 2, CMD_STOP = 3 };
enum { TYPE_LIST = 1, TYPE_SEARCH = 2, TYPE_STATUS = 3 };
#define ST_BT_OFF         (1 << 0)
#define ST_NO_PERMISSION  (1 << 1)
#define ST_SCREEN_OFF     (1 << 2)
#define ST_CLASSIC_ACTIVE (1 << 3)
#define ST_LOCATION_OFF   (1 << 4)
#define ST_NO_BG_LOCATION (1 << 5)
#define ST_SCAN_ERROR     (1 << 6)
#define FL_CLASSIC    (1 << 0)
#define FL_BONDED     (1 << 2)

#define MAX_DEVICES 40
#define TEXT_LEN 25
#define PERSIST_FEEDBACK 1
#define KEEPALIVE_MS 60000

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
static uint8_t s_view[MAX_DEVICES];
static int s_view_count, s_total;
static int32_t s_status;
static char s_status_text[48];
static bool s_got_data;
static int64_t s_start_ms;

static int s_filter;
static const char *const FILTER_NAMES[] = {"Alle", "Tracker", "Audio", "Handy & PC", "Wearables", "Sonstige"};
#define FILTER_COUNT 6

static Search s_search;
static bool s_in_search;
static int s_feedback = 1;  // bit0 = tone, bit1 = vibration
static const char *const FEEDBACK_NAMES[] = {"Signal: stumm", "Signal: Ton", "Signal: Vibration", "Signal: Ton + Vibration"};
static bool s_light_on;
static char s_toast[40];
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

// 0..100 from -100..-40 dBm
static int signal_pct(int rssi) { return clampi((rssi + 100) * 100 / 60, 0, 100); }

static GColor heat_color(int rssi) {
  if (rssi >= -50) return GColorFromHEX(0xFF0000);
  if (rssi >= -60) return GColorFromHEX(0xFF5500);
  if (rssi >= -70) return GColorFromHEX(0xFFAA00);
  if (rssi >= -80) return GColorFromHEX(0xAAFF55);
  if (rssi >= -90) return GColorFromHEX(0x00AAFF);
  return GColorFromHEX(0x5555FF);
}

static const char *proximity(int rssi) {
  if (rssi >= -50) return "direkt daneben";
  if (rssi >= -60) return "sehr nah";
  if (rssi >= -70) return "nah";
  if (rssi >= -80) return "mittel";
  if (rssi >= -90) return "weit";
  return "sehr weit";
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
  if (FMA_DEMO || !s_pending_cmd || s_outbox_busy) return;
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) {
    schedule_retry(500);
    return;
  }
  dict_write_uint8(it, MESSAGE_KEY_Cmd, (uint8_t)s_pending_cmd);
  if (s_pending_cmd == CMD_SEARCH) dict_write_uint32(it, MESSAGE_KEY_Target, s_pending_target);
  if (app_message_outbox_send() == APP_MSG_OK) {
    s_outbox_busy = true;
    s_last_cmd = s_pending_cmd;
    s_last_target = s_pending_target;
    s_pending_cmd = 0;
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

static void rebuild_view(void) {
  s_view_count = 0;
  for (int i = 0; i < s_live_count; i++) {
    if (filter_match(s_live[i].cat)) s_view[s_view_count++] = (uint8_t)i;
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
  menu_layer_reload_data(s_menu);
  if (keep_id) {
    for (int r = 0; r < s_view_count; r++) {
      if (s_live[s_view[r]].id == keep_id) {
        menu_layer_set_selected_index(s_menu, (MenuIndex){0, r}, MenuRowAlignNone, false);
        break;
      }
    }
  }
  layer_set_hidden(s_empty_layer, s_view_count > 0);
  layer_mark_dirty(s_empty_layer);
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
  memcpy(s_live, s_build, sizeof(Device) * s_build_count);
  s_live_count = s_build_count;
  s_total = total;
  rebuild_view();
  refresh_list(keep);
}

static void handle_list(DictionaryIterator *it) {
  int seq = get_int(it, MESSAGE_KEY_Seq, 0);
  int part = get_int(it, MESSAGE_KEY_Part, 0);
  int parts = get_int(it, MESSAGE_KEY_Parts, 1);
  if (part == 0) {
    s_build_count = 0;
    s_build_seq = seq;
    s_build_next = 0;
  }
  if (seq != s_build_seq || part != s_build_next) {  // lost a part: wait for the next snapshot
    s_build_seq = -1;
    return;
  }
  Tuple *data = dict_find(it, MESSAGE_KEY_Data);
  if (data) parse_entries(data->value->data, data->length);
  s_build_next++;
  if (s_build_next >= parts) commit_list(get_int(it, MESSAGE_KEY_Total, s_build_count));
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
  Tuple *t = dict_find(it, MESSAGE_KEY_Category);
  if (t) s_search.cat = (uint8_t)get_int(it, MESSAGE_KEY_Category, 0);
  t = dict_find(it, MESSAGE_KEY_Name);
  if (t && t->length > 1) snprintf(s_search.name, sizeof(s_search.name), "%s", t->value->cstring);
  t = dict_find(it, MESSAGE_KEY_Detail);
  if (t) snprintf(s_search.detail, sizeof(s_search.detail), "%s", t->value->cstring);
  s_search.have = s_search.rssi > -127 && s_search.age10 < 9999;
  if (s_search.have && s_search.age10 <= 30 && s_search.rssi > s_search.peak) s_search.peak = s_search.rssi;
  layer_mark_dirty(s_search_layer);
}

static void inbox_received(DictionaryIterator *it, void *ctx) {
  s_got_data = true;
  Tuple *st = dict_find(it, MESSAGE_KEY_Status);
  if (st) s_status = get_int(it, MESSAGE_KEY_Status, 0);
  Tuple *stt = dict_find(it, MESSAGE_KEY_StatusText);
  if (stt) snprintf(s_status_text, sizeof(s_status_text), "%s", stt->value->cstring);
  switch (get_int(it, MESSAGE_KEY_Type, 0)) {
    case TYPE_LIST: handle_list(it); break;
    case TYPE_SEARCH: handle_search(it); break;
    default: break;
  }
  if (s_menu) layer_mark_dirty(menu_layer_get_layer(s_menu));
}

static void inbox_dropped(AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "inbox dropped %d", (int)reason);
}

// ---- List window ----
static uint16_t menu_rows(MenuLayer *ml, uint16_t section, void *ctx) { return s_view_count; }
static int16_t menu_row_height(MenuLayer *ml, MenuIndex *idx, void *ctx) { return 46; }
static int16_t menu_header_height(MenuLayer *ml, uint16_t section, void *ctx) { return 24; }

static void draw_phone_badge(GContext *ctx, GRect r) {
  GColor c = GColorFromHEX(0x55FF55);
  if (!s_got_data) c = GColorFromHEX(0x555555);
  else if (s_status & (ST_BT_OFF | ST_NO_PERMISSION | ST_SCAN_ERROR | ST_LOCATION_OFF)) c = GColorFromHEX(0xFF5555);
  else if (s_status & ST_SCREEN_OFF) c = GColorFromHEX(0xFFAA00);
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, r, 2, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(r.origin.x + 2, r.origin.y + 2, r.size.w - 4, r.size.h - 6), 0, GCornerNone);
}

static void menu_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *ctx2) {
  GRect b = layer_get_bounds(cell);
  graphics_context_set_fill_color(ctx, GColorFromHEX(0x000055));
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  char buf[48];
  if (s_filter == 0) snprintf(buf, sizeof(buf), "%d Geräte", s_total);
  else snprintf(buf, sizeof(buf), "%d/%d · %s", s_view_count, s_total, FILTER_NAMES[s_filter]);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(8, -1, b.size.w - 36, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (s_status & ST_CLASSIC_ACTIVE) {
    graphics_context_set_fill_color(ctx, GColorFromHEX(0x55AAFF));
    graphics_fill_circle(ctx, GPoint(b.size.w - 26, 12), 3);
  }
  draw_phone_badge(ctx, GRect(b.size.w - 17, 3, 9, 17));
}

static void draw_bars(GContext *ctx, int x, int y, int rssi, bool stale) {
  int n = bars_for(rssi);
  GColor on = stale ? GColorFromHEX(0x555555) : heat_color(rssi);
  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 3;
    graphics_context_set_fill_color(ctx, i < n ? on : GColorFromHEX(0x555555));
    graphics_fill_rect(ctx, GRect(x + i * 5, y + 13 - h, 3, h), 0, GCornerNone);
  }
}

static void menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  if (idx->row >= s_view_count) return;
  const Device *d = &s_live[s_view[idx->row]];
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  bool stale = d->age > 15;
  GColor bg = hl ? GColorFromHEX(0x0055AA) : GColorBlack;

  draw_category_icon(ctx, GRect(6, 13, 20, 20), d->cat, bg);

  int right = 50;
  graphics_context_set_text_color(ctx, stale ? GColorFromHEX(0xAAAAAA) : GColorWhite);
  graphics_draw_text(ctx, d->name, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(32, 1, b.size.w - 32 - right, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  char det[40];
  if (stale) snprintf(det, sizeof(det), "vor %d s · %s", d->age, d->detail);
  else if (d->flags & FL_BONDED) snprintf(det, sizeof(det), "gekoppelt · %s", d->detail);
  else snprintf(det, sizeof(det), "%s", d->detail);
  graphics_context_set_text_color(ctx, hl ? GColorFromHEX(0xAAFFFF) : GColorFromHEX(0xAAAAAA));
  graphics_draw_text(ctx, det, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(32, 23, b.size.w - 32 - right + 8, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  draw_bars(ctx, b.size.w - 26, 6, d->rssi, stale);
  char num[8];
  snprintf(num, sizeof(num), "%d", d->rssi);
  graphics_context_set_text_color(ctx, stale ? GColorFromHEX(0xAAAAAA) : GColorWhite);
  graphics_draw_text(ctx, num, fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(b.size.w - 46, 22, 42, 18),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

static void open_search(const Device *d);

static void menu_select(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  if (idx->row < s_view_count) open_search(&s_live[s_view[idx->row]]);
}

static void menu_select_long(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  uint32_t keep = selected_id();
  s_filter = (s_filter + 1) % FILTER_COUNT;
  rebuild_view();
  refresh_list(keep);
  if (s_view_count > 0 && !keep) menu_layer_set_selected_index(s_menu, (MenuIndex){0, 0}, MenuRowAlignTop, false);
  vibes_short_pulse();
}

static void draw_multiline(GContext *ctx, const char *text, GRect r, const char *font, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(font), r, GTextOverflowModeWordWrap,
                     GTextAlignmentCenter, NULL);
}

static void empty_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  // radar symbol
  GPoint c = GPoint(b.size.w / 2, 62);
  graphics_context_set_stroke_color(ctx, GColorFromHEX(0xFFAA00));
  graphics_context_set_fill_color(ctx, GColorFromHEX(0xFFAA00));
  graphics_context_set_stroke_width(ctx, 3);
  graphics_fill_circle(ctx, c, 5);
  for (int r = 14; r <= 38; r += 12) {
    graphics_draw_arc(ctx, GRect(c.x - r, c.y - r, 2 * r, 2 * r), GOvalScaleModeFitCircle,
                      DEG_TO_TRIGANGLE(-45), DEG_TO_TRIGANGLE(45));
  }
  const char *title, *text;
  char buf[96];
  int64_t waited = now_ms() - s_start_ms;
  if (!s_got_data && waited < 8000) {
    title = "Suche startet …";
    text = "Die Handy-App wird geweckt";
  } else if (!s_got_data) {
    title = "Keine Antwort vom Handy";
    text = "Ist die Find-My-Anything-App auf dem Handy installiert und eingerichtet?";
  } else if (s_status_text[0]) {
    title = "Handy meldet:";
    text = s_status_text;
  } else if (s_live_count > 0) {
    title = "Nichts in diesem Filter";
    snprintf(buf, sizeof(buf), "Filter «%s». Lang auf Select drücken für den nächsten Filter.", FILTER_NAMES[s_filter]);
    text = buf;
  } else {
    title = "Noch nichts gefunden";
    text = "Die Suche läuft weiter …";
  }
  draw_multiline(ctx, title, GRect(8, 112, b.size.w - 16, 30), FONT_KEY_GOTHIC_24_BOLD, GColorWhite);
  draw_multiline(ctx, text, GRect(12, 142, b.size.w - 24, 80), FONT_KEY_GOTHIC_18, GColorFromHEX(0xAAAAAA));
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
  });
  menu_layer_set_normal_colors(s_menu, GColorBlack, GColorWhite);
  menu_layer_set_highlight_colors(s_menu, GColorFromHEX(0x0055AA), GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu, w);
  layer_add_child(root, menu_layer_get_layer(s_menu));

  s_empty_layer = layer_create(GRect(0, 24, b.size.w, b.size.h - 24));
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

// Small marker across the band at a position (peak value)
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

static void draw_trend(GContext *ctx, GPoint c, int trend10) {
  if (trend10 > 10) {
    GPathInfo info = {3, (GPoint[]){{c.x - 9, c.y + 6}, {c.x + 9, c.y + 6}, {c.x, c.y - 8}}};
    GPath *p = gpath_create(&info);
    graphics_context_set_fill_color(ctx, GColorFromHEX(0xFF5500));
    gpath_draw_filled(ctx, p);
    gpath_destroy(p);
  } else if (trend10 < -10) {
    GPathInfo info = {3, (GPoint[]){{c.x - 9, c.y - 6}, {c.x + 9, c.y - 6}, {c.x, c.y + 8}}};
    GPath *p = gpath_create(&info);
    graphics_context_set_fill_color(ctx, GColorFromHEX(0x00AAFF));
    gpath_draw_filled(ctx, p);
    gpath_destroy(p);
  }
}

static void search_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  const int th = 8;
  int64_t now = now_ms();
  bool no_phone = now - s_search.last_msg > 6000;
  bool lost = !s_search.have || s_search.age10 > 100 || no_phone;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  GColor heat = lost ? GColorFromHEX(0x555555) : heat_color(s_search.rssi);
  border_band(ctx, b, th, 1000, GColorFromHEX(0x000055));
  if (!lost) border_band(ctx, b, th, signal_pct(s_search.rssi) * 10, heat);
  if (s_search.peak > -127) border_marker(ctx, b, th, signal_pct(s_search.peak) * 10, GColorWhite);

  // Header: icon, name, detail
  draw_category_icon(ctx, GRect(16, 16, 20, 20), s_search.cat, GColorBlack);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_search.name, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(42, 10, b.size.w - 42 - 14, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorFromHEX(0xAAAAAA));
  graphics_draw_text(ctx, s_search.detail, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(42, 30, b.size.w - 42 - 14, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  // Big signal value 0..100
  char num[8];
  if (lost) snprintf(num, sizeof(num), "--");
  else snprintf(num, sizeof(num), "%d", signal_pct(s_search.rssi));
  graphics_context_set_text_color(ctx, lost ? GColorFromHEX(0xAAAAAA) : heat);
  graphics_draw_text(ctx, num, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS), GRect(20, 56, b.size.w - 40, 50),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  if (!lost) {
    draw_trend(ctx, GPoint(b.size.w - 34, 84), s_search.trend10);
  }

  char line[48];
  const char *word;
  if (no_phone && s_search.last_msg) word = "Handy antwortet nicht";
  else if (!s_search.last_msg) word = "warte auf Handy …";
  else if (!s_search.have) word = "nicht gefunden";
  else if (lost) {
    snprintf(line, sizeof(line), "kein Signal seit %d s", s_search.age10 / 10);
    word = line;
  } else word = proximity(s_search.rssi);
  graphics_context_set_text_color(ctx, lost ? GColorFromHEX(0xAAAAAA) : heat);
  graphics_draw_text(ctx, word, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect(14, 108, b.size.w - 28, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Details
  char info[64];
  if (s_search.have) {
    if (s_search.peak > -127) snprintf(info, sizeof(info), "%d dBm · Bestwert %d", s_search.rssi, signal_pct(s_search.peak));
    else snprintf(info, sizeof(info), "%d dBm", s_search.rssi);
  } else {
    info[0] = '\0';
  }
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, info, fonts_get_system_font(FONT_KEY_GOTHIC_18), GRect(14, 140, b.size.w - 28, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  char info2[64];
  info2[0] = '\0';
  if (s_search.have && s_search.dist > 0) {
    if (s_search.dist < 100) snprintf(info2, sizeof(info2), "ca. %d cm · ", s_search.dist);
    else snprintf(info2, sizeof(info2), "ca. %d,%d m · ", s_search.dist / 100, (s_search.dist % 100) / 10);
  }
  size_t l = strlen(info2);
  snprintf(info2 + l, sizeof(info2) - l, "%d,%d Messungen/s", s_search.rate10 / 10, s_search.rate10 % 10);
  graphics_context_set_text_color(ctx, GColorFromHEX(0xAAAAAA));
  graphics_draw_text(ctx, info2, fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(14, 162, b.size.w - 28, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Bottom line: toast, phone hint or feedback mode
  const char *bottom;
  GColor bc = GColorFromHEX(0xAAAAAA);
  if (now < s_toast_until) { bottom = s_toast; bc = GColorWhite; }
  else if (s_status_text[0]) { bottom = s_status_text; bc = GColorFromHEX(0xFF5555); }
  else if ((s_status & ST_SCREEN_OFF) && s_search.rate10 < 15) { bottom = "Handy-Display aus: langsamer"; bc = GColorFromHEX(0xFFAA00); }
  else bottom = FEEDBACK_NAMES[s_feedback & 3];
  graphics_context_set_text_color(ctx, bc);
  graphics_draw_text(ctx, bottom, fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(14, b.size.h - 36, b.size.w - 28, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Beeps/vibration like a Geiger counter: faster and higher when closer
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
  if (!lost && s_feedback) {
    next = beep_interval(s_search.rssi);
    int t = clampi((s_search.rssi + 95) * 1000 / 50, 0, 1000);
#if defined(PBL_SPEAKER)
    if (s_feedback & 1) speaker_play_tone((uint16_t)(500 + 1700 * t / 1000), 35, 60, SpeakerWaveformTriangle);
#endif
    if (s_feedback & 2) {
      static const uint32_t seg[] = {40};
      VibePattern pat = {.durations = seg, .num_segments = 1};
      vibes_enqueue_custom_pattern(pat);
    }
  }
  s_beep_timer = app_timer_register(next, beep_cb, NULL);
}

// Repaint once a second (age counters, lost state)
static void ui_tick(void *ctx) {
  s_ui_timer = app_timer_register(1000, ui_tick, NULL);
  if (s_in_search && s_search_layer) layer_mark_dirty(s_search_layer);
  else if (s_empty_layer && !layer_get_hidden(s_empty_layer)) layer_mark_dirty(s_empty_layer);
}

static void search_select(ClickRecognizerRef r, void *ctx) {
  s_feedback = (s_feedback + 1) & 3;
#if defined(PBL_SPEAKER)
  if ((s_feedback & 1) && speaker_is_muted()) {
    toast("Ton ist auf der Uhr stumm");
    return;
  }
#endif
  toast(FEEDBACK_NAMES[s_feedback]);
}

static void search_up(ClickRecognizerRef r, void *ctx) {
  s_search.peak = s_search.have ? s_search.rssi : -127;
  toast("Bestwert zurückgesetzt");
}

static void search_down(ClickRecognizerRef r, void *ctx) {
  s_light_on = !s_light_on;
  light_enable(s_light_on);
  toast(s_light_on ? "Licht bleibt an" : "Licht automatisch");
}

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
  if (s_beep_timer) { app_timer_cancel(s_beep_timer); s_beep_timer = NULL; }
  if (s_light_on) { s_light_on = false; light_enable(false); }
  layer_destroy(s_search_layer);
  s_search_layer = NULL;
  window_destroy(s_search_window);
  s_search_window = NULL;
  s_current_cmd = CMD_LIST;
  send_cmd(CMD_LIST, 0);
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
  s_beep_timer = app_timer_register(500, beep_cb, NULL);
}

// ---- Demo data for the emulator ----
#if FMA_DEMO
static void demo_fill(void) {
  static const struct { const char *n, *d; uint8_t c; int8_t r; uint8_t age; uint8_t fl; } demo[] = {
    {"AirTag", "Find My getrennt, Akku voll", CAT_TRACKER, -54, 0, 0},
    {"Mikes Buds", "Fast Pair", CAT_AUDIO, -61, 0, FL_BONDED},
    {"iPhone/iPad/Mac", "Apple in der Nähe", CAT_PHONE, -66, 0, 0},
    {"Pebble Time 2", "Wearable", CAT_WEARABLE, -68, 0, FL_BONDED},
    {"Galaxy SmartTag", "Samsung SmartTag", CAT_TRACKER, -72, 1, 0},
    {"[TV] Samsung 7", "TV/Medien", CAT_MEDIA, -75, 0, 0},
    {"LYWSD03MMC", "Xiaomi MiBeacon", CAT_SMARTHOME, -79, 2, 0},
    {"Windows-Laptop", "Microsoft Windows-Laptop", CAT_COMPUTER, -83, 0, 0},
    {"MX Keys", "Logitech", CAT_INPUT, -86, 3, 0},
    {"Tile", "Tile-Tracker", CAT_TRACKER, -91, 4, 0},
    {"VW ID.3", "Fahrzeug", CAT_VEHICLE, -93, 20, 0},
  };
  s_live_count = ARRAY_LENGTH(demo);
  for (int i = 0; i < s_live_count; i++) {
    Device *d = &s_live[i];
    d->id = i + 1; d->rssi = demo[i].r; d->cat = demo[i].c; d->flags = demo[i].fl; d->age = demo[i].age;
    snprintf(d->name, TEXT_LEN, "%s", demo[i].n);
    snprintf(d->detail, TEXT_LEN, "%s", demo[i].d);
  }
  s_total = s_live_count + 3;
  s_got_data = true;
  s_status = ST_CLASSIC_ACTIVE;
  rebuild_view();
}

static void demo_tick(void *ctx) {
  app_timer_register(400, demo_tick, NULL);
  if (!s_in_search) return;
  static int phase;
  phase++;
  s_search.last_msg = now_ms();
  s_search.have = true;
  s_search.age10 = 1;
  s_search.rssi = -58 + (phase % 7) - 3;
  s_search.trend10 = 14;
  s_search.rate10 = 72;
  s_search.dist = 180;
  if (s_search.rssi > s_search.peak) s_search.peak = s_search.rssi + 2;
  if (s_search_layer) layer_mark_dirty(s_search_layer);
}
#endif

// ---- App ----
static void init(void) {
  s_start_ms = now_ms();
  if (persist_exists(PERSIST_FEEDBACK)) s_feedback = persist_read_int(PERSIST_FEEDBACK) & 3;

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_sent(outbox_sent);
  app_message_register_outbox_failed(outbox_failed);
  uint32_t in_size = app_message_inbox_size_maximum();
  if (in_size > 2048) in_size = 2048;
  app_message_open(in_size, 64);

#if FMA_DEMO
  demo_fill();
  app_timer_register(400, demo_tick, NULL);
#endif

  s_list_window = window_create();
  window_set_background_color(s_list_window, GColorBlack);
  window_set_window_handlers(s_list_window, (WindowHandlers){.load = list_load, .unload = list_unload});
  window_stack_push(s_list_window, true);

  send_cmd(CMD_LIST, 0);
  s_keepalive_timer = app_timer_register(KEEPALIVE_MS, keepalive_cb, NULL);
  s_ui_timer = app_timer_register(1000, ui_tick, NULL);
}

static void deinit(void) {
  persist_write_int(PERSIST_FEEDBACK, s_feedback);
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
