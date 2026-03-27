#include <pebble.h>
#include "../../shared/pebble_pastel.h"

static PastelTheme s_theme;
static int s_theme_id = THEME_WARM_SUNSET;

// ── Constants ──────────────────────────────────────────────────────────
#define NUM_HOUR_DOTS      12
#define SWING_AMPLITUDE    30          // degrees
#define BOB_MIN_DIST       40          // px at minute 0
#define BOB_MAX_DIST       130         // px at minute 59
#define BOB_RADIUS         12          // increased from 8 to accommodate date text
#define HOUR_DOT_RADIUS    3
#define HOUR_DOT_ACTIVE_R  5
#define TRAIL_ARC_POINTS   12
#define TICK_MARK_LENGTH   5           // length of tick marks on swing arc
#define NUM_TICK_MARKS     4           // tick marks at 15-second intervals (0, 15, 30, 45)

// ── Globals ────────────────────────────────────────────────────────────
static Window    *s_window;
static Layer     *s_canvas;
static int        s_hour, s_minute, s_second, s_day;
static int        s_prev_hour;         // track hour changes for chime

// ── Helpers ────────────────────────────────────────────────────────────

// Convert degrees to Pebble trig-angle (0..65535 = 0..360 deg)
static int32_t deg_to_trig(int deg) {
  return (deg * TRIG_MAX_ANGLE) / 360;
}

// Compute the pendulum swing angle in trig-angle units with damped harmonic motion.
// Base: angle_deg = SWING_AMPLITUDE * sin(second * 2*pi / 60)
// Damped: apply cubic easing so the bob decelerates at endpoints
// and accelerates through center, giving more natural physics.
static int32_t pendulum_angle(int second) {
  // fraction of cycle: second / 60 mapped to 0..TRIG_MAX_ANGLE
  int32_t phase = (second * TRIG_MAX_ANGLE) / 60;
  int32_t sine  = sin_lookup(phase);   // -TRIG_MAX_RATIO .. +TRIG_MAX_RATIO

  // Apply cubic easing: sine^3 / TRIG_MAX_RATIO^2
  // This makes the motion slower at extremes (deceleration) and faster at center (acceleration)
  // sine^3 preserves the sign and squishes the waveform toward center
  int32_t sine_sq = (sine * sine) / TRIG_MAX_RATIO;  // always positive, range 0..TRIG_MAX_RATIO
  int32_t sine_cb = (sine_sq * sine) / TRIG_MAX_RATIO; // same sign as sine, range -TRIG_MAX_RATIO..+TRIG_MAX_RATIO

  // Blend: 60% cubic + 40% linear for a subtle but noticeable damped feel
  int32_t blended = (sine * 40 + sine_cb * 60) / 100;

  // result in trig-angle units
  return (SWING_AMPLITUDE * blended * (int32_t)TRIG_MAX_ANGLE) / (360 * (int32_t)TRIG_MAX_RATIO);
}

