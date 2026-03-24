#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_time_layer;

// BCD columns: H1 H0 : M1 M0 : S1 S0
// Bit weights top to bottom: 8, 4, 2, 1

#define DOT_RADIUS 6
#define PAIR_COL_SPACING 16   // spacing within H, M, S pairs
#define PAIR_GAP 24           // gap between pairs (for colons)
#define ROW_SPACING 22
#define ROWS 4
#define COLS 6

static int s_digits[COLS];
static char s_time_buf[12];

static void update_time(void) {
  time_t now = time(NULL);
  const struct tm *t = localtime(&now);

  s_digits[0] = t->tm_hour / 10;
  s_digits[1] = t->tm_hour % 10;
  s_digits[2] = t->tm_min / 10;
  s_digits[3] = t->tm_min % 10;
  s_digits[4] = t->tm_sec / 10;
  s_digits[5] = t->tm_sec % 10;

  snprintf(s_time_buf, sizeof(s_time_buf), "%02d:%02d:%02d",
           t->tm_hour, t->tm_min, t->tm_sec);
  text_layer_set_text(s_time_layer, s_time_buf);
}

// Get the x position for a column, with proper pair grouping
static int col_x(int col, int x_origin) {
  // Columns are grouped in pairs: [0,1] [2,3] [4,5]
  int pair = col / 2;
  int within = col % 2;
  return x_origin + pair * (2 * PAIR_COL_SPACING + PAIR_GAP) + within * PAIR_COL_SPACING;
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Calculate total grid dimensions for centering
  // Width: 3 pairs of 2 cols each, with gaps between pairs
  // col5_x - col0_x = 2*(2*PAIR_COL_SPACING + PAIR_GAP) + PAIR_COL_SPACING
  int grid_w = 2 * (2 * PAIR_COL_SPACING + PAIR_GAP) + PAIR_COL_SPACING;
  int grid_h = (ROWS - 1) * ROW_SPACING;

  int x_origin = (bounds.size.w - grid_w) / 2;
  int y_origin = 20;  // top margin for header area

  // Bit weights top to bottom: 8, 4, 2, 1
  const int weights[ROWS] = {8, 4, 2, 1};

  // Draw column header labels: H  H : M  M : S  S
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  const char *headers[] = {"H", "H", "M", "M", "S", "S"};
  for (int col = 0; col < COLS; col++) {
    int x = col_x(col, x_origin);
    graphics_draw_text(ctx, headers[col],
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(x - 6, y_origin - 18, 12, 16),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }

  // Draw row weight labels on the left
  const char *weight_labels[] = {"8", "4", "2", "1"};
  for (int row = 0; row < ROWS; row++) {
    int y = y_origin + row * ROW_SPACING;
    graphics_draw_text(ctx, weight_labels[row],
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(x_origin - 18, y - 9, 14, 16),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentRight, NULL);
  }

  // Draw dots
  for (int col = 0; col < COLS; col++) {
    int x = col_x(col, x_origin);

    for (int row = 0; row < ROWS; row++) {
      int y = y_origin + row * ROW_SPACING;
      GPoint center = GPoint(x, y);

      bool bit_on = (s_digits[col] & weights[row]) != 0;

      // Tens columns (0, 2, 4) never use the 8-bit
      // H tens (col 0) also never uses the 4-bit (max value 2)
      bool valid = true;
      if ((col == 0 || col == 2 || col == 4) && row == 0) valid = false;
      if (col == 0 && row == 1) valid = false;  // H tens max=2, no 4-bit

      if (!valid) {
        // Tiny dim dot for impossible position
        graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorDarkGray));
        graphics_fill_circle(ctx, center, 2);
      } else if (bit_on) {
        // Lit dot
        graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
        graphics_fill_circle(ctx, center, DOT_RADIUS);
      } else {
        // Unlit dot (outline only)
        graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorLightGray));
        graphics_context_set_stroke_width(ctx, 1);
        graphics_draw_circle(ctx, center, DOT_RADIUS);
      }
    }
  }

  // Draw colon separators between pairs
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  for (int pair = 0; pair < 2; pair++) {
    // Colon goes between col (pair*2+1) and col ((pair+1)*2)
    int left_x = col_x(pair * 2 + 1, x_origin);
    int right_x = col_x((pair + 1) * 2, x_origin);
    int colon_x = (left_x + right_x) / 2;
    int cy1 = y_origin + ROW_SPACING;
    int cy2 = y_origin + 2 * ROW_SPACING;
    graphics_fill_circle(ctx, GPoint(colon_x, cy1), 2);
    graphics_fill_circle(ctx, GPoint(colon_x, cy2), 2);
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

  // Digital time at bottom for quick reading
  s_time_layer = text_layer_create(GRect(0, bounds.size.h - 36, bounds.size.w, 30));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  update_time();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
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
