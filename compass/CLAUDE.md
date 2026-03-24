# Compass

Digital compass using the Pebble's magnetometer.

## How It Works
- Large heading display in degrees (0-359)
- Cardinal direction label (N, NE, E, SE, S, SW, W, NW)
- Visual compass needle drawn with GPath
- Calibration status indicator
- Requires Pebble hardware with magnetometer (Pebble 2, Time, Time 2)

## Controls
- Buttons not used — compass updates automatically
- Back to exit

## Platforms
- diorite, basalt, emery

## Build
```bash
pebble build
pebble install --emulator diorite
```
