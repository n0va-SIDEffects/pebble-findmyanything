#include "icons.h"
#include "i18n.h"

GColor category_color(uint8_t cat) {
  switch (cat) {
    case CAT_TRACKER:   return GColorFromHEX(0xFF5555);
    case CAT_PHONE:     return GColorFromHEX(0x55AAFF);
    case CAT_COMPUTER:  return GColorFromHEX(0x5555FF);
    case CAT_AUDIO:     return GColorFromHEX(0xFFAA00);
    case CAT_WEARABLE:  return GColorFromHEX(0x55FF55);
    case CAT_MEDIA:     return GColorFromHEX(0xAA55FF);
    case CAT_BEACON:    return GColorFromHEX(0x00AAAA);
    case CAT_SMARTHOME: return GColorFromHEX(0xAAFF55);
    case CAT_INPUT:     return GColorFromHEX(0xFF55FF);
    case CAT_VEHICLE:   return GColorFromHEX(0xFFFF55);
    case CAT_HEALTH:    return GColorFromHEX(0xFF55AA);
    default:            return GColorFromHEX(0xAAAAAA);
  }
}

const char *category_label(uint8_t cat) {
  return tr((StrId)(S_CAT_0 + (cat <= CAT_HEALTH ? cat : 0)));
}

static void fill(GContext *ctx, int x, int y, int w, int h, int radius) {
  graphics_fill_rect(ctx, GRect(x, y, w, h), radius, radius ? GCornersAll : GCornerNone);
}

void draw_category_icon(GContext *ctx, GRect r, uint8_t cat, GColor bg) {
  const int x = r.origin.x, y = r.origin.y;
  // black & white: the opposite of the background
  const GColor c = PBL_IF_COLOR_ELSE(category_color(cat), gcolor_equal(bg, GColorWhite) ? GColorBlack : GColorWhite);
  graphics_context_set_fill_color(ctx, c);
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_antialiased(ctx, false);

  switch (cat) {
    case CAT_TRACKER:  // round tag with a hole
      graphics_fill_circle(ctx, GPoint(x + 10, y + 11), 8);
      graphics_context_set_fill_color(ctx, bg);
      graphics_fill_circle(ctx, GPoint(x + 10, y + 11), 3);
      graphics_context_set_fill_color(ctx, c);
      fill(ctx, x + 9, y, 3, 3, 0);
      break;
    case CAT_PHONE:
      fill(ctx, x + 5, y + 1, 11, 19, 2);
      graphics_context_set_fill_color(ctx, bg);
      fill(ctx, x + 7, y + 3, 7, 12, 0);
      break;
    case CAT_COMPUTER:  // laptop
      fill(ctx, x + 3, y + 3, 15, 11, 1);
      graphics_context_set_fill_color(ctx, bg);
      fill(ctx, x + 5, y + 5, 11, 7, 0);
      graphics_context_set_fill_color(ctx, c);
      fill(ctx, x, y + 15, 21, 3, 1);
      break;
    case CAT_AUDIO:  // headphones
      graphics_context_set_stroke_width(ctx, 3);
      graphics_draw_arc(ctx, GRect(x + 2, y + 1, 17, 17), GOvalScaleModeFitCircle,
                        DEG_TO_TRIGANGLE(270), DEG_TO_TRIGANGLE(450));
      fill(ctx, x + 1, y + 10, 5, 9, 1);
      fill(ctx, x + 15, y + 10, 5, 9, 1);
      break;
    case CAT_WEARABLE:  // watch
      fill(ctx, x + 7, y, 7, 21, 0);
      fill(ctx, x + 4, y + 5, 13, 11, 2);
      graphics_context_set_fill_color(ctx, bg);
      fill(ctx, x + 6, y + 7, 9, 7, 0);
      break;
    case CAT_MEDIA:  // tv
      fill(ctx, x + 1, y + 3, 19, 13, 1);
      graphics_context_set_fill_color(ctx, bg);
      fill(ctx, x + 3, y + 5, 15, 9, 0);
      graphics_context_set_fill_color(ctx, c);
      fill(ctx, x + 6, y + 17, 9, 2, 0);
      break;
    case CAT_BEACON:  // dot with waves
      graphics_fill_circle(ctx, GPoint(x + 5, y + 15), 3);
      graphics_context_set_stroke_width(ctx, 2);
      graphics_draw_arc(ctx, GRect(x - 3, y + 7, 16, 16), GOvalScaleModeFitCircle,
                        DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(90));
      graphics_draw_arc(ctx, GRect(x - 9, y + 1, 28, 28), GOvalScaleModeFitCircle,
                        DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(90));
      break;
    case CAT_SMARTHOME:  // house
      graphics_context_set_stroke_width(ctx, 3);
      graphics_draw_line(ctx, GPoint(x + 2, y + 10), GPoint(x + 10, y + 2));
      graphics_draw_line(ctx, GPoint(x + 10, y + 2), GPoint(x + 18, y + 10));
      fill(ctx, x + 5, y + 10, 11, 9, 0);
      graphics_context_set_fill_color(ctx, bg);
      fill(ctx, x + 9, y + 13, 3, 6, 0);
      break;
    case CAT_INPUT:  // gamepad / keyboard
      fill(ctx, x + 1, y + 5, 19, 11, 4);
      graphics_context_set_fill_color(ctx, bg);
      fill(ctx, x + 4, y + 9, 5, 2, 0);
      fill(ctx, x + 5, y + 8, 2, 4, 0);
      graphics_fill_circle(ctx, GPoint(x + 15, y + 9), 1);
      graphics_fill_circle(ctx, GPoint(x + 13, y + 12), 1);
      break;
    case CAT_VEHICLE:  // car
      fill(ctx, x + 5, y + 4, 11, 6, 2);
      fill(ctx, x + 1, y + 9, 19, 7, 2);
      graphics_context_set_fill_color(ctx, bg);
      graphics_fill_circle(ctx, GPoint(x + 5, y + 16), 3);
      graphics_fill_circle(ctx, GPoint(x + 15, y + 16), 3);
      graphics_context_set_fill_color(ctx, c);
      graphics_fill_circle(ctx, GPoint(x + 5, y + 16), 2);
      graphics_fill_circle(ctx, GPoint(x + 15, y + 16), 2);
      break;
    case CAT_HEALTH:  // cross
      fill(ctx, x + 7, y + 2, 7, 17, 1);
      fill(ctx, x + 2, y + 7, 17, 7, 1);
      break;
    default:  // generic: ring with dot
      graphics_context_set_stroke_width(ctx, 2);
      graphics_draw_circle(ctx, GPoint(x + 10, y + 10), 7);
      graphics_fill_circle(ctx, GPoint(x + 10, y + 10), 2);
      break;
  }
  graphics_context_set_stroke_width(ctx, 1);
}
