#pragma once
#include <pebble.h>

// Device categories, shared with the Android app (Protocol.kt, object Category)
enum {
  CAT_OTHER = 0,
  CAT_TRACKER = 1,
  CAT_PHONE = 2,
  CAT_COMPUTER = 3,
  CAT_AUDIO = 4,
  CAT_WEARABLE = 5,
  CAT_MEDIA = 6,
  CAT_BEACON = 7,
  CAT_SMARTHOME = 8,
  CAT_INPUT = 9,
  CAT_VEHICLE = 10,
  CAT_HEALTH = 11,
};

GColor category_color(uint8_t cat);
const char *category_label(uint8_t cat);
// Draws a small symbol for the category into r (designed for 20 x 20)
void draw_category_icon(GContext *ctx, GRect r, uint8_t cat, GColor bg);
