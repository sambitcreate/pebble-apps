#include <pebble.h>
#include "../../shared/pebble_pastel.h"

// ─── UI layers ───────────────────────────────────────────────────────────────
static PastelTheme s_theme;
static int s_theme_id = THEME_WARM_SUNSET;

static Window *s_window;
static StatusBarLayer *s_status_bar;
static Layer *s_canvas;
static TextLayer *s_heading_layer;
static TextLayer *s_cardinal_layer;
static TextLayer *s_status_layer;
static TextLayer *s_cal_instruction_layer;
static TextLayer *s_lock_layer;

// First-launch overlay
static Layer *s_overlay_layer;
static TextLayer *s_overlay_text;
static bool s_overlay_visible = true;

// ─── Compass state ───────────────────────────────────────────────────────────
static int s_display_heading = 0;   // smoothed heading shown on screen
static int s_target_heading = 0;    // raw magnetometer reading
static int s_velocity = 0;          // angular velocity
static bool s_heading_valid = false;
static CompassStatus s_cal_status = CompassStatusUnavailable;
static char s_heading_buf[16];

// Physics constants
#define FRICTION_DEFAULT 85  // 0-100, higher = more damping
#define SPRING   12          // spring constant
static int s_friction = FRICTION_DEFAULT;

// Persistent storage / AppMessage keys
#define STORAGE_KEY_FRICTION 1
#define MSG_KEY_NEEDLE_DAMPING 0

// ─── Heading lock ────────────────────────────────────────────────────────────
#define LOCK_THRESHOLD 2     // degrees
#define LOCK_COUNT_NEEDED 3  // consecutive readings within threshold
static int s_lock_counter = 0;
static int s_last_heading = -1;
static bool s_heading_locked = false;

// ─── Animation timer ─────────────────────────────────────────────────────────
static AppTimer *s_anim_timer = NULL;
#define ANIM_INTERVAL_MS 33  // ~30 fps

// ─── Bluetooth state ─────────────────────────────────────────────────────────
static bool s_bt_connected = true;

static const char *cardinal_direction(int heading) {
  if (heading >= 337 || heading < 23)  return "N";
  if (heading < 68)  return "NE";
  if (heading < 113) return "E";
  if (heading < 158) return "SE";
  if (heading < 203) return "S";
  if (heading < 248) return "SW";
  if (heading < 293) return "W";
  return "NW";
}

// Needle triangle path (north, red)
static GPoint s_needle_points[] = {
  {0, -50},
  {-8, 10},
  {8, 10},
};
static GPathInfo s_needle_info = {
  .num_points = 3,
  .points = s_needle_points,
};
static GPath *s_needle_path;

// South half of needle (gray)
static GPoint s_south_points[] = {
  {0, 50},
  {-6, -5},
  {6, -5},
};
static GPathInfo s_south_info = {
  .num_points = 3,
  .points = s_south_points,
};
static GPath *s_south_path;

// ─── Physics smoothing ──────────────────────────────────────────────────────
static void update_physics(void) {
  int delta = s_target_heading - s_display_heading;
  // Normalize to -180..180
  while (delta > 180) delta -= 360;
  while (delta < -180) delta += 360;
  s_velocity = (s_velocity * s_friction / 100) + (delta * SPRING / 100);
  s_display_heading = ((s_display_heading + s_velocity) % 360 + 360) % 360;
}

// ─── Animation timer callback ────────────────────────────────────────────────
static void anim_timer_callback(void *data) {
  if (s_heading_valid) {
    update_physics();

    snprintf(s_heading_buf, sizeof(s_heading_buf), "%d\u00B0", s_display_heading);
    text_layer_set_text(s_heading_layer, s_heading_buf);
    text_layer_set_text(s_cardinal_layer, cardinal_direction(s_display_heading));

    layer_mark_dirty(s_canvas);
  }
  s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_timer_callback, NULL);
}

