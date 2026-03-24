#include <pebble.h>

#define CELL 6
#define GRID_W (144 / CELL)  // 24
#define GRID_H (156 / CELL)  // 26 (leave room for score bar)
#define MAX_SNAKE (GRID_W * GRID_H)
#define TICK_MS 200

#define STORAGE_KEY_HIGH 1

typedef enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT } Dir;

typedef struct { int8_t x, y; } Pos;

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_timer;

static Pos s_snake[MAX_SNAKE];
static int s_length;
static Dir s_dir;
static Pos s_food;
static int s_score;
static int s_high_score;
static bool s_game_over;
static bool s_paused;
static char s_score_buf[24];

static void place_food(void) {
  bool valid;
  do {
    valid = true;
    s_food.x = rand() % GRID_W;
    s_food.y = rand() % GRID_H;
    for (int i = 0; i < s_length; i++) {
      if (s_snake[i].x == s_food.x && s_snake[i].y == s_food.y) {
        valid = false;
        break;
      }
    }
  } while (!valid);
}

static void reset_game(void) {
  s_length = 3;
  s_dir = DIR_RIGHT;
  s_game_over = false;
  s_paused = false;
  s_score = 0;

  // Start in center
  for (int i = 0; i < s_length; i++) {
    s_snake[i].x = GRID_W / 2 - i;
    s_snake[i].y = GRID_H / 2;
  }

  place_food();
}

static void step(void) {
  if (s_game_over || s_paused) return;

  // Calculate new head
  Pos head = s_snake[0];
  switch (s_dir) {
    case DIR_UP:    head.y--; break;
    case DIR_DOWN:  head.y++; break;
    case DIR_LEFT:  head.x--; break;
    case DIR_RIGHT: head.x++; break;
  }

  // Wall collision
  if (head.x < 0 || head.x >= GRID_W || head.y < 0 || head.y >= GRID_H) {
    s_game_over = true;
    if (s_score > s_high_score) {
      s_high_score = s_score;
      persist_write_int(STORAGE_KEY_HIGH, s_high_score);
    }
    static const uint32_t segs[] = {300, 100, 300};
    VibePattern pat = { .durations = segs, .num_segments = 3 };
    vibes_enqueue_custom_pattern(pat);
    return;
  }

  // Self collision
  for (int i = 0; i < s_length; i++) {
    if (s_snake[i].x == head.x && s_snake[i].y == head.y) {
      s_game_over = true;
      if (s_score > s_high_score) {
        s_high_score = s_score;
        persist_write_int(STORAGE_KEY_HIGH, s_high_score);
      }
      vibes_long_pulse();
      return;
    }
  }

  // Check food
  bool ate = (head.x == s_food.x && head.y == s_food.y);

  // Move body
  if (!ate) {
    // Shift tail off
    for (int i = s_length - 1; i > 0; i--) {
      s_snake[i] = s_snake[i - 1];
    }
  } else {
    // Grow: shift everything, keep tail
    if (s_length < MAX_SNAKE) {
      for (int i = s_length; i > 0; i--) {
        s_snake[i] = s_snake[i - 1];
      }
      s_length++;
    }
    s_score++;
    place_food();
    vibes_short_pulse();
  }

  s_snake[0] = head;
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (s_game_over) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "GAME OVER",
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(0, 40, bounds.size.w, 30),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    snprintf(s_score_buf, sizeof(s_score_buf), "Score: %d  Hi: %d", s_score, s_high_score);
    graphics_draw_text(ctx, s_score_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(0, 75, bounds.size.w, 24),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    graphics_draw_text(ctx, "Select: Restart",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 105, bounds.size.w, 20),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    return;
  }

  // Draw food
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  graphics_fill_rect(ctx, GRect(s_food.x * CELL + 1, s_food.y * CELL + 1, CELL - 2, CELL - 2), 0, GCornerNone);

  // Draw snake
  for (int i = 0; i < s_length; i++) {
    if (i == 0) {
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
    } else {
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorWhite));
    }
    graphics_fill_rect(ctx, GRect(s_snake[i].x * CELL, s_snake[i].y * CELL, CELL - 1, CELL - 1), 0, GCornerNone);
  }

  // Score bar at bottom
  int bar_y = GRID_H * CELL;
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  graphics_fill_rect(ctx, GRect(0, bar_y, bounds.size.w, bounds.size.h - bar_y), 0, GCornerNone);

  snprintf(s_score_buf, sizeof(s_score_buf), "%d  Hi:%d", s_score, s_high_score);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  graphics_draw_text(ctx, s_score_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(4, bar_y - 1, bounds.size.w - 8, 14),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  if (s_paused) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "PAUSED",
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(0, 60, bounds.size.w, 30),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void game_loop(void *data) {
  step();
  layer_mark_dirty(s_canvas);
  s_timer = app_timer_register(TICK_MS, game_loop, NULL);
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  // Rotate direction counter-clockwise
  if (!s_game_over && !s_paused) {
    s_dir = (s_dir + 3) % 4;  // LEFT rotation
  }
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  // Rotate direction clockwise
  if (!s_game_over && !s_paused) {
    s_dir = (s_dir + 1) % 4;  // RIGHT rotation
  }
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_game_over) {
    reset_game();
    layer_mark_dirty(s_canvas);
  } else {
    s_paused = !s_paused;
  }
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  if (persist_exists(STORAGE_KEY_HIGH)) {
    s_high_score = persist_read_int(STORAGE_KEY_HIGH);
  }

  reset_game();
}

static void window_unload(Window *window) {
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
  s_timer = app_timer_register(TICK_MS, game_loop, NULL);
}

static void deinit(void) {
  if (s_timer) app_timer_cancel(s_timer);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
