# Snake

Classic Snake game for Pebble.

## How It Works
- Snake moves continuously, eat food to grow
- Up: Turn up (or cycle clockwise)
- Down: Turn down (or cycle counter-clockwise)
- Select: Pause/Resume, restart after game over
- Score = number of food eaten
- Game over on self-collision or wall hit
- Persistent high score

## Platforms
- diorite, basalt, emery

## Build
```bash
pebble build
pebble install --emulator diorite
```
