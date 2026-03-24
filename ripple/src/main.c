#include <pebble.h>

static Window *s_window;
static Layer *s_canvas_layer;
static TextLayer *s_time_layer;

static AppTimer *s_pulse_timer;

static int s_hour_ring_radius;
static int s_minute_ring_radius;
static int s_pulse_radius;

// Current time state for ring styling
static int s_current_hour;
static int s_current_min;

// Pulse animation state
static int s_pulse_step;          // 0..20 (50ms * 20 = 1000ms = 1 second)
#define PULSE_STEPS 20
#define PULSE_MAX_RADIUS 65
#define PULSE_INTERVAL_MS 50

static char s_time_buf[8];
static char s_date_buf[8];        // "MAR 24" etc.

// ---- Time-of-day color lookup (color platforms) ----

#if defined(PBL_COLOR)
// Hour ring color cycles through the day:
// 0=cyan, 6=green, 12=yellow, 18=magenta
// We define 4 anchor colors and pick the nearest for each quarter.
static GColor hour_color_for_time(int hour) {
  // 0-2: cyan, 3-5: transition cyan->green, 6-8: green,
  // 9-11: transition green->yellow, 12-14: yellow,
  // 15-17: transition yellow->magenta, 18-20: magenta,
  // 21-23: transition magenta->cyan
  if (hour < 3) return GColorCyan;
  if (hour < 6) return GColorMediumSpringGreen;
  if (hour < 9) return GColorGreen;
  if (hour < 12) return GColorChromeYellow;
  if (hour < 15) return GColorYellow;
  if (hour < 18) return GColorOrange;
  if (hour < 21) return GColorMagenta;
  return GColorVividViolet;
}

// Minute ring gets a complementary shifted hue
static GColor minute_color_for_time(int hour) {
  // Offset by ~6 hours from hour ring for contrast
  if (hour < 3) return GColorMagenta;
  if (hour < 6) return GColorVividViolet;
  if (hour < 9) return GColorCyan;
  if (hour < 12) return GColorMediumSpringGreen;
  if (hour < 15) return GColorGreen;
  if (hour < 18) return GColorChromeYellow;
  if (hour < 21) return GColorYellow;
  return GColorOrange;
}
#endif

// ---- Drawing helpers ----

static void draw_circle_outline(GContext *ctx, GPoint center, int radius, int stroke_width) {
  if (radius <= 0) return;
  graphics_context_set_stroke_width(ctx, stroke_width);
  graphics_draw_circle(ctx, center, radius);
}

static void draw_crosshair(GContext *ctx, GPoint center) {
  // 4 short lines, each 4px long, from center
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, GPoint(center.x - 4, center.y), GPoint(center.x - 1, center.y));
  graphics_draw_line(ctx, GPoint(center.x + 1, center.y), GPoint(center.x + 4, center.y));
  graphics_draw_line(ctx, GPoint(center.x, center.y - 4), GPoint(center.x, center.y - 1));
  graphics_draw_line(ctx, GPoint(center.x, center.y + 1), GPoint(center.x, center.y + 4));
}

// ---- Canvas update ----

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  // Dark background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Ring thickness variation:
  // Hour ring: 1-3px, thicker later in day (based on 12h value)
  int hour12 = s_current_hour % 12;
  int hour_stroke = 1 + (hour12 * 2) / 11;  // 0->1, 5->1, 6->2, 11->3
  // Minute ring: 1-2px, thicker later in hour
  int min_stroke = 1 + (s_current_min >= 30 ? 1 : 0);

  // Ring 1 - Hours
#if defined(PBL_COLOR)
  graphics_context_set_stroke_color(ctx, hour_color_for_time(s_current_hour));
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
  draw_circle_outline(ctx, center, s_hour_ring_radius, hour_stroke);

  // Ring 2 - Minutes
#if defined(PBL_COLOR)
  graphics_context_set_stroke_color(ctx, minute_color_for_time(s_current_hour));
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
  draw_circle_outline(ctx, center, s_minute_ring_radius, min_stroke);

  // Ring 3 - Pulse (seconds)
#if defined(PBL_COLOR)
  graphics_context_set_stroke_color(ctx, GColorYellow);
#else
  graphics_context_set_stroke_color(ctx, GColorLightGray);
#endif
  draw_circle_outline(ctx, center, s_pulse_radius, 1);

  // Crosshair at center
  draw_crosshair(ctx, center);

  // Date text along the crosshair, left of center
  graphics_context_set_text_color(ctx, GColorWhite);
  GFont date_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  // Position: small text just left of the crosshair, vertically centered
  // We offset to the left so it sits beside the crosshair gap
  GRect date_rect = GRect(center.x - 46, center.y - 8, 38, 16);
  graphics_draw_text(ctx, s_date_buf, date_font, date_rect,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);
}