// ─── Canvas drawing ──────────────────────────────────────────────────────────
static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  int cy = bounds.size.h / 2;
  GPoint center = GPoint(cx, cy);

  // Compass circle
  graphics_context_set_stroke_color(ctx, s_theme.muted);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, center, 55);

  if (!s_heading_valid) {
    // Draw calibration animation — rotating dashes
    for (int deg = 0; deg < 360; deg += 15) {
      int32_t angle = DEG_TO_TRIGANGLE(deg);
      int inner = 48;
      int outer = 54;
      GPoint p1 = {
        .x = cx + (sin_lookup(angle) * inner / TRIG_MAX_RATIO),
        .y = cy - (cos_lookup(angle) * inner / TRIG_MAX_RATIO),
      };
      GPoint p2 = {
        .x = cx + (sin_lookup(angle) * outer / TRIG_MAX_RATIO),
        .y = cy - (cos_lookup(angle) * outer / TRIG_MAX_RATIO),
      };
      graphics_context_set_stroke_color(ctx, s_theme.secondary);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, p1, p2);
    }

    // "Calibrating..." text in center (larger, bolder)
    graphics_context_set_text_color(ctx, s_theme.secondary);
    graphics_draw_text(ctx, "?",
                       fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                       GRect(cx - 20, cy - 30, 40, 50),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    // Show calibration label below the question mark
    graphics_draw_text(ctx, "Calibrating",
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(0, cy + 18, bounds.size.w, 28),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    return;
  }

  // Tick marks every 30 degrees (rotate with smoothed heading)
  for (int deg = 0; deg < 360; deg += 30) {
    int32_t angle = DEG_TO_TRIGANGLE(deg - s_display_heading);
    int inner = 48;
    int outer = 54;
    GPoint p1 = {
      .x = cx + (sin_lookup(angle) * inner / TRIG_MAX_RATIO),
      .y = cy - (cos_lookup(angle) * inner / TRIG_MAX_RATIO),
    };
    GPoint p2 = {
      .x = cx + (sin_lookup(angle) * outer / TRIG_MAX_RATIO),
      .y = cy - (cos_lookup(angle) * outer / TRIG_MAX_RATIO),
    };
    graphics_context_set_stroke_color(ctx, s_theme.primary);
    graphics_context_set_stroke_width(ctx, deg % 90 == 0 ? 3 : 1);
    graphics_draw_line(ctx, p1, p2);
  }

  // Cardinal labels on the dial (rotate with smoothed heading)
  const char *labels[] = {"N", "E", "S", "W"};
  const int label_degs[] = {0, 90, 180, 270};
  for (int i = 0; i < 4; i++) {
    int32_t angle = DEG_TO_TRIGANGLE(label_degs[i] - s_display_heading);
    int r = 40;
    int lx = cx + (sin_lookup(angle) * r / TRIG_MAX_RATIO) - 5;
    int ly = cy - (cos_lookup(angle) * r / TRIG_MAX_RATIO) - 8;

    GColor color = (i == 0) ? s_theme.highlight : s_theme.primary;
    graphics_context_set_text_color(ctx, color);
    graphics_draw_text(ctx, labels[i],
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(lx, ly, 12, 16),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }

  // Draw needle — north (red/white)
  gpath_move_to(s_needle_path, center);
  gpath_rotate_to(s_needle_path, 0);
  graphics_context_set_fill_color(ctx, s_theme.highlight);
  gpath_draw_filled(ctx, s_needle_path);

  // Draw needle — south (gray)
  gpath_move_to(s_south_path, center);
  gpath_rotate_to(s_south_path, 0);
  graphics_context_set_fill_color(ctx, s_theme.muted);
  gpath_draw_filled(ctx, s_south_path);

  // Center dot
  graphics_context_set_fill_color(ctx, s_theme.primary);
  graphics_fill_circle(ctx, center, 4);
}

// ─── Compass handler ─────────────────────────────────────────────────────────
static void compass_handler(CompassHeadingData heading_data) {
  s_cal_status = heading_data.compass_status;

  if (s_cal_status == CompassStatusCalibrated ||
      s_cal_status == CompassStatusCalibrating) {
    int32_t heading_angle = heading_data.true_heading;
    if (heading_angle == 0 && heading_data.magnetic_heading != 0) {
      heading_angle = heading_data.magnetic_heading;
    }
    int raw_heading = TRIGANGLE_TO_DEG((int)heading_angle);
    raw_heading = ((raw_heading % 360) + 360) % 360;
    s_target_heading = raw_heading;
    s_heading_valid = true;

    // Heading lock detection
    if (s_last_heading >= 0) {
      int diff = raw_heading - s_last_heading;
      if (diff < 0) diff = -diff;
      if (diff > 180) diff = 360 - diff;
      if (diff <= LOCK_THRESHOLD) {
        s_lock_counter++;
        if (s_lock_counter >= LOCK_COUNT_NEEDED) {
          s_heading_locked = true;
        }
      } else {
        s_lock_counter = 0;
        s_heading_locked = false;
      }
    }
    s_last_heading = raw_heading;

    // Show/hide lock indicator
    if (s_heading_locked) {
      text_layer_set_text(s_lock_layer, "LOCKED");
    } else {
      text_layer_set_text(s_lock_layer, "");
    }

    // Physics update happens in the animation timer; just mark dirty
    update_physics();

    snprintf(s_heading_buf, sizeof(s_heading_buf), "%d\u00B0", s_display_heading);
    text_layer_set_text(s_heading_layer, s_heading_buf);
    text_layer_set_text(s_cardinal_layer, cardinal_direction(s_display_heading));
  } else {
    s_heading_valid = false;
    s_heading_locked = false;
    s_lock_counter = 0;
    s_last_heading = -1;
    text_layer_set_text(s_heading_layer, "--");
    text_layer_set_text(s_cardinal_layer, "");
    text_layer_set_text(s_lock_layer, "");
  }

  // Calibration status text
  switch (s_cal_status) {
    case CompassStatusDataInvalid:
      text_layer_set_text(s_status_layer, "No data");
      text_layer_set_text(s_cal_instruction_layer,
                          "Tilt and rotate\nyour wrist slowly");
      break;
    case CompassStatusCalibrating:
      text_layer_set_text(s_status_layer, "Calibrating...");
      text_layer_set_text(s_cal_instruction_layer,
                          "Rotate wrist in\nfigure-8 pattern");
      break;
    case CompassStatusCalibrated:
      text_layer_set_text(s_status_layer, "");
      text_layer_set_text(s_cal_instruction_layer, "");
      break;
    default:
      text_layer_set_text(s_status_layer, "Unavailable");
      text_layer_set_text(s_cal_instruction_layer,
                          "Compass not\nsupported");
      break;
  }

  layer_mark_dirty(s_canvas);
}

// ─── Overlay drawing ─────────────────────────────────────────────────────────
static void overlay_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  // Semi-transparent background (solid black on B&W)
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Overlay border
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, GRect(4, 4, bounds.size.w - 8, bounds.size.h - 8));
}

