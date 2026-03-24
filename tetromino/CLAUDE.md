# Tetromino Watchface

A Pebble watchface that displays the current time using Tetris-style block formations.

## Overview

Time is rendered as HH:MM using large pixel-font digits composed of small 6x6 blocks arranged in 3x5 grid patterns. Each block has a bevel effect (highlight top-left, shadow bottom-right) to mimic classic Tetris pieces. The background features a faint grid resembling a Tetris game board.

## Features

- **Block digits**: Each digit (0-9) is a 3-wide x 5-tall grid of filled/empty blocks.
- **Tetris colors**: On color displays (basalt, emery), each digit uses a different classic Tetris color (cyan, yellow, magenta, green, red, orange, blue). On B&W (diorite), blocks are white with dark borders.
- **Blinking colon**: Two stacked blocks between HH and MM toggle visibility every second.
- **Date**: Abbreviated day-of-week shown at the bottom in small font.
- **12h/24h**: Respects the user's clock style setting; suppresses leading zero in 12h mode.
- **Efficient redraws**: Subscribes to SECOND_UNIT for colon blink, full digit redraw occurs on MINUTE_UNIT.

## Build

```
pebble build
```

## Target Platforms

diorite, basalt, emery

## Project Structure

- `src/main.c` -- All watchface logic (digit patterns, block rendering, tick handling)
- `package.json` -- Pebble project metadata (UUID: a1b2c3d4-e005-4000-8000-000000000e05)
- `wscript` -- Standard Pebble build script
