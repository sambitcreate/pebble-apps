#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_time_layer;

// BCD columns: H1 H0 : M1 M0 : S1 S0
// Bit weights top to bottom: 8, 4, 2, 1

#define DOT_RADIUS 5
#define ROWS 4
#define COLS 6

#define STORAGE_KEY_SHOW_DIGITAL 1
#define MSG_KEY_SHOW_DIGITAL 0

static int s_digits[COLS];
static char s_time_buf[12];
static bool s_show_digital = true;

static void apply_digital_visibility(void) {
  layer_set_hidden(text_layer_get_layer(s_time_layer), !s_show_digital);
}

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

// Screen is 144x168. All positions are hardcoded for safety.
// Layout: [14px labels] [4px gap] [dots: 6 cols in 3 pairs] [margin]
//
// Within-pair spacing: 15px center-to-center
// Between-pair extra: 10px (for colon)
// rel[] = {0, 15, 40, 55, 80, 95}
// total dot grid = 95px
// left_margin = 18px (labels), x_start = 18 + (144-18-4-95)/2 = 31
// col positions: 31, 46, 71, 86, 111, 126
// rightmost edge: 126+5 = 131 (safe, screen=144)
// Row spacing: 20px, grid_h = 60px
// y_origin: center in top 132px (168-36 for time) = 36

static const int COL_X[6] = {31, 46, 71, 86, 111, 126};
static const int COLON_X[2] = {58, 98};  // midpoints between pairs
#define ROW_SPACING 20
#define Y_ORIGIN 36
#define LABEL_RIGHT_EDGE 16  // right edge of label text area

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  const int weights[ROWS] = {8, 4, 2, 1};

  // Row weight labels — left margin, clear of all dots
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  const char *weight_labels[] = {"8", "4", "2", "1"};
  for (int row = 0; row < ROWS; row++) {
    int y = Y_ORIGIN + row * ROW_SPACING - 8;
    graphics_draw_text(ctx, weight_labels[row],
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, y, LABEL_RIGHT_EDGE, 16),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentRight, NULL);
  }

  // Dots
  for (int col = 0; col < COLS; col++) {
    int x = COL_X[col];
    for (int row = 0; row < ROWS; row++) {
      int y = Y_ORIGIN + row * ROW_SPACING;
      GPoint center = GPoint(x, y);
      bool bit_on = (s_digits[col] & weights[row]) != 0;

      bool valid = true;
      if ((col == 0 || col == 2 || col == 4) && row == 0) valid = false;
      if (col == 0 && row == 1) valid = false;

      if (!valid) {
        graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorDarkGray));
        graphics_fill_circle(ctx, center, 2);
      } else if (bit_on) {
        graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
        graphics_fill_circle(ctx, center, DOT_RADIUS);
      } else {
        graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorLightGray));
        graphics_context_set_stroke_width(ctx, 1);
        graphics_draw_circle(ctx, center, DOT_RADIUS);
      }
    }
  }

  // Colon separators
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  for (int c = 0; c < 2; c++) {
    int cx = COLON_X[c];
    int cy1 = Y_ORIGIN + ROW_SPACING;
    int cy2 = Y_ORIGIN + 2 * ROW_SPACING;
    graphics_fill_circle(ctx, GPoint(cx, cy1), 2);
    graphics_fill_circle(ctx, GPoint(cx, cy2), 2);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  update_time();
  layer_mark_dirty(s_canvas);
}

// AppMessage: receive config from phone
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *show_digital_t = dict_find(iter, MSG_KEY_SHOW_DIGITAL);
  if (show_digital_t) {
    s_show_digital = (show_digital_t->value->int32 != 0);
    persist_write_bool(STORAGE_KEY_SHOW_DIGITAL, s_show_digital);
    apply_digital_visibility();
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  // Digital time at bottom
  s_time_layer = text_layer_create(GRect(0, bounds.size.h - 36, bounds.size.w, 30));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // Load saved setting
  if (persist_exists(STORAGE_KEY_SHOW_DIGITAL)) {
    s_show_digital = persist_read_bool(STORAGE_KEY_SHOW_DIGITAL);
  }
  apply_digital_visibility();

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

  // Set up AppMessage for config
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(64, 64);
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
