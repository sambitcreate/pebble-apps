#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_heading_layer;
static TextLayer *s_cardinal_layer;
static TextLayer *s_status_layer;
static TextLayer *s_cal_instruction_layer;

static int s_heading = 0;
static bool s_heading_valid = false;
static CompassStatus s_cal_status = CompassStatusUnavailable;
static char s_heading_buf[8];

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

// Needle triangle path
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

// South half of needle
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

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  int cy = bounds.size.h / 2;
  GPoint center = GPoint(cx, cy);

  // Compass circle
  GColor ring_color;
  if (!s_heading_valid) {
    ring_color = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray);
  } else {
    ring_color = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray);
  }
  graphics_context_set_stroke_color(ctx, ring_color);
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
      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, p1, p2);
    }

    // Question mark in center
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, "?",
                       fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                       GRect(cx - 20, cy - 28, 40, 50),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    return;
  }

  // Tick marks every 30 degrees (rotate with heading)
  for (int deg = 0; deg < 360; deg += 30) {
    int32_t angle = DEG_TO_TRIGANGLE(deg - s_heading);
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
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, deg % 90 == 0 ? 3 : 1);
    graphics_draw_line(ctx, p1, p2);
  }

  // Cardinal labels on the dial (rotate with heading)
  const char *labels[] = {"N", "E", "S", "W"};
  const int label_degs[] = {0, 90, 180, 270};
  for (int i = 0; i < 4; i++) {
    int32_t angle = DEG_TO_TRIGANGLE(label_degs[i] - s_heading);
    int r = 40;
    int lx = cx + (sin_lookup(angle) * r / TRIG_MAX_RATIO) - 5;
    int ly = cy - (cos_lookup(angle) * r / TRIG_MAX_RATIO) - 8;

    GColor color = (i == 0) ? PBL_IF_COLOR_ELSE(GColorRed, GColorWhite) : GColorWhite;
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
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  gpath_draw_filled(ctx, s_needle_path);

  // Draw needle — south (gray)
  gpath_move_to(s_south_path, center);
  gpath_rotate_to(s_south_path, 0);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
  gpath_draw_filled(ctx, s_south_path);

  // Center dot
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, 4);
}

static void compass_handler(CompassHeadingData heading_data) {
  s_cal_status = heading_data.compass_status;

  // Only use heading data when calibrated or calibrating (with usable data)
  if (s_cal_status == CompassStatusCalibrated ||
      s_cal_status == CompassStatusCalibrating) {
    // Prefer true_heading (accounts for magnetic declination) when valid.
    // true_heading is set to magnetic_heading when true north is unavailable,
    // so we can always use it safely. Fall back to magnetic if it's somehow 0
    // and magnetic is not.
    int32_t heading_angle = heading_data.true_heading;
    if (heading_angle == 0 && heading_data.magnetic_heading != 0) {
      heading_angle = heading_data.magnetic_heading;
    }
    s_heading = TRIGANGLE_TO_DEG((int)heading_angle);
    // Clamp to 0-359
    s_heading = ((s_heading % 360) + 360) % 360;
    s_heading_valid = true;

    snprintf(s_heading_buf, sizeof(s_heading_buf), "%d\u00B0", s_heading);
    text_layer_set_text(s_heading_layer, s_heading_buf);
    text_layer_set_text(s_cardinal_layer, cardinal_direction(s_heading));
  } else {
    s_heading_valid = false;
    text_layer_set_text(s_heading_layer, "--");
    text_layer_set_text(s_cardinal_layer, "");
  }

  // Update calibration status text
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

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  s_canvas = layer_create(GRect(0, 0, bounds.size.w, bounds.size.w));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  // Heading degrees
  s_heading_layer = text_layer_create(GRect(0, bounds.size.w - 5, bounds.size.w, 30));
  text_layer_set_background_color(s_heading_layer, GColorClear);
  text_layer_set_text_color(s_heading_layer, GColorWhite);
  text_layer_set_font(s_heading_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_heading_layer, GTextAlignmentCenter);
  text_layer_set_text(s_heading_layer, "--");
  layer_add_child(root, text_layer_get_layer(s_heading_layer));

  // Cardinal label
  s_cardinal_layer = text_layer_create(GRect(0, bounds.size.w + 22, bounds.size.w / 2, 24));
  text_layer_set_background_color(s_cardinal_layer, GColorClear);
  text_layer_set_text_color(s_cardinal_layer, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  text_layer_set_font(s_cardinal_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_cardinal_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_cardinal_layer));

  // Calibration status
  s_status_layer = text_layer_create(GRect(bounds.size.w / 2, bounds.size.w + 22, bounds.size.w / 2, 24));
  text_layer_set_background_color(s_status_layer, GColorClear);
  text_layer_set_text_color(s_status_layer, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_status_layer));

  // Calibration instruction text (shown over compass when calibrating)
  s_cal_instruction_layer = text_layer_create(GRect(10, bounds.size.h - 40, bounds.size.w - 20, 40));
  text_layer_set_background_color(s_cal_instruction_layer, GColorClear);
  text_layer_set_text_color(s_cal_instruction_layer, PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite));
  text_layer_set_font(s_cal_instruction_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_cal_instruction_layer, GTextAlignmentCenter);
  text_layer_set_text(s_cal_instruction_layer, "Starting compass...");
  layer_add_child(root, text_layer_get_layer(s_cal_instruction_layer));

  s_needle_path = gpath_create(&s_needle_info);
  s_south_path = gpath_create(&s_south_info);
}

static void window_unload(Window *window) {
  gpath_destroy(s_needle_path);
  gpath_destroy(s_south_path);
  text_layer_destroy(s_heading_layer);
  text_layer_destroy(s_cardinal_layer);
  text_layer_destroy(s_status_layer);
  text_layer_destroy(s_cal_instruction_layer);
  layer_destroy(s_canvas);
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  // Use a small heading filter for smooth updates during calibration
  compass_service_set_heading_filter(DEG_TO_TRIGANGLE(1));
  compass_service_subscribe(compass_handler);
}

static void deinit(void) {
  compass_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
