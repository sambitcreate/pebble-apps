#include <pebble.h>
#include "../../shared/pebble_pastel.h"

// ---- Maze configuration ----
#define MAZE_COLS 12
#define MAZE_ROWS 14
#define CELL_W    12
#define CELL_H    12
#define NUM_MAZES 6

// Wall bits per cell: right, bottom (we only need 2 bits per cell)
// Bit 0 = right wall present, Bit 1 = bottom wall present
// Outer walls (left col, top row, right col, bottom row) are always drawn.

// Pre-made maze layouts: each byte stores wall bits for one cell.
// 0 = no walls, 1 = right wall, 2 = bottom wall, 3 = both walls.
static const uint8_t s_mazes[NUM_MAZES][MAZE_ROWS][MAZE_COLS] = {
  // Maze 0: serpentine horizontal
  {
    {0,0,0,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,2,2,2,2,2,2,2,0},
    {0,0,0,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,2,2,2,2,2,2,2,0},
    {0,0,0,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,2,2,2,2,2,2,2,0},
    {0,0,0,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,2,2,2,2,2,2,2,0},
    {0,0,0,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,2,2,2,2,2,2,2,0},
    {0,0,0,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,2,2,2,2,2,2,2,0},
    {0,0,0,0,0,0,0,0,0,0,0,2},
    {0,0,0,0,0,0,0,0,0,0,0,0},
  },
  // Maze 1: serpentine vertical
  {
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {0,1,0,1,0,1,0,1,0,1,0,1},
    {0,0,0,0,0,0,0,0,0,0,0,0},
  },
  // Maze 2: zigzag pattern
  {
    {0,0,0,0,0,2,0,0,0,0,0,2},
    {2,2,2,2,2,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,2,2,2,2,2,2},
    {0,0,0,0,0,2,0,0,0,0,0,0},
    {2,2,2,2,2,0,0,0,0,0,0,2},
    {0,0,0,0,0,0,2,2,2,2,2,0},
    {0,0,0,0,0,2,0,0,0,0,0,2},
    {2,2,2,2,2,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,2,2,2,2,2,2},
    {0,0,0,0,0,2,0,0,0,0,0,0},
    {2,2,2,2,2,0,0,0,0,0,0,2},
    {0,0,0,0,0,0,2,2,2,2,2,0},
    {0,0,0,0,0,2,0,0,0,0,0,2},
    {0,0,0,0,0,0,0,0,0,0,0,0},
  },
  // Maze 3: spiral inward
  {
    {0,0,0,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,0,2,0},
    {0,2,0,0,0,0,0,0,0,2,0,0},
    {0,0,2,0,0,0,0,0,2,0,0,2},
    {0,0,0,2,0,0,0,2,0,0,2,0},
    {0,0,0,0,2,0,2,0,0,2,0,0},
    {0,0,0,0,0,2,0,0,2,0,0,2},
    {0,0,0,0,2,0,0,2,0,0,2,0},
    {0,0,0,2,0,0,2,0,0,2,0,0},
    {0,0,2,0,0,2,0,0,2,0,0,2},
    {0,2,0,0,2,0,0,2,0,0,2,0},
    {2,0,0,2,0,0,2,0,0,2,0,0},
    {0,0,2,0,0,2,0,0,2,0,0,2},
    {0,0,0,0,0,0,0,0,0,0,0,0},
  },
  // Maze 4: rooms pattern
  {
    {0,1,0,0,0,1,0,0,0,1,0,2},
    {0,1,0,0,0,1,0,0,0,1,0,2},
    {2,2,0,2,2,2,0,2,2,2,0,2},
    {0,0,0,1,0,0,0,1,0,0,0,0},
    {0,0,0,1,0,0,0,1,0,0,0,2},
    {0,2,2,2,0,2,2,2,0,2,2,0},
    {0,1,0,0,0,1,0,0,0,1,0,2},
    {0,1,0,0,0,1,0,0,0,1,0,2},
    {2,2,0,2,2,2,0,2,2,2,0,2},
    {0,0,0,1,0,0,0,1,0,0,0,0},
    {0,0,0,1,0,0,0,1,0,0,0,2},
    {0,2,2,2,0,2,2,2,0,2,2,0},
    {0,1,0,0,0,1,0,0,0,1,0,2},
    {0,0,0,0,0,0,0,0,0,0,0,0},
  },
  // Maze 5: diamond weave
  {
    {0,2,0,0,0,2,0,0,0,2,0,2},
    {2,0,2,0,2,0,2,0,2,0,2,0},
    {0,2,0,2,0,0,0,2,0,0,0,2},
    {2,0,0,0,2,0,2,0,2,0,2,0},
    {0,2,0,2,0,2,0,0,0,2,0,2},
    {2,0,2,0,0,0,2,0,2,0,2,0},
    {0,0,0,2,0,2,0,2,0,0,0,2},
    {2,0,2,0,2,0,0,0,2,0,2,0},
    {0,2,0,0,0,2,0,2,0,2,0,2},
    {2,0,2,0,2,0,2,0,0,0,2,0},
    {0,0,0,2,0,0,0,2,0,2,0,2},
    {2,0,2,0,2,0,2,0,2,0,0,0},
    {0,2,0,0,0,2,0,0,0,2,0,2},
    {0,0,0,0,0,0,0,0,0,0,0,0},
  },
};

