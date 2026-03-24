# Orbit - Pebble Watchface

Geometric/abstract watchface showing three dots orbiting the center of the display, inspired by the 3ANGLE watchface concept.

## Design

- **Hour dot**: Outermost orbit (55px radius), largest dot (radius 6), moves once around in 12 hours.
- **Minute dot**: Middle orbit (38px radius), medium dot (radius 4), completes one revolution per hour.
- **Second dot**: Innermost orbit (20px radius), smallest dot (radius 3), completes one revolution per minute.
- Faint gray orbit path circles are drawn for each ring.
- Each dot leaves a fading trail of its last 5 positions (older = smaller/dimmer).
- A small white center dot marks the origin.

## Color Scheme

- **Color platforms (Basalt/Emery)**: Hour = cyan, Minute = magenta, Second = yellow.
- **B&W platform (Diorite)**: All dots are white, differentiated by size.

## Target Platforms

diorite, basalt, emery

## Build

```
pebble build
```

## Project Structure

- `src/main.c` - All watchface logic (single-file app)
- `package.json` - Pebble project manifest (UUID: a1b2c3d4-e001-4000-8000-000000000e01)
- `wscript` - Standard Pebble waf build script

## Technical Notes

- Uses `SECOND_UNIT` tick timer for per-second updates.
- Trigonometry via Pebble's `sin_lookup`, `cos_lookup`, `DEG_TO_TRIGANGLE`, and `TRIG_MAX_RATIO`.
- Angle 0 degrees = 12 o'clock position, increasing clockwise.
- Trail positions are stored in a ring buffer (FIFO) of length 5 per hand.
- This is a watchface (`watchapp.watchface = true`), not an app.