// ─── Overlay dismiss on any button ──────────────────────────────────────────
static void dismiss_overlay(ClickRecognizerRef recognizer, void *context) {
  if (s_overlay_visible) {
    s_overlay_visible = false;
    layer_set_hidden(s_overlay_layer, true);
    layer_set_hidden(text_layer_get_layer(s_overlay_text), true);
  }
}

static void overlay_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, dismiss_overlay);
  window_single_click_subscribe(BUTTON_ID_SELECT, dismiss_overlay);
  window_single_click_subscribe(BUTTON_ID_DOWN, dismiss_overlay);
}

// ─── AppMessage handlers ─────────────────────────────────────────────────────
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *damping_t = dict_find(iter, MSG_KEY_NEEDLE_DAMPING);
  if (damping_t) {
    int val = (int)damping_t->value->int32;
    if (val >= 0 && val <= 100) {
      s_friction = val;
      persist_write_int(STORAGE_KEY_FRICTION, s_friction);
    }
  }

  Tuple *theme_t = dict_find(iter, PASTEL_MSG_KEY_THEME);
  if (theme_t) {
    s_theme_id = theme_t->value->int32;
    persist_write_int(PASTEL_STORAGE_KEY_THEME, s_theme_id);
    s_theme = pastel_get_theme(s_theme_id);
    // Re-apply theme colors
    text_layer_set_text_color(s_heading_layer, s_theme.primary);
    text_layer_set_text_color(s_cardinal_layer, s_theme.accent);
    text_layer_set_text_color(s_status_layer, s_theme.secondary);
    text_layer_set_text_color(s_cal_instruction_layer, s_theme.secondary);
    text_layer_set_text_color(s_lock_layer, s_theme.highlight);
    layer_mark_dirty(s_canvas);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

// ─── Bluetooth connection handler ────────────────────────────────────────────
static void bt_handler(bool connected) {
  if (!connected && s_bt_connected) {
    // Just disconnected — vibrate to alert
    vibes_double_pulse();
  }
  s_bt_connected = connected;
}

// ─── Window lifecycle ────────────────────────────────────────────────────────
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int sb_h = STATUS_BAR_LAYER_HEIGHT;  // 16px

  window_set_background_color(window, GColorBlack);

  // Status bar at top with dotted separator
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorBlack, GColorWhite);
  status_bar_layer_set_separator_mode(s_status_bar,
                                       StatusBarLayerSeparatorModeDotted);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  // Canvas shifted down by status bar height
  s_canvas = layer_create(GRect(0, sb_h, bounds.size.w, bounds.size.w));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  // Heading degrees (shifted down by sb_h)
  s_heading_layer = text_layer_create(
      GRect(0, sb_h + bounds.size.w - 5, bounds.size.w, 30));
  text_layer_set_background_color(s_heading_layer, GColorClear);
  text_layer_set_text_color(s_heading_layer, s_theme.primary);
  text_layer_set_font(s_heading_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_heading_layer, GTextAlignmentCenter);
  text_layer_set_text(s_heading_layer, "--");
  layer_add_child(root, text_layer_get_layer(s_heading_layer));

  // Cardinal label (shifted down by sb_h)
  s_cardinal_layer = text_layer_create(
      GRect(0, sb_h + bounds.size.w + 22, bounds.size.w / 2, 24));
  text_layer_set_background_color(s_cardinal_layer, GColorClear);
  text_layer_set_text_color(s_cardinal_layer, s_theme.accent);
  text_layer_set_font(s_cardinal_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_cardinal_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_cardinal_layer));

  // Calibration status (shifted down by sb_h)
  s_status_layer = text_layer_create(
      GRect(bounds.size.w / 2, sb_h + bounds.size.w + 22,
            bounds.size.w / 2, 24));
  text_layer_set_background_color(s_status_layer, GColorClear);
  text_layer_set_text_color(s_status_layer, s_theme.secondary);
  text_layer_set_font(s_status_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_status_layer));

  // Calibration instruction text
  s_cal_instruction_layer = text_layer_create(
      GRect(10, bounds.size.h - 40, bounds.size.w - 20, 40));
  text_layer_set_background_color(s_cal_instruction_layer, GColorClear);
  text_layer_set_text_color(s_cal_instruction_layer, s_theme.secondary);
  text_layer_set_font(s_cal_instruction_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_cal_instruction_layer, GTextAlignmentCenter);
  text_layer_set_text(s_cal_instruction_layer, "Starting compass...");
  layer_add_child(root, text_layer_get_layer(s_cal_instruction_layer));

  // Heading lock indicator (below cardinal, shifted by sb_h)
  s_lock_layer = text_layer_create(
      GRect(0, sb_h + bounds.size.w + 42, bounds.size.w, 20));
  text_layer_set_background_color(s_lock_layer, GColorClear);
  text_layer_set_text_color(s_lock_layer, s_theme.highlight);
  text_layer_set_font(s_lock_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_lock_layer, GTextAlignmentCenter);
  text_layer_set_text(s_lock_layer, "");
  layer_add_child(root, text_layer_get_layer(s_lock_layer));

  // First-launch instruction overlay
  s_overlay_layer = layer_create(GRect(10, sb_h + 20,
                                       bounds.size.w - 20,
                                       bounds.size.h - sb_h - 40));
  layer_set_update_proc(s_overlay_layer, overlay_update);
  layer_add_child(root, s_overlay_layer);

  GRect overlay_bounds = layer_get_bounds(s_overlay_layer);
  s_overlay_text = text_layer_create(
      GRect(18, sb_h + 30, bounds.size.w - 56,
            bounds.size.h - sb_h - 60));
  text_layer_set_background_color(s_overlay_text, GColorClear);
  text_layer_set_text_color(s_overlay_text, GColorWhite);
  text_layer_set_font(s_overlay_text,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_overlay_text, GTextAlignmentCenter);
  text_layer_set_text(s_overlay_text,
                      "Compass\n\n"
                      "Updates automatically.\n"
                      "Point wrist forward\nfor heading.\n\n"
                      "Press any button\nto dismiss.");
  layer_add_child(root, text_layer_get_layer(s_overlay_text));
  (void)overlay_bounds;

  // Set click config for overlay dismiss
  window_set_click_config_provider(window, overlay_click_config);

  // Create paths
  s_needle_path = gpath_create(&s_needle_info);
  s_south_path = gpath_create(&s_south_info);

  // Start animation timer
  s_anim_timer = app_timer_register(ANIM_INTERVAL_MS,
                                    anim_timer_callback, NULL);
}

