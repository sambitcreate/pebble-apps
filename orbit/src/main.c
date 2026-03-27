#include <pebble.h>
#include "../../shared/pebble_pastel.h"

static PastelTheme s_theme;
static int s_theme_id = THEME_WARM_SUNSET;

// --- Constants ---
#define TRAIL_LENGTH 8

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
static TextLayer *s_date_layer;

static Trail s_hour_trail;
static Trail s_minute_trail;
static Trail s_second_trail;

static int s_hours, s_minutes, s_seconds;

static bool s_bt_connected = true;
static char s_date_buf[16];

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

    // Fade trail dots: recent positions use hand color, older use muted
    if (age >= 3) {
      graphics_context_set_fill_color(ctx, s_theme.muted);
    } else if (age >= 2) {
      graphics_context_set_fill_color(ctx, s_theme.muted);
    } else {
      graphics_context_set_fill_color(ctx, color);
    }

    draw_dot(ctx, trail->positions[i], r);
  }
}

// Draw battery arc around the outermost orbit ring.
static void draw_battery_arc(GContext *ctx, GPoint center) {
  BatteryChargeState charge = battery_state_service_peek();
  int end_angle = charge.charge_percent * 360 / 100;
  GColor color;
#ifdef PBL_COLOR
  if (charge.charge_percent > 30) color = GColorGreen;
  else if (charge.charge_percent > 20) color = GColorYellow;
  else color = GColorRed;
#else
  color = GColorWhite;
#endif
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_arc(ctx,
      GRect(center.x - 62, center.y - 62, 124, 124),
      GOvalScaleModeFitCircle, 0, DEG_TO_TRIGANGLE(end_angle));
}

// --- Canvas update ---
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw battery arc (behind orbit rings).
  draw_battery_arc(ctx, center);

  // Draw orbit path rings (faint).
  graphics_context_set_stroke_color(ctx, s_theme.muted);
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
  GColor hour_color   = s_theme.accent;
  GColor minute_color = s_theme.secondary;
  GColor second_color = s_theme.highlight;

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

  // Center dot: highlight if BT disconnected, theme highlight otherwise.
  if (!s_bt_connected) {
    graphics_context_set_fill_color(ctx, s_theme.highlight);
  } else {
    graphics_context_set_fill_color(ctx, s_theme.highlight);
  }
  draw_dot(ctx, center, CENTER_DOT_RADIUS);

  // Draw "!" near center when BT disconnected.
  if (!s_bt_connected) {
    graphics_context_set_text_color(ctx, s_theme.highlight);
    graphics_draw_text(ctx, "!",
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(center.x + 4, center.y - 10, 14, 14),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// --- Bluetooth handler ---
static void bluetooth_handler(bool connected) {
  s_bt_connected = connected;
  if (!connected) {
    vibes_double_pulse();
  }
  layer_mark_dirty(s_canvas_layer);
}

// --- Battery handler ---
static void battery_handler(BatteryChargeState charge) {
  layer_mark_dirty(s_canvas_layer);
}

// --- Tick handler ---
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_hours   = tick_time->tm_hour;
  s_minutes = tick_time->tm_min;
  s_seconds = tick_time->tm_sec;

  // Update date text.
  static const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  snprintf(s_date_buf, sizeof(s_date_buf), "%s %d", days[tick_time->tm_wday], tick_time->tm_mday);
  text_layer_set_text(s_date_layer, s_date_buf);

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

// --- AppMessage handlers ---
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *theme_t = dict_find(iter, PASTEL_MSG_KEY_THEME);
  if (theme_t) {
    s_theme_id = theme_t->value->int32;
    persist_write_int(PASTEL_STORAGE_KEY_THEME, s_theme_id);
    s_theme = pastel_get_theme(s_theme_id);
    // Refresh text and canvas
    if (s_date_layer) text_layer_set_text_color(s_date_layer, s_theme.primary);
    if (s_canvas_layer) layer_mark_dirty(s_canvas_layer);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

// --- Window handlers ---
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  // Create date TextLayer at bottom of screen.
  int date_height = 22;
  s_date_layer = text_layer_create(GRect(0, bounds.size.h - date_height - 2, bounds.size.w, date_height));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, s_theme.primary);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

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

  // Set initial date text.
  static const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  snprintf(s_date_buf, sizeof(s_date_buf), "%s %d", days[t->tm_wday], t->tm_mday);
  text_layer_set_text(s_date_layer, s_date_buf);

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

  // Initialize BT state.
  s_bt_connected = connection_service_peek_pebble_app_connection();
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  text_layer_destroy(s_date_layer);
}

// --- App lifecycle ---
static void init(void) {
  // Load saved theme
  if (persist_exists(PASTEL_STORAGE_KEY_THEME)) {
    s_theme_id = persist_read_int(PASTEL_STORAGE_KEY_THEME);
  }
  s_theme = pastel_get_theme(s_theme_id);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_handler,
  });

  // Set up AppMessage for config
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(128, 64);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