// ---- Radius mapping ----

// Map a value in [0, max_val) to [10, 60] px
static int map_radius(int value, int max_val) {
  if (max_val <= 0) return 10;
  return 10 + (value * 50) / (max_val - 1 > 0 ? max_val - 1 : 1);
}

// ---- Ease-out cubic ----

// Returns eased radius: max_radius * (1 - (1-t)^3) where t = step/PULSE_STEPS
// Uses integer math to avoid floating point.
static int ease_out_cubic_radius(int step, int max_steps, int max_radius) {
  // t = step / max_steps (conceptual, 0.0 to 1.0)
  // ease = 1 - (1-t)^3
  // (1-t) = (max_steps - step) / max_steps
  // (1-t)^3 = (max_steps - step)^3 / max_steps^3
  // ease = 1 - (max_steps - step)^3 / max_steps^3
  //      = (max_steps^3 - (max_steps - step)^3) / max_steps^3
  // radius = max_radius * ease
  int remaining = max_steps - step;
  int denom = max_steps * max_steps * max_steps;  // max_steps^3
  int numer_inv = remaining * remaining * remaining;  // (max_steps - step)^3
  // eased = max_radius * (denom - numer_inv) / denom
  return (max_radius * (denom - numer_inv)) / denom;
}

// ---- Pulse timer callback ----

static void pulse_timer_callback(void *data) {
  s_pulse_step++;
  if (s_pulse_step >= PULSE_STEPS) {
    s_pulse_step = 0;
  }

  // Ease-out cubic expansion from 0 to PULSE_MAX_RADIUS over one second
  s_pulse_radius = ease_out_cubic_radius(s_pulse_step, PULSE_STEPS, PULSE_MAX_RADIUS);

  // Mark canvas dirty to redraw
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }

  // Re-register timer
  s_pulse_timer = app_timer_register(PULSE_INTERVAL_MS, pulse_timer_callback, NULL);
}

// ---- Tick handler ----

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Store current time for ring styling
  s_current_hour = tick_time->tm_hour;
  s_current_min = tick_time->tm_min;

  // Update hour ring: use 12-hour value
  int hour12 = tick_time->tm_hour % 12;
  s_hour_ring_radius = map_radius(hour12, 12);

  // Update minute ring
  s_minute_ring_radius = map_radius(tick_time->tm_min, 60);

  // Reset pulse step at each new second so it stays in sync
  s_pulse_step = 0;
  s_pulse_radius = 0;

  // Update time text
  if (clock_is_24h_style()) {
    strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", tick_time);
  } else {
    strftime(s_time_buf, sizeof(s_time_buf), "%I:%M", tick_time);
  }
  text_layer_set_text(s_time_layer, s_time_buf);

  // Update date text (e.g. "MAR 24")
  strftime(s_date_buf, sizeof(s_date_buf), "%b %d", tick_time);
  // Uppercase the month abbreviation for clean look
  for (int i = 0; i < 3 && s_date_buf[i]; i++) {
    if (s_date_buf[i] >= 'a' && s_date_buf[i] <= 'z') {
      s_date_buf[i] -= 32;
    }
  }

  // Mark dirty (pulse timer also marks dirty, but ensure immediate update)
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

// ---- Window handlers ----

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Canvas layer fills entire window
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  // Time text layer at the very bottom
  GRect time_frame = GRect(0, bounds.size.h - 20, bounds.size.w, 20);
  s_time_layer = text_layer_create(time_frame);
  text_layer_set_background_color(s_time_layer, GColorClear);
#if defined(PBL_COLOR)
  text_layer_set_text_color(s_time_layer, GColorDarkGray);
#else
  text_layer_set_text_color(s_time_layer, GColorLightGray);
#endif
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  text_layer_set_text(s_time_layer, "00:00");
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // Initialize with current time
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  tick_handler(t, SECOND_UNIT);

  // Start pulse animation timer
  s_pulse_step = 0;
  s_pulse_radius = 0;
  s_pulse_timer = app_timer_register(PULSE_INTERVAL_MS, pulse_timer_callback, NULL);
}

static void window_unload(Window *window) {
  // Cancel pulse timer
  if (s_pulse_timer) {
    app_timer_cancel(s_pulse_timer);
    s_pulse_timer = NULL;
  }

  text_layer_destroy(s_time_layer);
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

  // Subscribe to second ticks for ring 1 and ring 2 updates
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