// ---- Path storage ----
// Maximum path length for solved path through the maze
#define MAX_PATH_LEN (MAZE_COLS * MAZE_ROWS)

typedef struct {
  uint8_t col;
  uint8_t row;
} PathCell;

static PathCell s_path[MAX_PATH_LEN];
static int s_path_len = 0;
static int s_current_maze = 0;

// ---- Procedural maze generation (DFS recursive backtracker) ----
static uint8_t s_gen_maze[MAZE_ROWS][MAZE_COLS];
static bool s_using_generated = false;

// Static DFS stack to avoid stack overflow
static uint8_t s_gen_stack_x[MAZE_ROWS * MAZE_COLS];
static uint8_t s_gen_stack_y[MAZE_ROWS * MAZE_COLS];

// Simple PRNG (xorshift32) for maze generation
static uint32_t s_rng_state;

static uint32_t xorshift32(void) {
  uint32_t x = s_rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s_rng_state = x;
  return x;
}

static void generate_maze(int hour, int day) {
  // Seed RNG from hour + day
  s_rng_state = (uint32_t)(hour + day * 24 + 1);
  // Warm up the RNG a few rounds
  for (int i = 0; i < 8; i++) xorshift32();

  // Initialize all cells with all walls (right + bottom = 3)
  for (int r = 0; r < MAZE_ROWS; r++) {
    for (int c = 0; c < MAZE_COLS; c++) {
      s_gen_maze[r][c] = 3;
    }
  }

  // Visited array
  bool visited[MAZE_ROWS][MAZE_COLS];
  memset(visited, 0, sizeof(visited));

  // Start DFS at (0, 0)
  int stack_top = 0;
  s_gen_stack_x[stack_top] = 0;
  s_gen_stack_y[stack_top] = 0;
  visited[0][0] = true;

  // Direction offsets: right, down, left, up
  static const int dx[] = {1, 0, -1, 0};
  static const int dy[] = {0, 1, 0, -1};

  while (stack_top >= 0) {
    int cx = s_gen_stack_x[stack_top];
    int cy = s_gen_stack_y[stack_top];

    // Collect unvisited neighbors
    int neighbors[4];
    int n_count = 0;
    for (int d = 0; d < 4; d++) {
      int nx = cx + dx[d];
      int ny = cy + dy[d];
      if (nx >= 0 && nx < MAZE_COLS && ny >= 0 && ny < MAZE_ROWS && !visited[ny][nx]) {
        neighbors[n_count++] = d;
      }
    }

    if (n_count == 0) {
      // Backtrack
      stack_top--;
    } else {
      // Pick a random unvisited neighbor
      int choice = (int)(xorshift32() % n_count);
      int d = neighbors[choice];
      int nx = cx + dx[d];
      int ny = cy + dy[d];

      // Carve wall between current cell and neighbor
      if (d == 0) {
        // Moving right: remove right wall of current cell
        s_gen_maze[cy][cx] &= ~1;
      } else if (d == 1) {
        // Moving down: remove bottom wall of current cell
        s_gen_maze[cy][cx] &= ~2;
      } else if (d == 2) {
        // Moving left: remove right wall of neighbor
        s_gen_maze[ny][nx] &= ~1;
      } else if (d == 3) {
        // Moving up: remove bottom wall of neighbor
        s_gen_maze[ny][nx] &= ~2;
      }

      visited[ny][nx] = true;
      stack_top++;
      s_gen_stack_x[stack_top] = (uint8_t)nx;
      s_gen_stack_y[stack_top] = (uint8_t)ny;
    }
  }
}

