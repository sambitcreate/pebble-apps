#include <pebble.h>
#include "../../shared/pebble_pastel.h"

static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;

static PastelTheme s_theme;
static int s_theme_id = THEME_WARM_SUNSET;

// BCD columns: H1 H0 : M1 M0 : S1 S0
// Bit weights top to bottom: 8, 4, 2, 1

#define DOT_RADIUS 5
#define ROWS 4
#define COLS 6

#define STORAGE_KEY_SHOW_DIGITAL 1
#define MSG_KEY_SHOW_DIGITAL 0

// Animation constants
#define ANIM_STEPS 10
#define ANIM_INTERVAL_MS 30  // 30ms * 10 steps = 300ms total

static int s_digits[COLS];
static int s_prev_digits[COLS];
static char s_time_buf[12];
static char s_date_buf[16];
static bool s_show_digital = true;

// Bit-flip animation: 0 = no animation, 1..ANIM_STEPS = animating
// Positive = turning ON, Negative = turning OFF
static int s_anim_progress[COLS][ROWS];
static AppTimer *s_anim_timer = NULL;
static bool s_animating = false;


static void apply_digital_visibility(void) {
  layer_set_hidden(text_layer_get_layer(s_time_layer), !s_show_digital);
}

static void anim_timer_callback(void *data);

static void start_animation(void) {
  if (!s_animating) {
    s_animating = true;
    s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_timer_callback, NULL);
  }
}

static void anim_timer_callback(void *data) {
  bool still_animating = false;

  for (int col = 0; col < COLS; col++) {
    for (int row = 0; row < ROWS; row++) {
      if (s_anim_progress[col][row] > 0) {
        // Animating ON: increment toward ANIM_STEPS
        s_anim_progress[col][row]++;
        if (s_anim_progress[col][row] > ANIM_STEPS) {
          s_anim_progress[col][row] = 0;  // done
        } else {
          still_animating = true;
        }
      } else if (s_anim_progress[col][row] < 0) {
        // Animating OFF: decrement toward -ANIM_STEPS
        s_anim_progress[col][row]--;
        if (s_anim_progress[col][row] < -ANIM_STEPS) {
          s_anim_progress[col][row] = 0;  // done
        } else {
          still_animating = true;
        }
      }
    }
  }

  layer_mark_dirty(s_canvas);

  if (still_animating) {
    s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_timer_callback, NULL);
  } else {
    s_animating = false;
    s_anim_timer = NULL;
  }
}

static void update_time(void) {
  time_t now = time(NULL);
  const struct tm *t = localtime(&now);

  // Save previous digits for animation diff
  for (int i = 0; i < COLS; i++) {
    s_prev_digits[i] = s_digits[i];
  }

  s_digits[0] = t->tm_hour / 10;
  s_digits[1] = t->tm_hour % 10;
  s_digits[2] = t->tm_min / 10;
  s_digits[3] = t->tm_min % 10;
  s_digits[4] = t->tm_sec / 10;
  s_digits[5] = t->tm_sec % 10;

  // Detect bit flips and start animations
  const int weights[ROWS] = {8, 4, 2, 1};
  bool needs_anim = false;
  for (int col = 0; col < COLS; col++) {
    if (s_digits[col] != s_prev_digits[col]) {
      for (int row = 0; row < ROWS; row++) {
        bool was_on = (s_prev_digits[col] & weights[row]) != 0;
        bool is_on = (s_digits[col] & weights[row]) != 0;
        if (was_on != is_on) {
          if (is_on) {
            s_anim_progress[col][row] = 1;  // start grow animation
          } else {
            s_anim_progress[col][row] = -1; // start shrink animation
          }
          needs_anim = true;
        }
      }
    }
  }
  if (needs_anim) {
    start_animation();
  }

  // Digital time string
  snprintf(s_time_buf, sizeof(s_time_buf), "%02d:%02d:%02d",
           t->tm_hour, t->tm_min, t->tm_sec);
  text_layer_set_text(s_time_layer, s_time_buf);

  // Date string: "MON MAR 24"
  static const char *day_names[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
  };
  static const char *month_names[] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
  };
  snprintf(s_date_buf, sizeof(s_date_buf), "%s %s %d",
           day_names[t->tm_wday], month_names[t->tm_mon], t->tm_mday);
  text_layer_set_text(s_date_layer, s_date_buf);

  // Update theme colors on text layers
  text_layer_set_text_color(s_date_layer, s_theme.secondary);
  text_layer_set_text_color(s_time_layer, s_theme.accent);
}

// Screen is 144x168. All positions are hardcoded for safety.
// Layout (top to bottom):
//   [Date text: 18px]
//   [4px gap]
//   [dots: Y_ORIGIN=26, 4 rows * 20px spacing = 60px, bottom at 86]
//   [4px gap]
//   [Column labels at y=92]
//   [Digital time at bottom]
//
// Within-pair spacing: 15px center-to-center
// Between-pair extra: 10px (for colon)
// rel[] = {0, 15, 40, 55, 80, 95}
// total dot grid = 95px
// left_margin = 18px (labels), x_start = 18 + (144-18-4-95)/2 = 31
// col positions: 31, 46, 71, 86, 111, 126

