# Ripple - Pebble Watchface

A zen/abstract watchface displaying concentric expanding rings, like ripples on water or a sonar display.

## Visual Design

- **Dark background** with three concentric rings drawn from the screen center.
- **Ring 1 (Hours)**: Variable stroke (1-3px, thicker later in day). Radius maps the current hour (0-11) to a 10-60px range. Color cycles by time-of-day on color platforms (cyan->green->yellow->magenta), white on B&W.
- **Ring 2 (Minutes)**: Variable stroke (1-2px, thicker past 30min). Radius maps the current minute (0-59) to a 10-60px range. Complementary color offset from hour ring on color platforms, white on B&W.
- **Ring 3 (Seconds pulse)**: 1px stroke circle animated via AppTimer at 50ms intervals. Expands from 0 to 65px over each second using **ease-out cubic** for organic water-ripple feel, then resets. Yellow on color, light gray on B&W.
- **Crosshair**: Small 4-line crosshair at the center point.
- **Date display**: Short date (e.g. "MAR 24") rendered left of center along the crosshair line in GOTHIC_14.
- **Time text**: Digital time in GOTHIC_14 at the bottom of the screen in a subtle color.

## Architecture

Single-file C application (`src/main.c`):
- `tick_handler` subscribes to `SECOND_UNIT` to update hour/minute ring radii, reset the pulse, and format date/time text.
- `pulse_timer_callback` fires every 50ms (20 steps per second) to animate ring 3 expansion with ease-out cubic easing.
- `ease_out_cubic_radius` implements `radius = max_radius * (1 - (1-t)^3)` using integer arithmetic.
- `canvas_update_proc` draws all rings (with dynamic stroke widths and time-of-day colors), the crosshair, and the date overlay.
- `hour_color_for_time` / `minute_color_for_time` provide 8-segment color lookup tables for 24h cycling on color platforms.
- Uses `PBL_COLOR` preprocessor guard for color vs B&W styling.

## Build

```
pebble build
```

## Target Platforms

diorite, basalt, emery
