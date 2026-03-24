#include <pebble.h>

// ---------------------------------------------------------------------------
// Tetromino Watchface
// Displays time as HH:MM using Tetris-style block digits on a grid background.
// ---------------------------------------------------------------------------

static Window    *s_main_window;
static Layer     *s_canvas_layer;
static TextLayer *s_date_layer;

static bool s_colon_visible = true;
static struct tm s_last_time;

// Block dimensions
#define BLOCK_SIZE   6
#define BLOCK_GAP    1
#define CELL_SIZE    (BLOCK_SIZE + BLOCK_GAP)  // 7
#define DIGIT_W      3
#define DIGIT_H      5
#define DIGIT_PX_W   (DIGIT_W * CELL_SIZE)     // 21
#define DIGIT_PX_H   (DIGIT_H * CELL_SIZE)     // 35

// Digit patterns: 3 wide x 5 tall, stored as 5 rows of 3 bits each.
// Bit layout per row: bit2=left, bit1=center, bit0=right
static const uint8_t DIGIT_PATTERNS[10][DIGIT_H] = {
  { 0x7, 0x5, 0x5, 0x5, 0x7 },  // 0: 111,101,101,101,111
  { 0x2, 0x6, 0x2, 0x2, 0x7 },  // 1: 010,110,010,010,111
  { 0x7, 0x1, 0x7, 0x4, 0x7 },  // 2: 111,001,111,100,111
  { 0x7, 0x1, 0x7, 0x1, 0x7 },  // 3: 111,001,111,001,111
  { 0x5, 0x5, 0x7, 0x1, 0x1 },  // 4: 101,101,111,001,001
  { 0x7, 0x4, 0x7, 0x1, 0x7 },  // 5: 111,100,111,001,111
  { 0x7, 0x4, 0x7, 0x5, 0x7 },  // 6: 111,100,111,101,111
  { 0x7, 0x1, 0x1, 0x1, 0x1 },  // 7: 111,001,001,001,001
  { 0x7, 0x5, 0x7, 0x5, 0x7 },  // 8: 111,101,111,101,111
  { 0x7, 0x5, 0x7, 0x1, 0x7 },  // 9: 111,101,111,001,111
};

// Classic Tetris piece colors (for color displays)
// 0=cyan, 1=yellow, 2=magenta, 3=green, 4=red, 5=orange, 6=blue
#ifdef PBL_COLOR
static GColor get_tetris_color(int color_index) {
  switch (color_index % 7) {
    case 0: return GColorCyan;
    case 1: return GColorYellow;
    case 2: return GColorMagenta;
    case 3: return GColorGreen;
    case 4: return GColorRed;
    case 5: return GColorOrange;
    case 6: return GColorBlue;
    default: return GColorWhite;
  }
}

static GColor get_highlight_color(int color_index) {
  switch (color_index % 7) {
    case 0: return GColorCeleste;
    case 1: return GColorPastelYellow;
    case 2: return GColorRichBrilliantLavender;
    case 3: return GColorMintGreen;
    case 4: return GColorMelon;
    case 5: return GColorRajah;
    case 6: return GColorVeryLightBlue;
    default: return GColorWhite;
  }
}

static GColor get_shadow_color(int color_index) {
  switch (color_index % 7) {
    case 0: return GColorTiffanyBlue;
    case 1: return GColorChromeYellow;
    case 2: return GColorPurple;
    case 3: return GColorIslamicGreen;
    case 4: return GColorDarkCandyAppleRed;
    case 5: return GColorWindsorTan;
    case 6: return GColorDukeBlue;
    default: return GColorLightGray;
  }
}
#endif

// Draw a single Tetris-style block with bevel effect
static void draw_block(GContext *ctx, int x, int y, int color_index) {
#ifdef PBL_COLOR
  // Fill with base color
  GColor base = get_tetris_color(color_index);
  GColor highlight = get_highlight_color(color_index);
  GColor shadow = get_shadow_color(color_index);

  // Draw main filled block
  graphics_context_set_fill_color(ctx, base);
  graphics_fill_rect(ctx, GRect(x, y, BLOCK_SIZE, BLOCK_SIZE), 0, GCornerNone);

  // Highlight: top edge and left edge (1px)
  graphics_context_set_stroke_color(ctx, highlight);
  graphics_draw_line(ctx, GPoint(x, y), GPoint(x + BLOCK_SIZE - 1, y));
  graphics_draw_line(ctx, GPoint(x, y), GPoint(x, y + BLOCK_SIZE - 1));

  // Shadow: bottom edge and right edge (1px)
  graphics_context_set_stroke_color(ctx, shadow);
  graphics_draw_line(ctx, GPoint(x, y + BLOCK_SIZE - 1), GPoint(x + BLOCK_SIZE - 1, y + BLOCK_SIZE - 1));
  graphics_draw_line(ctx, GPoint(x + BLOCK_SIZE - 1, y), GPoint(x + BLOCK_SIZE - 1, y + BLOCK_SIZE - 1));
#else
  // B&W: white block with dark border
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(x, y, BLOCK_SIZE, BLOCK_SIZE), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, GRect(x, y, BLOCK_SIZE, BLOCK_SIZE));
#endif
}

