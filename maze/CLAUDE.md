# Maze Watchface

A funky abstract Pebble watchface that displays a procedurally-navigated maze.

## How It Works

- The screen shows a 12x14 grid maze (each cell 12x12 pixels on the 144x168 display).
- Six pre-made maze layouts are stored as compact arrays (2 wall bits per cell: bit 0 = right wall, bit 1 = bottom wall).
- The current hour selects which maze pattern is displayed (`hour % 6`).
- A BFS solver finds the shortest path from top-left (0,0) to bottom-right (11,13).
- The current minute (0-59) maps to a position along the solved path, where a dot is drawn.
- A trail of small dots shows the visited portion of the path.
- The current time is displayed below the maze in small text.

## Color Behavior

- **Color platforms (Basalt, Emery):** Dark gray walls, green dot, dark green trail, black background.
- **B&W platform (Diorite):** White walls, white dot, white trail dots (smaller), black background.

## Memory

All maze data is stored in `const` arrays (ROM). The BFS solver uses stack-allocated arrays (~500 bytes). Total RAM usage is well within the 64KB diorite limit.

## Target Platforms

basalt, diorite, emery

## Build

```
pebble build
```

## Files

- `src/main.c` — All watchface logic (maze data, BFS solver, drawing, tick handler)
- `package.json` — Pebble project manifest (UUID: a1b2c3d4-e003-4000-8000-000000000e03)
- `wscript` — Standard Pebble build script
