#include <pebble.h>

// ---------------------------------------------------------------------------
// Grid constants
// ---------------------------------------------------------------------------
#define CELL_SIZE 4
#define GRID_W    36
#define GRID_H    42

// ---------------------------------------------------------------------------
// Speed settings (ms per frame)
// ---------------------------------------------------------------------------
#define SPEED_SLOW   200
#define SPEED_NORMAL 100
#define SPEED_FAST    50
#define SPEED_COUNT    3

static const int s_speed_values[SPEED_COUNT] = { SPEED_SLOW, SPEED_NORMAL, SPEED_FAST };
static const char *s_speed_names[SPEED_COUNT] = { "Slow", "Normal", "Fast" };
static int s_speed_idx = 1;  // start at Normal

// ---------------------------------------------------------------------------
// Population graph
// ---------------------------------------------------------------------------
#define POP_HISTORY 60
#define POP_GRAPH_H  8

static uint16_t s_pop_history[POP_HISTORY];
static int s_pop_idx = 0;
static bool s_pop_wrapped = false;

// ---------------------------------------------------------------------------
// Seed-pattern cycling
// ---------------------------------------------------------------------------
#define PATTERN_COUNT 4
static int s_pattern_idx = 0;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static Window    *s_window;
static Layer     *s_canvas;
static AppTimer  *s_timer;
static bool       s_running = true;
static int        s_generation = 0;

static uint8_t s_grid[GRID_H][GRID_W];
static uint8_t s_next[GRID_H][GRID_W];
static uint8_t s_cell_age[GRID_H][GRID_W];  // age tracking for color heat map

static TextLayer *s_gen_layer;
static char       s_gen_buf[32];

// BT disconnect alert
static bool s_bt_connected = true;

// First-launch overlay
static bool s_first_launch = true;
static AppTimer *s_overlay_timer = NULL;

// Speed indicator flash
static bool s_show_speed = false;
static AppTimer *s_speed_timer = NULL;

// ===== Utility helpers =====================================================

static void randomize_grid(void) {
  for (int y = 0; y < GRID_H; y++) {
    for (int x = 0; x < GRID_W; x++) {
      s_grid[y][x] = (rand() % 100) < 30 ? 1 : 0;
      s_cell_age[y][x] = 0;
    }
  }
  s_generation = 0;
  s_pop_idx = 0;
  s_pop_wrapped = false;
  memset(s_pop_history, 0, sizeof(s_pop_history));
}

static void clear_grid(void) {
  memset(s_grid, 0, sizeof(s_grid));
  memset(s_cell_age, 0, sizeof(s_cell_age));
  s_generation = 0;
  s_pop_idx = 0;
  s_pop_wrapped = false;
  memset(s_pop_history, 0, sizeof(s_pop_history));
}

// Place a list of (x,y) offsets relative to (ox,oy) into the grid
static void place_pattern(const uint8_t (*cells)[2], int count, int ox, int oy) {
  clear_grid();
  for (int i = 0; i < count; i++) {
    int x = ox + cells[i][0];
    int y = oy + cells[i][1];
    if (x >= 0 && x < GRID_W && y >= 0 && y < GRID_H) {
      s_grid[y][x] = 1;
    }
  }
}

// ===== Seed patterns =======================================================

// Gosper glider gun (fits in top-left)
static void seed_glider_gun(void) {
  clear_grid();
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

// R-pentomino: classic chaos generator
static void seed_r_pentomino(void) {
  static const uint8_t cells[][2] = {
    {1,0},{2,0},{0,1},{1,1},{1,2}
  };
  place_pattern(cells, sizeof(cells)/sizeof(cells[0]),
                GRID_W/2 - 1, GRID_H/2 - 1);
}

// Pulsar: period-3 oscillator (symmetric, quarter specified and mirrored)
static void seed_pulsar(void) {
  clear_grid();
  // Quarter-pattern offsets from center; we mirror across both axes
  static const int8_t qx[] = { 2, 3, 4,  6, 6, 6,  2, 3, 4 };
  static const int8_t qy[] = { 1, 1, 1,  2, 3, 4,  6, 6, 6 };
  int cx = GRID_W / 2;
  int cy = GRID_H / 2;
  int qn = sizeof(qx) / sizeof(qx[0]);
  for (int i = 0; i < qn; i++) {
    int signs[4][2] = {{1,1},{-1,1},{1,-1},{-1,-1}};
    for (int s = 0; s < 4; s++) {
      int x = cx + qx[i] * signs[s][0];
      int y = cy + qy[i] * signs[s][1];
      if (x >= 0 && x < GRID_W && y >= 0 && y < GRID_H) {
        s_grid[y][x] = 1;
      }
    }
  }
}

// Lightweight spaceship (LWSS)
static void seed_lwss(void) {
  static const uint8_t cells[][2] = {
    {0,1},{0,3},{1,0},{2,0},{3,0},{3,1},{3,2},{2,3}
  };
  place_pattern(cells, sizeof(cells)/sizeof(cells[0]),
                GRID_W/2 - 2, GRID_H/2 - 2);
}

// Diehard: vanishes after 130 generations
static void seed_diehard(void) {
  static const uint8_t cells[][2] = {
    {6,0},{0,1},{1,1},{1,2},{5,2},{6,2},{7,2}
  };
  place_pattern(cells, sizeof(cells)/sizeof(cells[0]),
                GRID_W/2 - 3, GRID_H/2 - 1);
}

static void seed_pattern_by_index(int idx) {
  switch (idx) {
    case 0: seed_r_pentomino(); break;
    case 1: seed_pulsar();      break;
    case 2: seed_lwss();        break;
    case 3: seed_diehard();     break;
  }
}

// ===== Simulation ==========================================================

static int count_neighbors(int cx, int cy) {
  int count = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = (cx + dx + GRID_W) % GRID_W;
      int ny = (cy + dy + GRID_H) % GRID_H;
      count += s_grid[ny][nx];
    }
  }
  return count;
}