// ---- Get the active maze data ----
static const uint8_t (*get_active_maze(void))[MAZE_COLS] {
  if (s_using_generated) {
    return (const uint8_t (*)[MAZE_COLS])s_gen_maze;
  }
  return s_mazes[s_current_maze];
}

// ---- Animated path drawing ----
static int s_visible_path_len = 0;
static AppTimer *s_path_timer = NULL;

// ---- Pulsing dot ----
static int s_dot_pulse = 0; // 0-9, cycles

static int dot_radius(void) {
  // Oscillate between 3 and 5
  return 3 + (s_dot_pulse < 5 ? s_dot_pulse : 9 - s_dot_pulse) * 2 / 4;
}

// ---- Pebble globals ----
static Window *s_window;
static Layer *s_canvas_layer;

// Pastel theme
static PastelTheme s_theme;
static int s_theme_id = THEME_LAVENDER_DREAM;  // default for games

// ---- Check if we can move between cells in the active maze ----
// Returns true if there is NO wall between (c1,r1) and (c2,r2).
static bool can_move(int c1, int r1, int c2, int r2) {
  // Out of bounds
  if (c2 < 0 || c2 >= MAZE_COLS || r2 < 0 || r2 >= MAZE_ROWS) return false;

  int dc = c2 - c1;
  int dr = r2 - r1;

  const uint8_t (*maze)[MAZE_COLS] = get_active_maze();

  if (dc == 1 && dr == 0) {
    // Moving right: check right wall of (c1,r1)
    return !(maze[r1][c1] & 1);
  } else if (dc == -1 && dr == 0) {
    // Moving left: check right wall of (c2,r2)
    return !(maze[r2][c2] & 1);
  } else if (dr == 1 && dc == 0) {
    // Moving down: check bottom wall of (c1,r1)
    return !(maze[r1][c1] & 2);
  } else if (dr == -1 && dc == 0) {
    // Moving up: check bottom wall of (c2,r2)
    return !(maze[r2][c2] & 2);
  }
  return false;
}

