#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_answer_layer;
static TextLayer *s_prompt_layer;

static const char *ANSWERS[] = {
  "It is\ncertain",
  "It is\ndecidedly so",
  "Without\na doubt",
  "Yes\ndefinitely",
  "You may\nrely on it",
  "As I\nsee it, yes",
  "Most\nlikely",
  "Outlook\ngood",
  "Yes",
  "Signs\npoint to yes",
  "Reply hazy\ntry again",
  "Ask again\nlater",
  "Better not\ntell you now",
  "Cannot\npredict now",
  "Concentrate\nand ask again",
  "Don't\ncount on it",
  "My reply\nis no",
  "My sources\nsay no",
  "Outlook\nnot so good",
  "Very\ndoubtful",
};
#define NUM_ANSWERS 20

static int s_current = 0;
static bool s_revealed = false;

static void reveal_answer(void) {
  s_current = rand() % NUM_ANSWERS;
  s_revealed = true;

  text_layer_set_text(s_answer_layer, ANSWERS[s_current]);
  text_layer_set_text(s_prompt_layer, "");
  layer_mark_dirty(s_canvas);

  // Vibrate
  static const uint32_t segments[] = {80, 40, 80};
  VibePattern pat = { .durations = segments, .num_segments = 3 };
  vibes_enqueue_custom_pattern(pat);
}

static void reset_ball(void) {
  s_revealed = false;
  text_layer_set_text(s_answer_layer, "");
  text_layer_set_text(s_prompt_layer, "Shake me\nor press Select");
  layer_mark_dirty(s_canvas);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  int cy = bounds.size.h / 2 - 10;

  // Draw the 8-ball circle
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(cx, cy), 60);

  if (s_revealed) {
    // Inner triangle
    GPoint tri[3] = {
      GPoint(cx, cy - 35),
      GPoint(cx - 30, cy + 20),
      GPoint(cx + 30, cy + 20),
    };

    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
    GPathInfo path_info = {
      .num_points = 3,
      .points = tri,
    };
    GPath *path = gpath_create(&path_info);
    gpath_draw_filled(ctx, path);
    gpath_destroy(path);
  } else {
    // Draw the "8"
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "8",
                       fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                       GRect(cx - 20, cy - 28, 40, 50),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  reveal_answer();
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_revealed) {
    reset_ball();
  } else {
    reveal_answer();
  }
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  reveal_answer();
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  reset_ball();
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, PBL_IF_COLOR_ELSE(GColorDarkCandyAppleRed, GColorWhite));

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  int cx = bounds.size.w / 2;
  int cy = bounds.size.h / 2 - 10;

  // Answer text (inside the triangle area)
  s_answer_layer = text_layer_create(GRect(cx - 40, cy - 25, 80, 50));
  text_layer_set_background_color(s_answer_layer, GColorClear);
  text_layer_set_text_color(s_answer_layer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  text_layer_set_font(s_answer_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_answer_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_answer_layer));

  // Prompt text at bottom
  s_prompt_layer = text_layer_create(GRect(0, bounds.size.h - 30, bounds.size.w, 30));
  text_layer_set_background_color(s_prompt_layer, GColorClear);
  text_layer_set_text_color(s_prompt_layer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  text_layer_set_font(s_prompt_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_prompt_layer, GTextAlignmentCenter);
  text_layer_set_text(s_prompt_layer, "Shake me\nor press Select");
  layer_add_child(root, text_layer_get_layer(s_prompt_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_answer_layer);
  text_layer_destroy(s_prompt_layer);
  layer_destroy(s_canvas);
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
