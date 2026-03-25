#include <pebble.h>

// ── Constants ──────────────────────────────────────────────────────────────
#define NUM_BACKGROUND_STARS 35
#define NUM_HOUR_MARKERS     12
#define HOUR_MARKER_RADIUS   60
#define CENTER_X             72
#define CENTER_Y             78

// Shooting star animation
#define SHOOTING_STAR_FRAME_MS  66   // ~15 fps
#define SHOOTING_STAR_FRAMES    30   // 30 frames * 66ms ~ 2 seconds
#define SHOOTING_STAR_TRAIL_LEN  5   // trailing points
#define SHOOTING_STAR_ARC_DEG   60   // arc sweep in degrees

// Pole star (battery indicator) position: near top center
#define POLE_STAR_X  72
#define POLE_STAR_Y  10

// ── Globals ────────────────────────────────────────────────────────────────
static Window    *s_window;
static Layer     *s_canvas_layer;
static TextLayer *s_time_layer;

static struct tm s_current_time;

// Background star positions (fixed, seeded)
static GPoint s_bg_stars[NUM_BACKGROUND_STARS];
static int    s_bg_star_base_size[NUM_BACKGROUND_STARS]; // base size 1 or 2
static int    s_bg_star_brightness[NUM_BACKGROUND_STARS]; // 0=dim(1px), 1=normal(2px), 2=bright(3px)

// Hour marker positions (computed once)
static GPoint s_hour_pos[NUM_HOUR_MARKERS];

// Shooting star animation state
static AppTimer *s_shooting_timer = NULL;
static bool      s_shooting_active = false;
static int       s_shooting_frame = 0;
static int       s_shooting_start_angle = 0; // starting angle in degrees
// Trail points: index 0 = oldest, TRAIL_LEN-1 = head
static GPoint    s_shooting_trail[SHOOTING_STAR_TRAIL_LEN];
static int       s_shooting_trail_count = 0;

// Battery level for pole star
static int s_battery_pct = 100;

// Simple LCG PRNG so star positions are deterministic
static uint32_t s_seed = 12345;
static uint32_t prng_next(void) {
  s_seed = s_seed * 1103515245 + 12345;
  return (s_seed >> 16) & 0x7FFF;
}

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
    s_bg_star_base_size[i] = (prng_next() % 2 == 0) ? 1 : 2;
    s_bg_star_brightness[i] = 1; // start at normal
  }
}

static void init_hour_markers(void) {
  for (int i = 0; i < NUM_HOUR_MARKERS; i++) {
    int angle = i * 30; // 360/12
    s_hour_pos[i] = point_on_circle(CENTER_X, CENTER_Y, HOUR_MARKER_RADIUS, angle);
  }
}

// ── Shooting star animation ───────────────────────────────────────────────
static void shooting_star_timer_callback(void *data);

static void start_shooting_star(void) {
  if (s_shooting_active) return;

  // Start angle: random-ish based on current minute
  s_shooting_start_angle = (s_current_time.tm_min * 137 + s_current_time.tm_hour * 53) % 360;
  s_shooting_active = true;
  s_shooting_frame = 0;
  s_shooting_trail_count = 0;

  s_shooting_timer = app_timer_register(SHOOTING_STAR_FRAME_MS,
                                        shooting_star_timer_callback, NULL);
}

