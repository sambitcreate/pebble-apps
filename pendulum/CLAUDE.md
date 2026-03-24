# Pendulum

A geometric watchface that displays time through a swinging pendulum.

## How It Works
- A pendulum line swings left and right from a pivot at the top-center of the screen
- The swing angle is driven by the current second: angle = 30 * sin(second * 2pi / 60), creating a smooth tick-tock motion
- The pendulum bob (filled circle) distance from the pivot represents the current minute (40px at minute 0, 130px at minute 59)
- 12 hour-indicator dots are arranged in an arc across the top of the screen; the current hour dot is filled and larger
- A faint arc trail shows the recent swing path of the bob

## Color Scheme
- **Color displays (basalt, emery):** white pendulum line, cyan bob, magenta hour dots
- **B&W display (diorite):** all white with size differentiation

## Platforms
- diorite (Pebble 2) -- B&W
- basalt (Pebble Time) -- color
- emery (Pebble Time 2) -- color, larger screen

## Build
```bash
pebble build
pebble install --emulator basalt
```