// Draw a digit (0-9) at the specified pixel position
static void draw_digit(GContext *ctx, int digit, int origin_x, int origin_y, int color_index) {
  if (digit < 0 || digit > 9) return;

  for (int row = 0; row < DIGIT_H; row++) {
    uint8_t pattern = DIGIT_PATTERNS[digit][row];
    for (int col = 0; col < DIGIT_W; col++) {
      // Check if this cell has a block
      // bit2=leftmost col (col 0), bit0=rightmost col (col 2)
      int bit = DIGIT_W - 1 - col;
      if (pattern & (1 << bit)) {
        int bx = origin_x + col * CELL_SIZE;
        int by = origin_y + row * CELL_SIZE;
        draw_block(ctx, bx, by, color_index);
      }
    }
  }
}

// Draw the colon (two vertically stacked blocks)
static void draw_colon(GContext *ctx, int center_x, int origin_y, int color_index) {
  if (!s_colon_visible) return;

  // Position two blocks: at rows 1 and 3 of the 5-row digit height
  int bx = center_x - BLOCK_SIZE / 2;
  int by1 = origin_y + 1 * CELL_SIZE + (CELL_SIZE - BLOCK_SIZE) / 2;
  int by2 = origin_y + 3 * CELL_SIZE + (CELL_SIZE - BLOCK_SIZE) / 2;

  draw_block(ctx, bx, by1, color_index);
  draw_block(ctx, bx, by2, color_index);
}

// Draw faint background grid
static void draw_grid(GContext *ctx, GRect bounds) {
#ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorFromHEX(0x111111));
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif

  // Vertical lines
  for (int x = 0; x < bounds.size.w; x += CELL_SIZE) {
    graphics_draw_line(ctx, GPoint(x, 0), GPoint(x, bounds.size.h - 1));
  }
  // Horizontal lines
  for (int y = 0; y < bounds.size.h; y += CELL_SIZE) {
    graphics_draw_line(ctx, GPoint(0, y), GPoint(bounds.size.w - 1, y));
  }
}

// Canvas update callback
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Black background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw grid
  draw_grid(ctx, bounds);

  // Layout calculation:
  // 4 digits + colon:  digit_w + gap + digit_w + colon_space + digit_w + gap + digit_w
  // digit_w = 21, gap between digits in same pair = 4, colon area = 14
  int digit_gap = 4;
  int colon_width = 14;
  int total_w = DIGIT_PX_W * 4 + digit_gap * 2 + colon_width;
  int start_x = (bounds.size.w - total_w) / 2;
  int start_y = (bounds.size.h - DIGIT_PX_H) / 2 - 10; // shift up a bit for date below

  // Extract time digits
  int h1 = s_last_time.tm_hour / 10;
  int h2 = s_last_time.tm_hour % 10;
  int m1 = s_last_time.tm_min / 10;
  int m2 = s_last_time.tm_min % 10;

  // Use 24h or 12h based on user preference
  if (!clock_is_24h_style()) {
    int hour12 = s_last_time.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    h1 = hour12 / 10;
    h2 = hour12 % 10;
  }

  // Each digit gets a different Tetris color
  int x = start_x;

  // Digit 1 (hour tens) -- skip leading zero on 12h
  if (h1 > 0) {
    draw_digit(ctx, h1, x, start_y, 0);
  }
  x += DIGIT_PX_W + digit_gap;

  // Digit 2 (hour ones)
  draw_digit(ctx, h2, x, start_y, 1);
  x += DIGIT_PX_W;

  // Colon
  int colon_center = x + colon_width / 2;
  draw_colon(ctx, colon_center, start_y, 2);
  x += colon_width;

  // Digit 3 (minute tens)
  draw_digit(ctx, m1, x, start_y, 3);
  x += DIGIT_PX_W + digit_gap;

  // Digit 4 (minute ones)
  draw_digit(ctx, m2, x, start_y, 5);
}

// Tick handler
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_last_time = *tick_time;

  if (units_changed & MINUTE_UNIT) {
    // Full redraw on minute change
    layer_mark_dirty(s_canvas_layer);

    // Update date text
    static char date_buf[16];
    strftime(date_buf, sizeof(date_buf), "%a", tick_time);
    text_layer_set_text(s_date_layer, date_buf);
  }

  if (units_changed & SECOND_UNIT) {
    // Toggle colon visibility each second
    s_colon_visible = (tick_time->tm_sec % 2 == 0);
    layer_mark_dirty(s_canvas_layer);
  }
}

// Window load
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Canvas layer for the time blocks and grid
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  // Date text layer at the bottom
  int date_h = 20;
  GRect date_rect = GRect(0, bounds.size.h - date_h - 4, bounds.size.w, date_h);
  s_date_layer = text_layer_create(date_rect);
  text_layer_set_background_color(s_date_layer, GColorClear);
#ifdef PBL_COLOR
  text_layer_set_text_color(s_date_layer, GColorLightGray);
#else
  text_layer_set_text_color(s_date_layer, GColorWhite);
#endif
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // Initialize with current time
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_last_time = *t;
  s_colon_visible = true;

  static char date_buf[16];
  strftime(date_buf, sizeof(date_buf), "%a", t);
  text_layer_set_text(s_date_layer, date_buf);
}

// Window unload
static void main_window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  text_layer_destroy(s_date_layer);
}

// Init
static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

// Deinit
static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
