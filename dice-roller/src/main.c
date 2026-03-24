#include <pebble.h>

static Window *s_window;
static Layer *s_die_layer;
static TextLayer *s_result_layer;
static TextLayer *s_type_layer;
static TextLayer *s_history_layer;

typedef enum {
  DIE_D4 = 0,
  DIE_D6,
  DIE_D8,
  DIE_D10,
  DIE_D12,
  DIE_D20,
  DIE_COUNT,
} DieType;

static const int DIE_SIDES[] = {4, 6, 8, 10, 12, 20};
static const char *DIE_NAMES[] = {"D4", "D6", "D8", "D10", "D12", "D20"};

static DieType s_die_type = DIE_D6;
static int s_result = 0;
static int s_history[5] = {0};
static int s_history_count = 0;
static char s_result_buf[8];
static char s_type_buf[16];
static char s_history_buf[40];

static void draw_d6_pips(GContext *ctx, int value, GRect face) {
  int cx = face.origin.x + face.size.w / 2;
  int cy = face.origin.y + face.size.h / 2;
  int dx = face.size.w / 4;
  int dy = face.size.h / 4;
  int r = 5;

  GColor pip = PBL_IF_COLOR_ELSE(GColorBlack, GColorBlack);
  graphics_context_set_fill_color(ctx, pip);

  // Center pip
  if (value == 1 || value == 3 || value == 5) {
    graphics_fill_circle(ctx, GPoint(cx, cy), r);
  }
  // Top-left, bottom-right
  if (value >= 2) {
    graphics_fill_circle(ctx, GPoint(cx - dx, cy - dy), r);
    graphics_fill_circle(ctx, GPoint(cx + dx, cy + dy), r);
  }
  // Top-right, bottom-left
  if (value >= 4) {
    graphics_fill_circle(ctx, GPoint(cx + dx, cy - dy), r);
    graphics_fill_circle(ctx, GPoint(cx - dx, cy + dy), r);
  }
  // Middle-left, middle-right
  if (value == 6) {
    graphics_fill_circle(ctx, GPoint(cx - dx, cy), r);
    graphics_fill_circle(ctx, GPoint(cx + dx, cy), r);
  }
}

static void die_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int size = 80;
  int x = (bounds.size.w - size) / 2;
  int y = 10;
  GRect face = GRect(x, y, size, size);

  if (s_result == 0) return;

  if (s_die_type == DIE_D6) {
    // Draw D6 face with pips
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, face, 8, GCornersAll);
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_round_rect(ctx, face, 8);
    draw_d6_pips(ctx, s_result, face);
  } else {
    // Draw generic die shape
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
    if (s_die_type == DIE_D4) {
      // Triangle
      GPoint tri[] = {
        GPoint(x + size/2, y),
        GPoint(x, y + size),
        GPoint(x + size, y + size),
      };
      GPathInfo info = { .num_points = 3, .points = tri };
      GPath *path = gpath_create(&info);
      gpath_draw_filled(ctx, path);
      graphics_context_set_stroke_color(ctx, GColorBlack);
      gpath_draw_outline(ctx, path);
      gpath_destroy(path);
    } else {
      // Rounded rect for all others
      graphics_fill_rect(ctx, face, 4, GCornersAll);
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_round_rect(ctx, face, 4);
    }

    // Draw number in center
    snprintf(s_result_buf, sizeof(s_result_buf), "%d", s_result);
    graphics_context_set_text_color(ctx, s_die_type == DIE_D6 ? GColorBlack :
                                    PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
    graphics_draw_text(ctx, s_result_buf,
                       fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS),
                       GRect(x, y + size/2 - 22, size, 44),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void update_history_text(void) {
  s_history_buf[0] = '\0';
  int count = s_history_count > 5 ? 5 : s_history_count;
  for (int i = 0; i < count; i++) {
    char tmp[8];
    snprintf(tmp, sizeof(tmp), "%s%d", i > 0 ? " " : "", s_history[i]);
    strcat(s_history_buf, tmp);
  }
  text_layer_set_text(s_history_layer, s_history_buf);
}

static void roll(void) {
  s_result = (rand() % DIE_SIDES[s_die_type]) + 1;

  // Shift history
  if (s_history_count < 5) {
    s_history[s_history_count++] = s_result;
  } else {
    for (int i = 0; i < 4; i++) s_history[i] = s_history[i + 1];
    s_history[4] = s_result;
  }

  snprintf(s_result_buf, sizeof(s_result_buf), "%d", s_result);
  text_layer_set_text(s_result_layer, s_result_buf);
  update_history_text();
  layer_mark_dirty(s_die_layer);

  vibes_short_pulse();
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  roll();
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  roll();
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  s_die_type = (s_die_type + 1) % DIE_COUNT;
  snprintf(s_type_buf, sizeof(s_type_buf), "[ %s ]", DIE_NAMES[s_die_type]);
  text_layer_set_text(s_type_layer, s_type_buf);
  s_result = 0;
  text_layer_set_text(s_result_layer, "");
  layer_mark_dirty(s_die_layer);
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  s_die_type = (s_die_type + DIE_COUNT - 1) % DIE_COUNT;
  snprintf(s_type_buf, sizeof(s_type_buf), "[ %s ]", DIE_NAMES[s_die_type]);
  text_layer_set_text(s_type_layer, s_type_buf);
  s_result = 0;
  text_layer_set_text(s_result_layer, "");
  layer_mark_dirty(s_die_layer);
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  // Die type label
  s_type_layer = text_layer_create(GRect(0, 0, bounds.size.w, 24));
  text_layer_set_background_color(s_type_layer, GColorClear);
  text_layer_set_text_color(s_type_layer, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  text_layer_set_font(s_type_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_type_layer, GTextAlignmentCenter);
  snprintf(s_type_buf, sizeof(s_type_buf), "[ %s ]", DIE_NAMES[s_die_type]);
  text_layer_set_text(s_type_layer, s_type_buf);
  layer_add_child(root, text_layer_get_layer(s_type_layer));

  // Die visual
  s_die_layer = layer_create(GRect(0, 24, bounds.size.w, 100));
  layer_set_update_proc(s_die_layer, die_layer_update);
  layer_add_child(root, s_die_layer);

  // Result text (for non-D6 big number display beneath die)
  s_result_layer = text_layer_create(GRect(0, 120, bounds.size.w, 24));
  text_layer_set_background_color(s_result_layer, GColorClear);
  text_layer_set_text_color(s_result_layer, GColorWhite);
  text_layer_set_font(s_result_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_result_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_result_layer));

  // History
  s_history_layer = text_layer_create(GRect(0, bounds.size.h - 20, bounds.size.w, 20));
  text_layer_set_background_color(s_history_layer, GColorClear);
  text_layer_set_text_color(s_history_layer, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  text_layer_set_font(s_history_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_history_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_history_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_type_layer);
  text_layer_destroy(s_result_layer);
  text_layer_destroy(s_history_layer);
  layer_destroy(s_die_layer);
}

static void init(void) {
  srand(time(NULL));
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  accel_tap_service_subscribe(tap_handler);
}

static void deinit(void) {
  accel_tap_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
