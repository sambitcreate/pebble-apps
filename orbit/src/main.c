#include <pebble.h>

// --- Constants ---
#define TRAIL_LENGTH 5

#define HOUR_ORBIT_RADIUS   55
#define MINUTE_ORBIT_RADIUS 38
#define SECOND_ORBIT_RADIUS 20

#define HOUR_DOT_RADIUS   6
#define MINUTE_DOT_RADIUS 4
#define SECOND_DOT_RADIUS 3

#define CENTER_DOT_RADIUS 2

// --- Trail storage ---
typedef struct {
  GPoint positions[TRAIL_LENGTH];
  int count; // how many valid positions stored (up to TRAIL_LENGTH)
} Trail;

// --- Globals ---
static Window *s_window;
static Layer *s_canvas_layer;

static Trail s_hour_trail;
static Trail s_minute_trail;
static Trail s_second_trail;

static int s_hours, s_minutes, s_seconds;

// --- Helpers ---

// Push a new position onto the trail, shifting old ones out.
static void trail_push(Trail *trail, GPoint pt) {
  // Shift everything down; index 0 is the oldest
  for (int i = 0; i < TRAIL_LENGTH - 1; i++) {
    trail->positions[i] = trail->positions[i + 1];
  }
  trail->positions[TRAIL_LENGTH - 1] = pt;
  if (trail->count < TRAIL_LENGTH) {
    trail->count++;
  }
}

// Compute a point on a circular orbit around center.
// angle_deg: 0-360 degrees, 0 = top (12 o'clock), clockwise.
static GPoint orbit_point(GPoint center, int radius, int angle_deg) {
  // Pebble trig: sin_lookup/cos_lookup use 0-TRIG_MAX_ANGLE.
  // 0 angle = 3 o'clock in standard math. We want 0 = 12 o'clock.
  // So subtract 90 degrees: effective = angle_deg - 90.
  int adjusted = angle_deg - 90;
  int32_t trig_angle = DEG_TO_TRIGANGLE(adjusted);

  int x = center.x + (radius * cos_lookup(trig_angle) / TRIG_MAX_RATIO);
  int y = center.y + (radius * sin_lookup(trig_angle) / TRIG_MAX_RATIO);

  return GPoint(x, y);
}

// Draw a filled circle at a given point.
static void draw_dot(GContext *ctx, GPoint pt, int radius) {
  graphics_fill_circle(ctx, pt, radius);
}

// Draw orbit ring (faint path).
static void draw_orbit_ring(GContext *ctx, GPoint center, int radius) {
  graphics_draw_circle(ctx, center, radius);
}

// Draw trail for a given hand. On color, we vary opacity by drawing
// progressively smaller dots. On B&W, same approach with white dots.
static void draw_trail(GContext *ctx, Trail *trail, int base_radius, GColor color) {
  // Draw from oldest to newest (index 0 = oldest).
  // The trail starts at index (TRAIL_LENGTH - trail->count).
  int start = TRAIL_LENGTH - trail->count;

  for (int i = start; i < TRAIL_LENGTH - 1; i++) {
    // i goes from oldest stored to second-newest.
    // Compute a fade factor: oldest = smallest, newest = biggest.
    int age = TRAIL_LENGTH - 1 - i; // age: higher = older
    int r = base_radius - age;
    if (r < 1) r = 1;

    // On color platforms, use a dimmer shade for older trail dots.
#if defined(PBL_COLOR)
    // Reduce alpha-like effect by using darker shades for older dots.
    // We approximate fading by adjusting the color channels.
    if (age >= 3) {
      GColor dim = GColorDarkGray;
      graphics_context_set_fill_color(ctx, dim);
    } else if (age >= 2) {
      // Use a mid-gray tinted toward the hand color
      graphics_context_set_fill_color(ctx, GColorLightGray);
    } else {
      graphics_context_set_fill_color(ctx, color);
    }
#else
    if (age >= 3) {
      graphics_context_set_fill_color(ctx, GColorDarkGray);
    } else {
      graphics_context_set_fill_color(ctx, GColorWhite);
    }
#endif

    draw_dot(ctx, trail->positions[i], r);
  }
}