static void shooting_star_timer_callback(void *data) {
  (void)data;
  if (!s_shooting_active) return;

  s_shooting_frame++;

  // Compute current position along the arc
  // The star sweeps SHOOTING_STAR_ARC_DEG degrees over SHOOTING_STAR_FRAMES frames
  // at a radius that goes from inner (30) to outer edge (85)
  int progress = s_shooting_frame; // 1..SHOOTING_STAR_FRAMES
  int angle = s_shooting_start_angle + (SHOOTING_STAR_ARC_DEG * progress / SHOOTING_STAR_FRAMES);
  int radius = 30 + (55 * progress / SHOOTING_STAR_FRAMES); // 30 -> 85

  GPoint pos = point_on_circle(CENTER_X, CENTER_Y, radius, angle % 360);

  // Shift trail: drop oldest, append new head
  if (s_shooting_trail_count < SHOOTING_STAR_TRAIL_LEN) {
    s_shooting_trail[s_shooting_trail_count] = pos;
    s_shooting_trail_count++;
  } else {
    for (int i = 0; i < SHOOTING_STAR_TRAIL_LEN - 1; i++) {
      s_shooting_trail[i] = s_shooting_trail[i + 1];
    }
    s_shooting_trail[SHOOTING_STAR_TRAIL_LEN - 1] = pos;
  }

  layer_mark_dirty(s_canvas_layer);

  if (s_shooting_frame >= SHOOTING_STAR_FRAMES) {
    s_shooting_active = false;
    s_shooting_timer = NULL;
    // After animation ends, let the trail fade out over a few more frames
    // by not clearing trail_count -- it will just not be drawn after active=false
    // Actually, mark dirty one last time after a short delay to clear
    s_shooting_trail_count = 0;
    layer_mark_dirty(s_canvas_layer);
  } else {
    s_shooting_timer = app_timer_register(SHOOTING_STAR_FRAME_MS,
                                          shooting_star_timer_callback, NULL);
  }
}

// ── Drawing ────────────────────────────────────────────────────────────────
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  (void)bounds;

  int hour12 = s_current_time.tm_hour % 12;
  int minute = s_current_time.tm_min;
  int second = s_current_time.tm_sec;

  // Hand angles (0° = 12 o'clock, clockwise)
  int hour_angle   = hour12 * 30 + minute / 2;  // smooth sweep
  int minute_angle = minute * 6;
  int second_angle = second * 6;

  // Hand positions on the same circle as hour markers
  GPoint hour_pos   = point_on_circle(CENTER_X, CENTER_Y, HOUR_MARKER_RADIUS, hour_angle);
  GPoint minute_pos = point_on_circle(CENTER_X, CENTER_Y, HOUR_MARKER_RADIUS, minute_angle);
  GPoint second_pos = point_on_circle(CENTER_X, CENTER_Y, HOUR_MARKER_RADIUS, second_angle);

  // -- Black background --
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  // -- Background stars with multi-level brightness --
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < NUM_BACKGROUND_STARS; i++) {
    int brightness = s_bg_star_brightness[i]; // 0=dim, 1=normal, 2=bright
    int size = brightness + 1; // 1px, 2px, 3px
    if (size == 1) {
      graphics_fill_rect(ctx, GRect(s_bg_stars[i].x, s_bg_stars[i].y, 1, 1), 0, GCornerNone);
    } else {
      graphics_fill_circle(ctx, s_bg_stars[i], size - 1);
    }
  }

  // -- Pole star (battery indicator) --
  {
    // Size scales from 1 (0%) to 5 (100%) radius
    int pole_radius = 1 + (s_battery_pct * 4 / 100);
    if (pole_radius > 5) pole_radius = 5;
    if (pole_radius < 1) pole_radius = 1;
#ifdef PBL_COLOR
    // Brighter yellow at high battery, dim red at low
    if (s_battery_pct > 60) {
      graphics_context_set_fill_color(ctx, GColorYellow);
    } else if (s_battery_pct > 20) {
      graphics_context_set_fill_color(ctx, GColorChromeYellow);
    } else {
      graphics_context_set_fill_color(ctx, GColorRed);
    }
#else
    graphics_context_set_fill_color(ctx, GColorWhite);
#endif
    graphics_fill_circle(ctx, GPoint(POLE_STAR_X, POLE_STAR_Y), pole_radius);
  }

  // -- Hour markers (subtle reference dots) --
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < NUM_HOUR_MARKERS; i++) {
    graphics_fill_circle(ctx, s_hour_pos[i], 1);
  }

  // -- Constellation triangle connecting H, M, S --
#ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorCyan);
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, hour_pos, minute_pos);
  graphics_draw_line(ctx, minute_pos, second_pos);
  graphics_draw_line(ctx, hour_pos, second_pos);

  // -- Hand dots (drawn on top of lines) --
  // Second hand: smallest
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorYellow);
#else
  graphics_context_set_fill_color(ctx, GColorWhite);
#endif
  graphics_fill_circle(ctx, second_pos, 1);

  // Minute hand: medium
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorCyan);
#else
  graphics_context_set_fill_color(ctx, GColorWhite);
#endif
  graphics_fill_circle(ctx, minute_pos, 3);

  // Hour hand: largest
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorRed);
#else
  graphics_context_set_fill_color(ctx, GColorWhite);
#endif
  graphics_fill_circle(ctx, hour_pos, 4);

  // -- Animated shooting star with trail --
  if (s_shooting_active && s_shooting_trail_count > 0) {
    for (int i = 0; i < s_shooting_trail_count; i++) {
      // Fade from dim (oldest) to bright (newest)
      // i=0 is oldest, i=trail_count-1 is newest (head)
      int fade = i; // 0..trail_count-1
      int dot_radius = 1 + fade; // 1 for oldest trail, up to trail_count for head
      if (dot_radius > 3) dot_radius = 3;

#ifdef PBL_COLOR
      // Trail fades from dark yellow to bright white-yellow
      if (fade == s_shooting_trail_count - 1) {
        graphics_context_set_fill_color(ctx, GColorWhite);
      } else if (fade >= s_shooting_trail_count / 2) {
        graphics_context_set_fill_color(ctx, GColorYellow);
      } else {
        graphics_context_set_fill_color(ctx, GColorChromeYellow);
      }
#else
      graphics_context_set_fill_color(ctx, GColorWhite);
#endif
      graphics_fill_circle(ctx, s_shooting_trail[i], dot_radius);
    }

    // Draw connecting lines for the trail
    if (s_shooting_trail_count > 1) {
#ifdef PBL_COLOR
      graphics_context_set_stroke_color(ctx, GColorYellow);
#else
      graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
      graphics_context_set_stroke_width(ctx, 1);
      for (int i = 0; i < s_shooting_trail_count - 1; i++) {
        graphics_draw_line(ctx, s_shooting_trail[i], s_shooting_trail[i + 1]);
      }
    }
  }
}

// ── Battery handler ───────────────────────────────────────────────────────
static void battery_handler(BatteryChargeState charge) {
  s_battery_pct = charge.charge_percent;
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

// ── Time handling ──────────────────────────────────────────────────────────
static void update_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_current_time = *t;

  static char buf[8];
  strftime(buf, sizeof(buf), clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
  text_layer_set_text(s_time_layer, buf);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_current_time = *tick_time;

  // Multi-level twinkle: cycle brightness levels pseudo-randomly each second
  s_seed = tick_time->tm_sec * 7919 + tick_time->tm_min * 131;
  for (int i = 0; i < NUM_BACKGROUND_STARS; i++) {
    uint32_t r = prng_next();
    if ((r % 3) == 0) {
      // Cycle brightness: pick new random level
      uint32_t r2 = prng_next();
      s_bg_star_brightness[i] = (int)(r2 % 3); // 0=dim, 1=normal, 2=bright
    }
  }

  // Trigger shooting star at second 30
  if (tick_time->tm_sec == 30) {
    start_shooting_star();
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

  // Digital time text at the bottom
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

  // Get initial battery level
  BatteryChargeState bat = battery_state_service_peek();
  s_battery_pct = bat.charge_percent;

  update_time();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  layer_destroy(s_canvas_layer);
  s_canvas_layer = NULL;
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
  battery_state_service_subscribe(battery_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  if (s_shooting_timer) {
    app_timer_cancel(s_shooting_timer);
    s_shooting_timer = NULL;
  }
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
