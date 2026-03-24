# Ripple - Pebble Watchface

A zen/abstract watchface displaying concentric expanding rings, like ripples on water or a sonar display.

## Visual Design

- **Dark background** with three concentric rings drawn from the screen center.
- **Ring 1 (Hours)**: 2px stroke circle. Radius maps the current hour (0-11) to a 10-60px range. Cyan on color displays, white on B&W.
- **Ring 2 (Minutes)**: 1px stroke circle. Radius maps the current minute (0-59) to a 10-60px range. Magenta on color, white on B&W.
- **Ring 3 (Seconds pulse)**: 1px stroke circle animated via AppTimer at 50ms intervals. Expands from 0 to 65px over each second, then resets. Yellow on color, light gray on B&W.
- **Crosshair**: Small 4-line crosshair at the center point.
- **Time text**: Digital time in GOTHIC_14 at the bottom of the screen in a subtle color.

## Architecture

Single-file C application (`src/main.c`):
- `tick_handler` subscribes to `SECOND_UNIT` to update hour/minute ring radii and reset the pulse.
- `pulse_timer_callback` fires every 50ms (20 steps per second) to animate ring 3 expansion.
- `canvas_update_proc` draws all rings and the crosshair on each frame.
- Uses `PBL_COLOR` preprocessor guard for color vs B&W styling.

## Build

```
pebble build
```

## Target Platforms

diorite, basalt, emery
