#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_time_layer;

// BCD columns: H1 H0 : M1 M0 : S1 S0
// Bit weights top to bottom: 8, 4, 2, 1

#define DOT_RADIUS 7
#define COL_WITHIN_PAIR 20  // center-to-center within a pair
#define COL_BETWEEN_PAIR 22 // extra gap between pairs (for colon)
#define ROW_SPACING 24
#define ROWS 4
#define COLS 6
#define LABEL_WIDTH 18      // space reserved for weight labels on left

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

// Explicit x positions for each column, computed once
// Layout: [label_area] [col0 col1] : [col2 col3] : [col4 col5]
static int s_col_x[COLS];
static int s_colon_x[2];
static int s_label_x;
static int s_y_origin;

static void compute_layout(int screen_w, int screen_h) {
  // Total dot grid width: 3 pairs of 2 columns
  // pair_w = COL_WITHIN_PAIR (one gap within pair)
  // Between pairs: COL_WITHIN_PAIR + COL_BETWEEN_PAIR
  int grid_w = 5 * COL_WITHIN_PAIR + 2 * COL_BETWEEN_PAIR;
  // grid_w = 5*20 + 2*22 = 100 + 44 = 144... too wide

  // Shift grid right to make room for labels
  int left_margin = LABEL_WIDTH + 4; // labels + gap
  int avail_w = screen_w - left_margin - 4; // 4px right margin
  // avail_w = 144 - 22 - 4 = 118

  // Scale spacing to fit: use 18px within pair, 20px between pair gap
  int cw = 18; // within-pair
  int cg = 12; // extra gap for colon area (on top of cw)

  // col positions relative to first col:
  // col0=0, col1=cw, col2=2*cw+cg, col3=3*cw+cg, col4=4*cw+2*cg, col5=5*cw+2*cg
  int rel[6] = {0, cw, 2*cw+cg, 3*cw+cg, 4*cw+2*cg, 5*cw+2*cg};
  int total = rel[5]; // = 5*18 + 2*12 = 90 + 24 = 114

  int x_start = left_margin + (avail_w - total) / 2;

  for (int i = 0; i < COLS; i++) {
    s_col_x[i] = x_start + rel[i];
  }

  // Colons centered between pairs
  s_colon_x[0] = (s_col_x[1] + s_col_x[2]) / 2;
  s_colon_x[1] = (s_col_x[3] + s_col_x[4]) / 2;

  // Label area: right-aligned before the first column
  s_label_x = 0;

  // Vertical: center 4 rows with room for digital time below
  int grid_h = (ROWS - 1) * ROW_SPACING; // 3 * 24 = 72
  s_y_origin = (screen_h - grid_h - 40) / 2 + 8; // 40px reserved for digital time area
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  const int weights[ROWS] = {8, 4, 2, 1};

  // Row weight labels — in the left margin, well clear of dots
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  const char *weight_labels[] = {"8", "4", "2", "1"};
  for (int row = 0; row < ROWS; row++) {
    int y = s_y_origin + row * ROW_SPACING - 9;
    graphics_draw_text(ctx, weight_labels[row],
                       fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(s_label_x, y, LABEL_WIDTH, 20),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentRight, NULL);
  }

  // Dots
  for (int col = 0; col < COLS; col++) {
    int x = s_col_x[col];
    for (int row = 0; row < ROWS; row++) {
      int y = s_y_origin + row * ROW_SPACING;
      GPoint center = GPoint(x, y);
      bool bit_on = (s_digits[col] & weights[row]) != 0;

      // Tens columns never use 8-bit; H tens never uses 4-bit
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

  // Colon separators — 3px dots, clearly visible
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  for (int c = 0; c < 2; c++) {
    int cx = s_colon_x[c];
    int cy1 = s_y_origin + ROW_SPACING;
    int cy2 = s_y_origin + 2 * ROW_SPACING;
    graphics_fill_circle(ctx, GPoint(cx, cy1), 3);
    graphics_fill_circle(ctx, GPoint(cx, cy2), 3);
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

  compute_layout(bounds.size.w, bounds.size.h);

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
