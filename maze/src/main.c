#include <pebble.h>

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

// ---- Pebble globals ----
static Window *s_window;
static Layer *s_canvas_layer;

// ---- Check if we can move between cells in the current maze ----
// Returns true if there is NO wall between (c1,r1) and (c2,r2).
static bool can_move(int c1, int r1, int c2, int r2) {
  // Out of bounds
  if (c2 < 0 || c2 >= MAZE_COLS || r2 < 0 || r2 >= MAZE_ROWS) return false;

  int dc = c2 - c1;
  int dr = r2 - r1;

  const uint8_t (*maze)[MAZE_COLS] = s_mazes[s_current_maze];

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
// Stores the solution path in s_path/s_path_len.
static void solve_maze(void) {
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
}

// ---- Select maze based on hour and solve it ----
static void update_maze(int hour) {
  s_current_maze = hour % NUM_MAZES;
  solve_maze();
}

// ---- Drawing ----
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  (void)bounds;

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

  // Offset to center the maze on screen
  int ox = (144 - MAZE_COLS * CELL_W) / 2;
  int oy = (168 - MAZE_ROWS * CELL_H) / 2;

  // Background: black
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Determine colors based on platform
#ifdef PBL_COLOR
  GColor wall_color = GColorDarkGray;
  GColor dot_color = GColorGreen;
  GColor trail_color = GColorDarkGreen;
#else
  GColor wall_color = GColorWhite;
  GColor dot_color = GColorWhite;
  GColor trail_color = GColorWhite;
#endif

  const uint8_t (*maze)[MAZE_COLS] = s_mazes[s_current_maze];

  // Draw visited trail (cells along path up to dot position)
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, trail_color);
  for (int i = 0; i <= dot_index && i < s_path_len; i++) {
    int cx = ox + s_path[i].col * CELL_W + CELL_W / 2;
    int cy = oy + s_path[i].row * CELL_H + CELL_H / 2;
    graphics_fill_circle(ctx, GPoint(cx, cy), 2);
  }
#else
  // On B&W, draw a subtle trail with small dots
  graphics_context_set_fill_color(ctx, trail_color);
  for (int i = 0; i <= dot_index && i < s_path_len; i++) {
    int cx = ox + s_path[i].col * CELL_W + CELL_W / 2;
    int cy = oy + s_path[i].row * CELL_H + CELL_H / 2;
    graphics_fill_circle(ctx, GPoint(cx, cy), 1);
  }
#endif

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

  // Draw the dot at current position
  if (s_path_len > 0) {
    int cx = ox + s_path[dot_index].col * CELL_W + CELL_W / 2;
    int cy = oy + s_path[dot_index].row * CELL_H + CELL_H / 2;

    graphics_context_set_fill_color(ctx, dot_color);
    graphics_fill_circle(ctx, GPoint(cx, cy), 4);
  }

  // Draw hour text at bottom center
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", t->tm_hour, t->tm_min);

  graphics_context_set_text_color(ctx, GColorWhite);
  GRect text_rect = GRect(0, oy + MAZE_ROWS * CELL_H + 1, 144, 20);
  graphics_draw_text(ctx, buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    text_rect,
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL);
}

// ---- Tick handler ----
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // If the hour changed, regenerate the maze
  if (units_changed & HOUR_UNIT) {
    update_maze(tick_time->tm_hour);
  }
  // Always mark dirty to update dot position
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
  update_maze(t->tm_hour);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

// ---- App lifecycle ----
static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
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
