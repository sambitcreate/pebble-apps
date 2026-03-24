# Conway's Game of Life

Cellular automaton running on your Pebble watch.

## How It Works
- 36x42 grid (4px cells on 144x168 screen)
- Classic Conway rules: birth=3, survive=2-3
- Select: Pause/Resume simulation
- Up: Randomize the grid
- Down: Clear and seed with a glider gun
- Runs at ~10 FPS via AppTimer

## Platforms
- diorite, basalt, emery

## Build
```bash
pebble build
pebble install --emulator diorite
```