static void step_simulation(void) {
  uint16_t population = 0;

  for (int y = 0; y < GRID_H; y++) {
    for (int x = 0; x < GRID_W; x++) {
      int n = count_neighbors(x, y);
      if (s_grid[y][x]) {
        s_next[y][x] = (n == 2 || n == 3) ? 1 : 0;
      } else {
        s_next[y][x] = (n == 3) ? 1 : 0;
      }

      // Update cell age
      if (s_next[y][x]) {
        if (s_grid[y][x]) {
          s_cell_age[y][x] = (s_cell_age[y][x] < 255) ? s_cell_age[y][x] + 1 : 255;
        } else {
          s_cell_age[y][x] = 0;  // newborn
        }
        population++;
      } else {
        s_cell_age[y][x] = 0;
      }
    }
  }

  memcpy(s_grid, s_next, sizeof(s_grid));
  s_generation++;

  // Record population for graph
  s_pop_history[s_pop_idx] = population;
  s_pop_idx++;
  if (s_pop_idx >= POP_HISTORY) {
    s_pop_idx = 0;
    s_pop_wrapped = true;
  }
}

// ===== Cell color by age (color platforms) ==================================

#ifdef PBL_COLOR
static GColor color_for_age(uint8_t age) {
  if (age == 0)       return GColorGreen;        // newborn = bright green
  if (age <= 5)       return GColorYellow;        // young = yellow
  if (age <= 10)      return GColorOrange;        // mature = orange
  return GColorRed;                               // elder = red
}
#endif

// ===== Drawing =============================================================

static void draw_population_graph(GContext *ctx, GRect bounds) {
  int graph_y = bounds.size.h - POP_GRAPH_H;

  // Find max population for scaling
  uint16_t max_pop = 1;
  int count = s_pop_wrapped ? POP_HISTORY : s_pop_idx;
  for (int i = 0; i < count; i++) {
    if (s_pop_history[i] > max_pop) max_pop = s_pop_history[i];
  }

  // Draw graph background (subtle dark bar)
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, graph_y, bounds.size.w, POP_GRAPH_H), 0, GCornerNone);

  // Draw population bars
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));

  int draw_count = s_pop_wrapped ? POP_HISTORY : s_pop_idx;
  // Each bar: spread across screen width
  int bar_w = bounds.size.w / POP_HISTORY;
  if (bar_w < 1) bar_w = 1;

  for (int i = 0; i < draw_count; i++) {
    // Index into circular buffer: oldest first
    int idx;
    if (s_pop_wrapped) {
      idx = (s_pop_idx + i) % POP_HISTORY;
    } else {
      idx = i;
    }
    int h = (int)((uint32_t)s_pop_history[idx] * (POP_GRAPH_H - 1) / max_pop);
    if (h < 1 && s_pop_history[idx] > 0) h = 1;
    int x_pos = i * bar_w;
    graphics_draw_line(ctx,
      GPoint(x_pos, graph_y + POP_GRAPH_H - 1),
      GPoint(x_pos, graph_y + POP_GRAPH_H - 1 - h));
  }
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Black background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw cells
  for (int y = 0; y < GRID_H; y++) {
    for (int x = 0; x < GRID_W; x++) {
      if (s_grid[y][x]) {
#ifdef PBL_COLOR
        graphics_context_set_fill_color(ctx, color_for_age(s_cell_age[y][x]));
#else
        graphics_context_set_fill_color(ctx, GColorWhite);
#endif
        graphics_fill_rect(ctx,
          GRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE - 1, CELL_SIZE - 1),
          0, GCornerNone);
      }
    }
  }

  // Population graph at bottom
  draw_population_graph(ctx, bounds);

  // Update generation counter text
  snprintf(s_gen_buf, sizeof(s_gen_buf), "Gen: %d", s_generation);
  text_layer_set_text(s_gen_layer, s_gen_buf);

  // BT disconnect warning
  if (!s_bt_connected) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_draw_text(ctx, "BT!",
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, 0, bounds.size.w, 20),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft,
      NULL);
  }

  // Speed indicator (shown briefly when changed)
  if (s_show_speed) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));
    graphics_draw_text(ctx,
      s_speed_names[s_speed_idx],
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, bounds.size.h / 2 - 12, bounds.size.w, 24),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
  }

  // First-launch overlay
  if (s_first_launch) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
    graphics_draw_text(ctx,
      "UP:Random\nSEL:Pause\nDN:Speed\nHold UP:Pattern\nHold DN:Gun",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(4, bounds.size.h / 2 - 40, bounds.size.w - 8, 80),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
  }
}