// ── Hour chime haptic ─────────────────────────────────────────────────
// Vibrate a distinctive pattern on hour change.
// Number of short pulses = (hour % 4) + 1
static void hour_chime(int hour) {
  int pulses = (hour % 4) + 1;
  // Build a vibe pattern: alternating ON/OFF durations
  // Each pulse is 80ms vibrate + 120ms pause
  static uint32_t segments[9]; // max 5 pulses = 9 segments (5 on + 4 off)
  int seg_count = 0;
  for (int i = 0; i < pulses; i++) {
    segments[seg_count++] = 80;   // vibrate duration
    if (i < pulses - 1) {
      segments[seg_count++] = 120; // pause between pulses
    }
  }
  VibePattern pattern = {
    .durations = segments,
    .num_segments = seg_count,
  };
  vibes_enqueue_custom_pattern(pattern);
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

    graphics_context_set_fill_color(ctx, is_current ? s_theme.highlight : s_theme.accent);
    if (is_current) {
      graphics_fill_circle(ctx, dot_pos, r);
    } else {
      // Draw outline for inactive dots
      graphics_context_set_stroke_color(ctx, s_theme.accent);
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

  // ── Tick marks at 15-second intervals on swing arc ────────────────
  graphics_context_set_stroke_color(ctx, s_theme.muted);
  graphics_context_set_stroke_width(ctx, 1);

  for (int i = 0; i < NUM_TICK_MARKS; i++) {
    int tick_sec = i * 15;  // 0, 15, 30, 45
    int32_t tick_swing = pendulum_angle(tick_sec);
    int32_t tick_trig  = (TRIG_MAX_ANGLE / 2) + tick_swing;

    // Inner point (closer to pivot)
    int inner_dist = bob_dist - TICK_MARK_LENGTH;
    int ix = pivot.x + (inner_dist * sin_lookup(tick_trig)) / TRIG_MAX_RATIO;
    int iy = pivot.y - (inner_dist * cos_lookup(tick_trig)) / TRIG_MAX_RATIO;

    // Outer point (farther from pivot)
    int outer_dist = bob_dist + TICK_MARK_LENGTH;
    int ox = pivot.x + (outer_dist * sin_lookup(tick_trig)) / TRIG_MAX_RATIO;
    int oy = pivot.y - (outer_dist * cos_lookup(tick_trig)) / TRIG_MAX_RATIO;

    graphics_draw_line(ctx, GPoint(ix, iy), GPoint(ox, oy));
  }

  // ── Trail arc (recent swing path) ───────────────────────────────
  graphics_context_set_stroke_color(ctx, s_theme.muted);
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
  graphics_context_set_stroke_color(ctx, s_theme.primary);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, pivot, bob);

  // ── Small pivot circle ───────────────────────────────────────────
  graphics_context_set_fill_color(ctx, s_theme.primary);
  graphics_fill_circle(ctx, pivot, 3);

  // ── Bob (filled circle) ──────────────────────────────────────────
  graphics_context_set_fill_color(ctx, s_theme.highlight);
  graphics_fill_circle(ctx, bob, BOB_RADIUS);

  // On B&W, add a ring to differentiate the bob from the pivot
#ifndef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, bob, BOB_RADIUS);
#endif

  // ── Date text inside the bob ─────────────────────────────────────
  // Render the day number (e.g., "24") centered inside the pendulum bob
  char day_str[4];
  snprintf(day_str, sizeof(day_str), "%d", s_day);

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  // Text bounding box centered on the bob
  GRect text_rect = GRect(bob.x - BOB_RADIUS, bob.y - 9, BOB_RADIUS * 2, 18);

#ifdef PBL_COLOR
  graphics_context_set_text_color(ctx, GColorBlack);
#else
  graphics_context_set_text_color(ctx, GColorBlack);
#endif
  graphics_draw_text(ctx, day_str, font, text_rect,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

// ── Tick handler ───────────────────────────────────────────────────────
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  int new_hour = tick_time->tm_hour;

  s_hour   = new_hour;
  s_minute = tick_time->tm_min;
  s_second = tick_time->tm_sec;
  s_day    = tick_time->tm_mday;

  // Hour chime: vibrate when the hour changes
  if ((units_changed & HOUR_UNIT) && new_hour != s_prev_hour) {
    hour_chime(new_hour);
    s_prev_hour = new_hour;
  }

  layer_mark_dirty(s_canvas);
}

// ── AppMessage handlers ──────────────────────────────────────────────
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *theme_t = dict_find(iter, PASTEL_MSG_KEY_THEME);
  if (theme_t) {
    s_theme_id = theme_t->value->int32;
    persist_write_int(PASTEL_STORAGE_KEY_THEME, s_theme_id);
    s_theme = pastel_get_theme(s_theme_id);
    if (s_canvas) layer_mark_dirty(s_canvas);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
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
  s_hour     = t->tm_hour;
  s_minute   = t->tm_min;
  s_second   = t->tm_sec;
  s_day      = t->tm_mday;
  s_prev_hour = t->tm_hour;
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

// ── App lifecycle ──────────────────────────────────────────────────────
static void init(void) {
  // Load saved theme
  if (persist_exists(PASTEL_STORAGE_KEY_THEME)) {
    s_theme_id = persist_read_int(PASTEL_STORAGE_KEY_THEME);
  }
  s_theme = pastel_get_theme(s_theme_id);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = window_load,
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