// ---- Solve maze using BFS from (0,0) to (MAZE_COLS-1, MAZE_ROWS-1) ----
// Stores the solution path in s_path/s_path_len. Returns true if solved.
static bool solve_maze(void) {
  // BFS parent tracking: store linear index of parent, -1 for unvisited
  int16_t parent[MAZE_ROWS * MAZE_COLS];
  memset(parent, -1, sizeof(parent));

  // BFS queue (reuse a simple array)
  int16_t queue[MAZE_ROWS * MAZE_COLS];
  int head = 0, tail = 0;

  int start = 0; // (0,0)
  int goal = (MAZE_ROWS - 1) * MAZE_COLS + (MAZE_COLS - 1);

  parent[start] = start; // mark visited, self-parent
  queue[tail++] = start;

  // Direction offsets: right, down, left, up
  static const int dc[] = {1, 0, -1, 0};
  static const int dr[] = {0, 1, 0, -1};

  bool found = false;

  while (head < tail && !found) {
    int idx = queue[head++];
    int c = idx % MAZE_COLS;
    int r = idx / MAZE_COLS;

    for (int d = 0; d < 4; d++) {
      int nc = c + dc[d];
      int nr = r + dr[d];
      if (nc < 0 || nc >= MAZE_COLS || nr < 0 || nr >= MAZE_ROWS) continue;
      int nidx = nr * MAZE_COLS + nc;
      if (parent[nidx] != -1) continue; // already visited
      if (!can_move(c, r, nc, nr)) continue;

      parent[nidx] = idx;
      queue[tail++] = nidx;

      if (nidx == goal) {
        found = true;
        break;
      }
    }
  }

  // Reconstruct path
  s_path_len = 0;
  if (found) {
    // Trace back from goal
    int idx = goal;
    while (idx != start) {
      s_path[s_path_len].col = idx % MAZE_COLS;
      s_path[s_path_len].row = idx / MAZE_COLS;
      s_path_len++;
      idx = parent[idx];
    }
    s_path[s_path_len].col = 0;
    s_path[s_path_len].row = 0;
    s_path_len++;

    // Reverse the path so it goes start -> goal
    for (int i = 0; i < s_path_len / 2; i++) {
      PathCell tmp = s_path[i];
      s_path[i] = s_path[s_path_len - 1 - i];
      s_path[s_path_len - 1 - i] = tmp;
    }
  } else {
    // Fallback: if no solution, create a simple left-to-right, top-to-bottom scan
    for (int r = 0; r < MAZE_ROWS && s_path_len < MAX_PATH_LEN; r++) {
      if (r % 2 == 0) {
        for (int c = 0; c < MAZE_COLS && s_path_len < MAX_PATH_LEN; c++) {
          s_path[s_path_len].col = c;
          s_path[s_path_len].row = r;
          s_path_len++;
        }
      } else {
        for (int c = MAZE_COLS - 1; c >= 0 && s_path_len < MAX_PATH_LEN; c--) {
          s_path[s_path_len].col = c;
          s_path[s_path_len].row = r;
          s_path_len++;
        }
      }
    }
  }
  return found;
}

// ---- Forward declarations ----
static void path_draw_callback(void *data);

// ---- Start animated path drawing ----
static void start_path_animation(void) {
  s_visible_path_len = 0;
  if (s_path_timer) {
    app_timer_cancel(s_path_timer);
  }
  s_path_timer = app_timer_register(80, path_draw_callback, NULL);
}

// ---- Path animation timer callback ----
static void path_draw_callback(void *data) {
  if (s_visible_path_len < s_path_len) {
    s_visible_path_len++;
    s_dot_pulse = (s_dot_pulse + 1) % 10;
    layer_mark_dirty(s_canvas_layer);
    s_path_timer = app_timer_register(80, path_draw_callback, NULL);
  } else {
    s_path_timer = NULL;
  }
}

// ---- Select maze based on hour and solve it ----
static void update_maze(int hour, int day) {
  // Cancel any running path animation
  if (s_path_timer) {
    app_timer_cancel(s_path_timer);
    s_path_timer = NULL;
  }

  // Try procedural generation first
  generate_maze(hour, day);
  s_using_generated = true;
  bool solved = solve_maze();

  if (!solved) {
    // Fallback to pre-made maze 0
    s_using_generated = false;
    s_current_maze = 0;
    solve_maze();
  }

  // Start animated path drawing
  start_path_animation();
}