static void window_unload(Window *window) {
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
  gpath_destroy(s_needle_path);
  gpath_destroy(s_south_path);
  text_layer_destroy(s_heading_layer);
  text_layer_destroy(s_cardinal_layer);
  text_layer_destroy(s_status_layer);
  text_layer_destroy(s_cal_instruction_layer);
  text_layer_destroy(s_lock_layer);
  text_layer_destroy(s_overlay_text);
  layer_destroy(s_overlay_layer);
  layer_destroy(s_canvas);
  status_bar_layer_destroy(s_status_bar);
}

static void init(void) {
  // Load persisted friction setting
  if (persist_exists(STORAGE_KEY_FRICTION)) {
    s_friction = persist_read_int(STORAGE_KEY_FRICTION);
  }
  if (persist_exists(PASTEL_STORAGE_KEY_THEME)) {
    s_theme_id = persist_read_int(PASTEL_STORAGE_KEY_THEME);
  }
  s_theme = pastel_get_theme(s_theme_id);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  // Compass service
  compass_service_set_heading_filter(DEG_TO_TRIGANGLE(1));
  compass_service_subscribe(compass_handler);

  // Bluetooth connection service
  s_bt_connected = connection_service_peek_pebble_app_connection();
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler,
  });

  // AppMessage for phone configuration
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(64, 64);
}

static void deinit(void) {
  connection_service_unsubscribe();
  compass_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
