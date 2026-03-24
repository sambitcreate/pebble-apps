#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;

// BCD columns: H1 H0 : M1 M0 : S1 S0
// Each digit 0-9, max 4 bits (8,4,2,1)
// H1 max=2 (3 bits), H0 max=9 (4 bits)
// M1 max=5 (3 bits), M0 max=9 (4 bits)
// S1 max=5 (3 bits), S0 max=9 (4 bits)

#define DOT_RADIUS 8
#define DOT_SPACING 22
#define COL_SPACING 20
#define ROWS 4
#define COLS 6

static int s_digits[COLS];

static void update_time(void) {
  time_t now = time(NULL);
  const struct tm *t = localtime(&now);

  s_digits[0] = t->tm_hour / 10;
  s_digits[1] = t->tm_hour % 10;
  s_digits[2] = t->tm_min / 10;
  s_digits[3] = t->tm_min % 10;
  s_digits[4] = t->tm_sec / 10;
  s_digits[5] = t->tm_sec % 10;
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Calculate centering offsets
  int total_w = COLS * COL_SPACING + 2 * 10; // extra spacing for colons
  int total_h = ROWS * DOT_SPACING;
  int x_off = (bounds.size.w - total_w) / 2 + DOT_RADIUS;
  int y_off = (bounds.size.h - total_h) / 2 + DOT_RADIUS;

  // Bit weights top to bottom: 8, 4, 2, 1
  const int weights[ROWS] = {8, 4, 2, 1};

  for (int col = 0; col < COLS; col++) {
    int x = x_off + col * COL_SPACING;
    // Add gap for colon separators
    if (col >= 2) x += 10;
    if (col >= 4) x += 10;

    for (int row = 0; row < ROWS; row++) {
      int y = y_off + row * DOT_SPACING;
      GPoint center = GPoint(x, y);

      bool bit_on = (s_digits[col] & weights[row]) != 0;

      // Skip impossible bits (H tens max=2, M/S tens max=5)
      bool valid = true;
      if (col == 0 && row == 0) valid = false; // H1 never has 8
      if ((col == 0 || col == 2 || col == 4) && row == 0) valid = false; // tens never have 8

      if (!valid) {
        // Dim dot for impossible position
        graphics_context_set_fill_color(ctx, GColorDarkGray);
        graphics_fill_circle(ctx, center, DOT_RADIUS / 3);
      } else if (bit_on) {
        // Lit dot
        graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
        graphics_fill_circle(ctx, center, DOT_RADIUS);
      } else {
        // Unlit dot (outline)
        graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorLightGray));
        graphics_context_set_stroke_width(ctx, 1);
        graphics_draw_circle(ctx, center, DOT_RADIUS);
      }
    }
  }

  // Draw colon separators
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  int colon1_x = x_off + 2 * COL_SPACING + 3;
  int colon2_x = x_off + 4 * COL_SPACING + 13;
  int cy1 = y_off + DOT_SPACING;
  int cy2 = y_off + 2 * DOT_SPACING;
  graphics_fill_circle(ctx, GPoint(colon1_x, cy1), 2);
  graphics_fill_circle(ctx, GPoint(colon1_x, cy2), 2);
  graphics_fill_circle(ctx, GPoint(colon2_x, cy1), 2);
  graphics_fill_circle(ctx, GPoint(colon2_x, cy2), 2);

  // Label row weights on the left
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  char *labels[] = {"8", "4", "2", "1"};
  for (int row = 0; row < ROWS; row++) {
    int y = y_off + row * DOT_SPACING - 8;
    graphics_draw_text(ctx, labels[row],
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(2, y, 14, 16),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  update_time();
  layer_mark_dirty(s_canvas);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  update_time();
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
