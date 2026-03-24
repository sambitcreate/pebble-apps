#include <pebble.h>

// ── Constants ──────────────────────────────────────────────────────────
#define NUM_HOUR_DOTS      12
#define SWING_AMPLITUDE    30          // degrees
#define BOB_MIN_DIST       40          // px at minute 0
#define BOB_MAX_DIST       130         // px at minute 59
#define BOB_RADIUS         8
#define HOUR_DOT_RADIUS    3
#define HOUR_DOT_ACTIVE_R  5
#define TRAIL_ARC_POINTS   12

// ── Globals ────────────────────────────────────────────────────────────
static Window    *s_window;
static Layer     *s_canvas;
static int        s_hour, s_minute, s_second;

// ── Helpers ────────────────────────────────────────────────────────────

// Convert degrees to Pebble trig-angle (0..65535 = 0..360 deg)
static int32_t deg_to_trig(int deg) {
  return (deg * TRIG_MAX_ANGLE) / 360;
}

// Compute the pendulum swing angle in trig-angle units.
// angle_deg = SWING_AMPLITUDE * sin(second * 2*pi / 60)
static int32_t pendulum_angle(int second) {
  // fraction of cycle: second / 60 mapped to 0..TRIG_MAX_ANGLE
  int32_t phase = (second * TRIG_MAX_ANGLE) / 60;
  int32_t sine  = sin_lookup(phase);                       // -TRIG_MAX_RATIO .. +TRIG_MAX_RATIO
  // result in trig-angle units
  return (SWING_AMPLITUDE * sine * (int32_t)TRIG_MAX_ANGLE) / (360 * (int32_t)TRIG_MAX_RATIO);
}

// ── Drawing callback ───────────────────────────────────────────────────
static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;

  // Pivot at top-center
  GPoint pivot = GPoint(cx, 8);

  // ── Hour dots around the top ─────────────────────────────────────
  // Arrange 12 dots in a shallow arc across the top of the screen.
  int arc_radius = cx - 10;
  for (int i = 0; i < NUM_HOUR_DOTS; i++) {
    // Spread from -80 deg to +80 deg (nearly the full top width)
    // i=0 is leftmost, i=11 is rightmost, mapped evenly
    int32_t dot_angle_deg = -80 + (i * 160) / (NUM_HOUR_DOTS - 1);
    int32_t trig_angle = deg_to_trig(dot_angle_deg);
    int dx = (arc_radius * sin_lookup(trig_angle)) / TRIG_MAX_RATIO;
    int dy = (arc_radius * cos_lookup(trig_angle)) / TRIG_MAX_RATIO;
    // cos gives downward component since 0 deg = up in Pebble trig
    // We want dots to droop slightly from pivot
    GPoint dot_pos = GPoint(pivot.x + dx, pivot.y + dy);

    bool is_current = ((s_hour % 12) == i);
    int r = is_current ? HOUR_DOT_ACTIVE_R : HOUR_DOT_RADIUS;

#ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, is_current ? GColorMagenta : GColorMagenta);
#else
    graphics_context_set_fill_color(ctx, GColorWhite);
#endif
    if (is_current) {
      graphics_fill_circle(ctx, dot_pos, r);
    } else {
      // Draw outline for inactive dots
#ifdef PBL_COLOR
      graphics_context_set_stroke_color(ctx, GColorMagenta);
#else
      graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_circle(ctx, dot_pos, r);
    }
  }

  // ── Pendulum geometry ────────────────────────────────────────────
  int32_t swing = pendulum_angle(s_second);
  // Bob distance: maps minute 0..59 to BOB_MIN_DIST..BOB_MAX_DIST
  int bob_dist = BOB_MIN_DIST + ((BOB_MAX_DIST - BOB_MIN_DIST) * s_minute) / 59;

  // Bob position relative to pivot
  // In Pebble trig: angle 0 = up (12 o'clock). We want 0 = straight down.
  // Straight down = TRIG_MAX_ANGLE/2 (180 deg). Add swing offset.
  int32_t bob_trig = (TRIG_MAX_ANGLE / 2) + swing;
  int bob_x = pivot.x + (bob_dist * sin_lookup(bob_trig)) / TRIG_MAX_RATIO;
  int bob_y = pivot.y - (bob_dist * cos_lookup(bob_trig)) / TRIG_MAX_RATIO;
  GPoint bob = GPoint(bob_x, bob_y);

  // ── Trail arc (recent swing path) ───────────────────────────────
  // Draw a faint arc showing where the bob has been over the last few seconds.
#ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
  graphics_context_set_stroke_width(ctx, 1);

  {
    GPoint prev = GPointZero;
    bool have_prev = false;
    for (int t = 0; t < TRAIL_ARC_POINTS; t++) {
      // Trail spans from (second - 5) to second
      int trail_sec = ((s_second - 5 + t * 5 / (TRAIL_ARC_POINTS - 1)) % 60 + 60) % 60;
      int32_t trail_swing = pendulum_angle(trail_sec);
      int32_t trail_trig  = (TRIG_MAX_ANGLE / 2) + trail_swing;
      int tx = pivot.x + (bob_dist * sin_lookup(trail_trig)) / TRIG_MAX_RATIO;
      int ty = pivot.y - (bob_dist * cos_lookup(trail_trig)) / TRIG_MAX_RATIO;
      GPoint tp = GPoint(tx, ty);
      if (have_prev) {
        graphics_draw_line(ctx, prev, tp);
      }
      prev = tp;
      have_prev = true;
    }
  }

  // ── Pendulum line ────────────────────────────────────────────────
#ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorWhite);
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, pivot, bob);

  // ── Small pivot circle ───────────────────────────────────────────
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, pivot, 3);

  // ── Bob (filled circle) ──────────────────────────────────────────
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorCyan);
#else
  graphics_context_set_fill_color(ctx, GColorWhite);
#endif
  graphics_fill_circle(ctx, bob, BOB_RADIUS);

  // On B&W, add a ring to differentiate the bob from the pivot
#ifndef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, bob, BOB_RADIUS);
#endif
}

// ── Tick handler ───────────────────────────────────────────────────────
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_hour   = tick_time->tm_hour;
  s_minute = tick_time->tm_min;
  s_second = tick_time->tm_sec;
  layer_mark_dirty(s_canvas);
}

// ── Window callbacks ───────────────────────────────────────────────────
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  // Seed with current time
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_hour   = t->tm_hour;
  s_minute = t->tm_min;
  s_second = t->tm_sec;
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

// ── App lifecycle ──────────────────────────────────────────────────────
static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
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