// ===== Game loop ===========================================================

static void game_loop(void *data) {
  if (s_running) {
    step_simulation();
    layer_mark_dirty(s_canvas);
  }
  s_timer = app_timer_register(s_speed_values[s_speed_idx], game_loop, NULL);
}

// ===== Button handlers =====================================================

static void select_click(ClickRecognizerRef recognizer, void *context) {
  s_running = !s_running;
}

// Short-press Up: randomize
static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_first_launch) {
    s_first_launch = false;
  }
  randomize_grid();
  layer_mark_dirty(s_canvas);
}

// Long-press Up: cycle seed patterns
static void up_long_click(ClickRecognizerRef recognizer, void *context) {
  if (s_first_launch) {
    s_first_launch = false;
  }
  seed_pattern_by_index(s_pattern_idx);
  s_pattern_idx = (s_pattern_idx + 1) % PATTERN_COUNT;
  layer_mark_dirty(s_canvas);
}

// Speed indicator hide callback
static void speed_timer_callback(void *data) {
  s_show_speed = false;
  s_speed_timer = NULL;
  layer_mark_dirty(s_canvas);
}

// Short-press Down: cycle speed
static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_first_launch) {
    s_first_launch = false;
  }
  s_speed_idx = (s_speed_idx + 1) % SPEED_COUNT;

  // Show speed indicator briefly
  s_show_speed = true;
  if (s_speed_timer) {
    app_timer_cancel(s_speed_timer);
  }
  s_speed_timer = app_timer_register(1000, speed_timer_callback, NULL);

  layer_mark_dirty(s_canvas);
}

// Long-press Down: glider gun
static void down_long_click(ClickRecognizerRef recognizer, void *context) {
  if (s_first_launch) {
    s_first_launch = false;
  }
  seed_glider_gun();
  layer_mark_dirty(s_canvas);
}

// Dismiss first-launch overlay on any first interaction
static void overlay_dismiss_timer(void *data) {
  if (s_first_launch) {
    s_first_launch = false;
    layer_mark_dirty(s_canvas);
  }
  s_overlay_timer = NULL;
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_long_click_subscribe(BUTTON_ID_UP, 500, up_long_click, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 500, down_long_click, NULL);
}

// ===== Bluetooth handler ===================================================

static void bt_handler(bool connected) {
  s_bt_connected = connected;
  if (!connected) {
    vibes_double_pulse();
  }
  layer_mark_dirty(s_canvas);
}

// ===== Window handlers =====================================================

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Full-screen canvas (no status bar)
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  // Generation counter overlay (above population graph)
  int gen_y = bounds.size.h - POP_GRAPH_H - 16;
  s_gen_layer = text_layer_create(GRect(0, gen_y, bounds.size.w, 16));
  text_layer_set_background_color(s_gen_layer, GColorClear);
  text_layer_set_text_color(s_gen_layer, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  text_layer_set_font(s_gen_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_gen_layer, GTextAlignmentRight);
  layer_add_child(root, text_layer_get_layer(s_gen_layer));

  randomize_grid();

  // Auto-dismiss overlay after 4 seconds
  s_overlay_timer = app_timer_register(4000, overlay_dismiss_timer, NULL);
}

static void window_unload(Window *window) {
  text_layer_destroy(s_gen_layer);
  layer_destroy(s_canvas);
}

// ===== Init / Deinit =======================================================

static void init(void) {
  srand(time(NULL));
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  // Subscribe to Bluetooth connection events
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler,
  });
  s_bt_connected = connection_service_peek_pebble_app_connection();

  s_timer = app_timer_register(s_speed_values[s_speed_idx], game_loop, NULL);
}

static void deinit(void) {
  connection_service_unsubscribe();
  if (s_timer) app_timer_cancel(s_timer);
  if (s_speed_timer) app_timer_cancel(s_speed_timer);
  if (s_overlay_timer) app_timer_cancel(s_overlay_timer);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