// --- Canvas update ---
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw orbit path rings (faint).
#if defined(PBL_COLOR)
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
#else
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
#endif
  graphics_context_set_stroke_width(ctx, 1);

  draw_orbit_ring(ctx, center, HOUR_ORBIT_RADIUS);
  draw_orbit_ring(ctx, center, MINUTE_ORBIT_RADIUS);
  draw_orbit_ring(ctx, center, SECOND_ORBIT_RADIUS);

  // Calculate angles from current time.
  // Hour: 12-hour cycle mapped to 360 degrees (includes fractional from minutes).
  int hour12 = s_hours % 12;
  int hour_angle = (hour12 * 30) + (s_minutes / 2); // 360/12=30 per hour, + minute contribution
  int minute_angle = s_minutes * 6; // 360/60 = 6 per minute
  int second_angle = s_seconds * 6; // 360/60 = 6 per second

  // Compute current positions.
  GPoint hour_pt   = orbit_point(center, HOUR_ORBIT_RADIUS, hour_angle);
  GPoint minute_pt = orbit_point(center, MINUTE_ORBIT_RADIUS, minute_angle);
  GPoint second_pt = orbit_point(center, SECOND_ORBIT_RADIUS, second_angle);

  // Draw trails.
  GColor hour_color, minute_color, second_color;
#if defined(PBL_COLOR)
  hour_color   = GColorCyan;
  minute_color = GColorMagenta;
  second_color = GColorYellow;
#else
  hour_color   = GColorWhite;
  minute_color = GColorWhite;
  second_color = GColorWhite;
#endif

  draw_trail(ctx, &s_hour_trail,   HOUR_DOT_RADIUS, hour_color);
  draw_trail(ctx, &s_minute_trail, MINUTE_DOT_RADIUS, minute_color);
  draw_trail(ctx, &s_second_trail, SECOND_DOT_RADIUS, second_color);

  // Draw main dots (current position, on top of trail).
  graphics_context_set_fill_color(ctx, hour_color);
  draw_dot(ctx, hour_pt, HOUR_DOT_RADIUS);

  graphics_context_set_fill_color(ctx, minute_color);
  draw_dot(ctx, minute_pt, MINUTE_DOT_RADIUS);

  graphics_context_set_fill_color(ctx, second_color);
  draw_dot(ctx, second_pt, SECOND_DOT_RADIUS);

  // Center dot.
  graphics_context_set_fill_color(ctx, GColorWhite);
  draw_dot(ctx, center, CENTER_DOT_RADIUS);
}

// --- Tick handler ---
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_hours   = tick_time->tm_hour;
  s_minutes = tick_time->tm_min;
  s_seconds = tick_time->tm_sec;

  // Compute current positions for trail tracking.
  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(window_layer);
  GPoint center = grect_center_point(&bounds);

  int hour12 = s_hours % 12;
  int hour_angle = (hour12 * 30) + (s_minutes / 2);
  int minute_angle = s_minutes * 6;
  int second_angle = s_seconds * 6;

  GPoint hour_pt   = orbit_point(center, HOUR_ORBIT_RADIUS, hour_angle);
  GPoint minute_pt = orbit_point(center, MINUTE_ORBIT_RADIUS, minute_angle);
  GPoint second_pt = orbit_point(center, SECOND_ORBIT_RADIUS, second_angle);

  trail_push(&s_hour_trail, hour_pt);
  trail_push(&s_minute_trail, minute_pt);
  trail_push(&s_second_trail, second_pt);

  layer_mark_dirty(s_canvas_layer);
}

// --- Window handlers ---
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  // Initialize trails to empty.
  memset(&s_hour_trail, 0, sizeof(s_hour_trail));
  memset(&s_minute_trail, 0, sizeof(s_minute_trail));
  memset(&s_second_trail, 0, sizeof(s_second_trail));

  // Seed with current time.
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_hours   = t->tm_hour;
  s_minutes = t->tm_min;
  s_seconds = t->tm_sec;

  // Pre-fill trails with current position so we don't start empty.
  GPoint center = grect_center_point(&bounds);
  int hour12 = s_hours % 12;
  int hour_angle = (hour12 * 30) + (s_minutes / 2);
  int minute_angle = s_minutes * 6;
  int second_angle = s_seconds * 6;

  GPoint hour_pt   = orbit_point(center, HOUR_ORBIT_RADIUS, hour_angle);
  GPoint minute_pt = orbit_point(center, MINUTE_ORBIT_RADIUS, minute_angle);
  GPoint second_pt = orbit_point(center, SECOND_ORBIT_RADIUS, second_angle);

  for (int i = 0; i < TRAIL_LENGTH; i++) {
    trail_push(&s_hour_trail, hour_pt);
    trail_push(&s_minute_trail, minute_pt);
    trail_push(&s_second_trail, second_pt);
  }
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

// --- App lifecycle ---
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
