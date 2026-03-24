#include <pebble.h>

#define CELL_SIZE 4
#define GRID_W    36
#define GRID_H    42
#define FRAME_MS  100  // ~10 FPS

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_timer;
static bool s_running = true;
static int s_generation = 0;

// Double-buffered grids (bit-packed per row would save memory,
// but byte-per-cell is simpler and fits in 64KB)
static uint8_t s_grid[GRID_H][GRID_W];
static uint8_t s_next[GRID_H][GRID_W];

static TextLayer *s_gen_layer;
static char s_gen_buf[20];

static void randomize_grid(void) {
  for (int y = 0; y < GRID_H; y++) {
    for (int x = 0; x < GRID_W; x++) {
      s_grid[y][x] = (rand() % 100) < 30 ? 1 : 0;
    }
  }
  s_generation = 0;
}

static void clear_grid(void) {
  memset(s_grid, 0, sizeof(s_grid));
  s_generation = 0;
}

// Gosper glider gun (fits in top-left)
static void seed_glider_gun(void) {
  clear_grid();
  // Classic Gosper glider gun pattern
  static const uint8_t gun[][2] = {
    {1,5},{1,6},{2,5},{2,6},
    {11,5},{11,6},{11,7},{12,4},{12,8},{13,3},{13,9},{14,3},{14,9},
    {15,6},{16,4},{16,8},{17,5},{17,6},{17,7},{18,6},
    {21,3},{21,4},{21,5},{22,3},{22,4},{22,5},{23,2},{23,6},
    {25,1},{25,2},{25,6},{25,7},
    {35,3},{35,4},{36,3},{36,4},
  };

  int n = sizeof(gun) / sizeof(gun[0]);
  for (int i = 0; i < n; i++) {
    int x = gun[i][0];
    int y = gun[i][1];
    if (x < GRID_W && y < GRID_H) {
      s_grid[y][x] = 1;
    }
  }
}

static int count_neighbors(int cx, int cy) {
  int count = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = (cx + dx + GRID_W) % GRID_W;  // wrap
      int ny = (cy + dy + GRID_H) % GRID_H;
      count += s_grid[ny][nx];
    }
  }
  return count;
}

static void step_simulation(void) {
  for (int y = 0; y < GRID_H; y++) {
    for (int x = 0; x < GRID_W; x++) {
      int n = count_neighbors(x, y);
      if (s_grid[y][x]) {
        s_next[y][x] = (n == 2 || n == 3) ? 1 : 0;
      } else {
        s_next[y][x] = (n == 3) ? 1 : 0;
      }
    }
  }
  memcpy(s_grid, s_next, sizeof(s_grid));
  s_generation++;
}

static void canvas_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  GRect bounds = layer_get_bounds(layer);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_fill_color(ctx, GColorWhite);

  for (int y = 0; y < GRID_H; y++) {
    for (int x = 0; x < GRID_W; x++) {
      if (s_grid[y][x]) {
        graphics_fill_rect(ctx,
          GRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE - 1, CELL_SIZE - 1),
          0, GCornerNone);
      }
    }
  }

  // Update generation counter
  snprintf(s_gen_buf, sizeof(s_gen_buf), "Gen: %d", s_generation);
  text_layer_set_text(s_gen_layer, s_gen_buf);
}

static void game_loop(void *data) {
  if (s_running) {
    step_simulation();
    layer_mark_dirty(s_canvas);
  }
  s_timer = app_timer_register(FRAME_MS, game_loop, NULL);
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  s_running = !s_running;
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  randomize_grid();
  layer_mark_dirty(s_canvas);
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  seed_glider_gun();
  layer_mark_dirty(s_canvas);
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  // Generation counter overlay
  s_gen_layer = text_layer_create(GRect(0, bounds.size.h - 16, bounds.size.w, 16));
  text_layer_set_background_color(s_gen_layer, GColorClear);
  text_layer_set_text_color(s_gen_layer, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  text_layer_set_font(s_gen_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_gen_layer, GTextAlignmentRight);
  layer_add_child(root, text_layer_get_layer(s_gen_layer));

  randomize_grid();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_gen_layer);
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
  s_timer = app_timer_register(FRAME_MS, game_loop, NULL);
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