// ---- Drawing ----
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Get current time for dot position
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int minute = t->tm_min;

  // Calculate dot position along the path
  int dot_index = 0;
  if (s_path_len > 1) {
    dot_index = (minute * (s_path_len - 1)) / 59;
    if (dot_index >= s_path_len) dot_index = s_path_len - 1;
  }

  // Offset to center the maze on screen (use bounds instead of hardcoded values)
  int ox = (bounds.size.w - MAZE_COLS * CELL_W) / 2;
  int oy = (bounds.size.h - MAZE_ROWS * CELL_H) / 2;

  // Background: black
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Determine colors from theme
  GColor wall_color = s_theme.muted;
  GColor dot_color = s_theme.highlight;
  GColor trail_color = s_theme.accent;

  const uint8_t (*maze)[MAZE_COLS] = get_active_maze();

  // Determine how many trail cells to draw: use animated s_visible_path_len
  // but cap at the current dot_index position
  int trail_limit = s_visible_path_len;
  if (trail_limit > dot_index) trail_limit = dot_index;

  // Draw visited trail (cells along path up to animated visible length)
  graphics_context_set_fill_color(ctx, trail_color);
  for (int i = 0; i <= trail_limit && i < s_path_len; i++) {
    int cx = ox + s_path[i].col * CELL_W + CELL_W / 2;
    int cy = oy + s_path[i].row * CELL_H + CELL_H / 2;
    graphics_fill_circle(ctx, GPoint(cx, cy), PBL_IF_COLOR_ELSE(2, 1));
  }

  // Draw maze walls
  graphics_context_set_stroke_color(ctx, wall_color);
  graphics_context_set_stroke_width(ctx, 1);

  // Draw outer border
  GRect border = GRect(ox, oy, MAZE_COLS * CELL_W, MAZE_ROWS * CELL_H);
  graphics_draw_rect(ctx, border);

  // Draw interior walls
  for (int r = 0; r < MAZE_ROWS; r++) {
    for (int c = 0; c < MAZE_COLS; c++) {
      uint8_t walls = maze[r][c];
      int x = ox + c * CELL_W;
      int y = oy + r * CELL_H;

      // Right wall (only for interior: c < MAZE_COLS - 1)
      if ((walls & 1) && c < MAZE_COLS - 1) {
        graphics_draw_line(ctx,
          GPoint(x + CELL_W, y),
          GPoint(x + CELL_W, y + CELL_H));
      }
      // Bottom wall (only for interior: r < MAZE_ROWS - 1)
      if ((walls & 2) && r < MAZE_ROWS - 1) {
        graphics_draw_line(ctx,
          GPoint(x, y + CELL_H),
          GPoint(x + CELL_W, y + CELL_H));
      }
    }
  }

  // Draw the pulsing dot at current position
  if (s_path_len > 0) {
    int cx = ox + s_path[dot_index].col * CELL_W + CELL_W / 2;
    int cy = oy + s_path[dot_index].row * CELL_H + CELL_H / 2;

    graphics_context_set_fill_color(ctx, dot_color);
    graphics_fill_circle(ctx, GPoint(cx, cy), dot_radius());
  }

  // Draw time text at bottom-right with GOTHIC_18 font
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", t->tm_hour, t->tm_min);

  graphics_context_set_text_color(ctx, s_theme.primary);
  GRect text_rect = GRect(0, oy + MAZE_ROWS * CELL_H + 1, bounds.size.w, 22);
  graphics_draw_text(ctx, buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_18),
    text_rect,
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL);
}

// ---- AppMessage handlers ----
static void inbox_received(DictionaryIterator *iter, void *context) {
  (void)context;
  Tuple *theme_t = dict_find(iter, PASTEL_MSG_KEY_THEME);
  if (theme_t) {
    s_theme_id = theme_t->value->int32;
    persist_write_int(PASTEL_STORAGE_KEY_THEME, s_theme_id);
    s_theme = pastel_get_theme(s_theme_id);
    layer_mark_dirty(s_canvas_layer);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  (void)context;
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

// ---- Tick handler ----
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // If the hour changed, regenerate the maze
  if (units_changed & HOUR_UNIT) {
    update_maze(tick_time->tm_hour, tick_time->tm_yday);
  }

  // If the minute changed, restart path animation
  if ((units_changed & MINUTE_UNIT) && !(units_changed & HOUR_UNIT)) {
    start_path_animation();
  }

  // Increment pulse counter every second for pulsing dot effect
  s_dot_pulse = (s_dot_pulse + 1) % 10;

  // Always mark dirty to update dot pulse
  layer_mark_dirty(s_canvas_layer);
}

// ---- Window handlers ----
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  // Initialize maze with current hour
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  update_maze(t->tm_hour, t->tm_yday);
}

static void window_unload(Window *window) {
  if (s_path_timer) {
    app_timer_cancel(s_path_timer);
    s_path_timer = NULL;
  }
  layer_destroy(s_canvas_layer);
}

// ---- App lifecycle ----
static void init(void) {
  // Load persisted theme
  if (persist_exists(PASTEL_STORAGE_KEY_THEME)) {
    s_theme_id = persist_read_int(PASTEL_STORAGE_KEY_THEME);
  }
  s_theme = pastel_get_theme(s_theme_id);

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
