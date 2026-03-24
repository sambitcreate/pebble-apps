#include <pebble.h>

// ── Constants ──────────────────────────────────────────────────────────────
#define NUM_BACKGROUND_STARS 35
#define NUM_HOUR_MARKERS     12
#define HOUR_MARKER_RADIUS   60
#define CENTER_X             72
#define CENTER_Y             78
#define SHOOTING_STAR_LEN    12
#define SHOOTING_STAR_INNER   4

// ── Globals ────────────────────────────────────────────────────────────────
static Window    *s_window;
static Layer     *s_canvas_layer;
static TextLayer *s_time_layer;

static struct tm s_current_time;

// Background star positions (fixed, seeded)
static GPoint s_bg_stars[NUM_BACKGROUND_STARS];
static int    s_bg_star_size[NUM_BACKGROUND_STARS]; // 1 or 2 px radius
static bool   s_bg_star_visible[NUM_BACKGROUND_STARS];

// Hour marker positions (computed once)
static GPoint s_hour_pos[NUM_HOUR_MARKERS];

// Simple LCG PRNG so star positions are deterministic
static uint32_t s_seed = 12345;
static uint32_t prng_next(void) {
  s_seed = s_seed * 1103515245 + 12345;
  return (s_seed >> 16) & 0x7FFF;
}

// ── Constellation patterns ─────────────────────────────────────────────────
// Each hour has lines connecting hour-marker indices.
// Format: pairs of indices. -1 sentinel terminates.
static const int8_t s_constellation_lines[][9] = {
  /* h0  (12 o'clock) */ { 11, 0,  0, 1,  0, 3,  -1 },         // chevron + cross
  /* h1  */              {  0, 1,  1, 2,  1, 4,  -1 },
  /* h2  */              {  1, 2,  2, 3,  2, 5,  -1 },
  /* h3  */              {  2, 3,  3, 4,  3, 6,  -1 },
  /* h4  */              {  3, 4,  4, 5,  4, 7,  -1 },
  /* h5  */              {  4, 5,  5, 6,  5, 8,  -1 },
  /* h6  */              {  5, 6,  6, 7,  6, 9,  -1 },
  /* h7  */              {  6, 7,  7, 8,  7,10,  -1 },
  /* h8  */              {  7, 8,  8, 9,  8,11,  -1 },
  /* h9  */              {  8, 9,  9,10,  9, 0,  -1 },
  /* h10 */              {  9,10, 10,11, 10, 1,  -1 },
  /* h11 */              { 10,11, 11, 0, 11, 2,  -1 },
};

// ── Helpers ────────────────────────────────────────────────────────────────

// Compute a point on a circle. angle is 0-359 degrees (0 = 12 o'clock).
static GPoint point_on_circle(int cx, int cy, int radius, int angle_deg) {
  int32_t trig_angle = DEG_TO_TRIGANGLE(angle_deg);
  int x = cx + (sin_lookup(trig_angle) * radius / TRIG_MAX_RATIO);
  int y = cy - (cos_lookup(trig_angle) * radius / TRIG_MAX_RATIO);
  return GPoint(x, y);
}

// ── Initialization helpers ─────────────────────────────────────────────────
static void init_background_stars(GRect bounds) {
  s_seed = 42; // fixed seed
  for (int i = 0; i < NUM_BACKGROUND_STARS; i++) {
    s_bg_stars[i].x = prng_next() % bounds.size.w;
    s_bg_stars[i].y = prng_next() % bounds.size.h;
    s_bg_star_size[i] = (prng_next() % 2 == 0) ? 1 : 2;
    s_bg_star_visible[i] = true;
  }
}

static void init_hour_markers(void) {
  for (int i = 0; i < NUM_HOUR_MARKERS; i++) {
    int angle = i * 30; // 360/12
    s_hour_pos[i] = point_on_circle(CENTER_X, CENTER_Y, HOUR_MARKER_RADIUS, angle);
  }
}