static const int COL_X[6] = {31, 46, 71, 86, 111, 126};
static const int COLON_X[2] = {58, 98};  // midpoints between pairs
#define ROW_SPACING 20
#define Y_ORIGIN 30
#define LABEL_RIGHT_EDGE 16  // right edge of label text area
#define COL_LABELS_Y 94      // y position for column labels below dot grid

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  const int weights[ROWS] = {8, 4, 2, 1};

  // Determine theme colors from pastel design system
  GColor color_on         = s_theme.highlight;
  GColor color_off_stroke = s_theme.muted;
  GColor color_dim        = s_theme.muted;
  GColor color_label      = s_theme.muted;
  GColor color_colon      = s_theme.highlight;
  GColor color_col_label  = s_theme.muted;

  // Row weight labels -- left margin, clear of all dots
  graphics_context_set_text_color(ctx, color_label);
  const char *weight_labels[] = {"8", "4", "2", "1"};
  for (int row = 0; row < ROWS; row++) {
    int y = Y_ORIGIN + row * ROW_SPACING - 8;
    graphics_draw_text(ctx, weight_labels[row],
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, y, LABEL_RIGHT_EDGE, 16),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentRight, NULL);
  }

  // Dots with bit-flip animation
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
        graphics_context_set_fill_color(ctx, color_dim);
        graphics_fill_circle(ctx, center, 2);
      } else {
        int anim = s_anim_progress[col][row];
        if (anim != 0) {
          // Animation in progress -- compute interpolated radius
          int step;
          int radius;
          if (anim > 0) {
            // Growing ON: step goes 1..ANIM_STEPS
            step = anim;
            radius = (DOT_RADIUS * step) / ANIM_STEPS;
          } else {
            // Shrinking OFF: step goes -1..-ANIM_STEPS
            step = -anim;
            radius = DOT_RADIUS - (DOT_RADIUS * step) / ANIM_STEPS;
          }
          if (radius < 1) radius = 1;
          if (radius > DOT_RADIUS) radius = DOT_RADIUS;

          if (anim > 0) {
            // Turning on: draw filled circle at current radius
            graphics_context_set_fill_color(ctx, color_on);
            graphics_fill_circle(ctx, center, radius);
          } else {
            // Turning off: draw filled circle shrinking
            graphics_context_set_fill_color(ctx, color_on);
            graphics_fill_circle(ctx, center, radius);
            // Also draw the outline so it transitions smoothly to "off"
            graphics_context_set_stroke_color(ctx, color_off_stroke);
            graphics_context_set_stroke_width(ctx, 1);
            graphics_draw_circle(ctx, center, DOT_RADIUS);
          }
        } else if (bit_on) {
          graphics_context_set_fill_color(ctx, color_on);
          graphics_fill_circle(ctx, center, DOT_RADIUS);
        } else {
          graphics_context_set_stroke_color(ctx, color_off_stroke);
          graphics_context_set_stroke_width(ctx, 1);
          graphics_draw_circle(ctx, center, DOT_RADIUS);
        }
      }
    }
  }

  // Colon separators
  graphics_context_set_fill_color(ctx, color_colon);
  for (int c = 0; c < 2; c++) {
    int cx = COLON_X[c];
    int cy1 = Y_ORIGIN + ROW_SPACING;
    int cy2 = Y_ORIGIN + 2 * ROW_SPACING;
    graphics_fill_circle(ctx, GPoint(cx, cy1), 2);
    graphics_fill_circle(ctx, GPoint(cx, cy2), 2);
  }

  // Column labels "H H : M M : S S" below the dot grid
  graphics_context_set_text_color(ctx, color_col_label);
  const char *col_labels[] = {"H", "H", "M", "M", "S", "S"};
  for (int col = 0; col < COLS; col++) {
    int x = COL_X[col];
    graphics_draw_text(ctx, col_labels[col],
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(x - 6, COL_LABELS_Y, 12, 16),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
  // Colon labels between pairs
  for (int c = 0; c < 2; c++) {
    graphics_draw_text(ctx, ":",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(COLON_X[c] - 4, COL_LABELS_Y, 8, 16),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
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

  Tuple *theme_t = dict_find(iter, PASTEL_MSG_KEY_THEME);
  if (theme_t) {
    s_theme_id = theme_t->value->int32;
    persist_write_int(PASTEL_STORAGE_KEY_THEME, s_theme_id);
    s_theme = pastel_get_theme(s_theme_id);
    // Refresh text layer colors
    text_layer_set_text_color(s_date_layer, s_theme.secondary);
    text_layer_set_text_color(s_time_layer, s_theme.accent);
    layer_mark_dirty(s_canvas);
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

  // Date display at top
  s_date_layer = text_layer_create(GRect(0, 0, bounds.size.w, 20));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, s_theme.secondary);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_date_layer));

  // Digital time at bottom
  s_time_layer = text_layer_create(GRect(0, bounds.size.h - 36, bounds.size.w, 30));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, s_theme.accent);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // Load saved setting
  if (persist_exists(STORAGE_KEY_SHOW_DIGITAL)) {
    s_show_digital = persist_read_bool(STORAGE_KEY_SHOW_DIGITAL);
  }
  if (persist_exists(PASTEL_STORAGE_KEY_THEME)) {
    s_theme_id = persist_read_int(PASTEL_STORAGE_KEY_THEME);
  }
  s_theme = pastel_get_theme(s_theme_id);
  apply_digital_visibility();

  // Initialize previous digits to current so first draw has no animation
  time_t now = time(NULL);
  const struct tm *t = localtime(&now);
  s_prev_digits[0] = t->tm_hour / 10;
  s_prev_digits[1] = t->tm_hour % 10;
  s_prev_digits[2] = t->tm_min / 10;
  s_prev_digits[3] = t->tm_min % 10;
  s_prev_digits[4] = t->tm_sec / 10;
  s_prev_digits[5] = t->tm_sec % 10;

  update_time();
}

static void window_unload(Window *window) {
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
    s_animating = false;
  }
  text_layer_destroy(s_date_layer);
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
  app_message_open(128, 64);
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