// ── Drawing ────────────────────────────────────────────────────────────────
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  (void)bounds;

  int hour12 = s_current_time.tm_hour % 12;
  int minute = s_current_time.tm_min;

  // -- Black background --
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  // -- Background stars --
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < NUM_BACKGROUND_STARS; i++) {
    if (!s_bg_star_visible[i]) continue;
    if (s_bg_star_size[i] == 1) {
      graphics_fill_rect(ctx, GRect(s_bg_stars[i].x, s_bg_stars[i].y, 1, 1), 0, GCornerNone);
    } else {
      graphics_fill_rect(ctx, GRect(s_bg_stars[i].x, s_bg_stars[i].y, 2, 2), 0, GCornerNone);
    }
  }

  // -- Constellation lines for current hour --
#ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorCyan);
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
  graphics_context_set_stroke_width(ctx, 1);
  const int8_t *lines = s_constellation_lines[hour12];
  for (int j = 0; lines[j] != -1; j += 2) {
    int a = lines[j];
    int b = lines[j + 1];
    graphics_draw_line(ctx, s_hour_pos[a], s_hour_pos[b]);
  }

  // -- Hour markers --
  for (int i = 0; i < NUM_HOUR_MARKERS; i++) {
    if (i == hour12) {
      // Current hour: largest, red on color
#ifdef PBL_COLOR
      graphics_context_set_fill_color(ctx, GColorRed);
#else
      graphics_context_set_fill_color(ctx, GColorWhite);
#endif
      graphics_fill_circle(ctx, s_hour_pos[i], 4);
    } else {
      // Other hours: small dot
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_circle(ctx, s_hour_pos[i], 2);
    }
  }

  // -- Shooting star (minute indicator) --
  {
    int min_angle = minute * 6; // 360/60 = 6 degrees per minute
    // Outer tip
    GPoint tip = point_on_circle(CENTER_X, CENTER_Y, HOUR_MARKER_RADIUS + SHOOTING_STAR_LEN, min_angle);
    // Tail (closer to centre)
    GPoint tail = point_on_circle(CENTER_X, CENTER_Y, HOUR_MARKER_RADIUS + SHOOTING_STAR_INNER, min_angle);

#ifdef PBL_COLOR
    graphics_context_set_stroke_color(ctx, GColorYellow);
#else
    graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, tail, tip);

    // Small bright head
#ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, GColorYellow);
#else
    graphics_context_set_fill_color(ctx, GColorWhite);
#endif
    graphics_fill_circle(ctx, tip, 2);
  }
}

// ── Time handling ──────────────────────────────────────────────────────────
static void update_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_current_time = *t;

  // Update text
  static char buf[6];
  strftime(buf, sizeof(buf), clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
  text_layer_set_text(s_time_layer, buf);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_current_time = *tick_time;

  // Twinkle: randomly toggle ~20% of background stars each second
  s_seed = tick_time->tm_sec * 7919 + tick_time->tm_min * 131;
  for (int i = 0; i < NUM_BACKGROUND_STARS; i++) {
    uint32_t r = prng_next();
    if ((r % 5) == 0) {
      s_bg_star_visible[i] = !s_bg_star_visible[i];
    }
  }

  update_time();
  layer_mark_dirty(s_canvas_layer);
}

// ── Window lifecycle ───────────────────────────────────────────────────────
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Canvas layer (full screen)
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(root, s_canvas_layer);

  // Time text at the bottom
  s_time_layer = text_layer_create(GRect(0, bounds.size.h - 20, bounds.size.w, 20));
  text_layer_set_background_color(s_time_layer, GColorClear);
#ifdef PBL_COLOR
  text_layer_set_text_color(s_time_layer, GColorLightGray);
#else
  text_layer_set_text_color(s_time_layer, GColorWhite);
#endif
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // Initialise star data
  init_background_stars(bounds);
  init_hour_markers();
  update_time();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  layer_destroy(s_canvas_layer);
}

// ── App lifecycle ──────────────────────────────────────────────────────────
static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
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
