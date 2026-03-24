# Constellation Watchface

A Pebble watchface that displays time through a star-field and constellation motif.

## Build

```
pebble build
```

## Install

```
pebble install --phone <IP>
```

## Architecture

Single-file C watchface (`src/main.c`) with no external resources.

### Visual elements

| Element              | Description                                                                 |
|----------------------|-----------------------------------------------------------------------------|
| Background stars     | 35 small dots (1-2 px) at deterministic positions; ~20% twinkle each second |
| Hour markers         | 12 dots on a 60 px radius circle; current hour is larger and red (color)    |
| Constellation lines  | 3 line segments connecting hour-marker dots; pattern changes each hour      |
| Shooting star        | Short streak at the current minute's angular position on the outer rim      |
| Digital time         | GOTHIC_14 text at the bottom of the screen                                  |

### Color mapping

| Element           | Color (Basalt/Emery) | B&W (Diorite) |
|-------------------|----------------------|---------------|
| Stars             | White                | White          |
| Constellation     | Cyan                 | White          |
| Shooting star     | Yellow               | White          |
| Current hour dot  | Red                  | White          |
| Time text         | Light Gray           | White          |

### Platforms

Targets **basalt**, **diorite**, and **emery**. Uses `#ifdef PBL_COLOR` for color/BW branching.

### Memory

All data is stack/static -- no heap allocations beyond Pebble SDK objects (Window, Layer, TextLayer). Well under the 64 KB limit for diorite.

### Tick subscription

Subscribed to `SECOND_UNIT` so background stars twinkle every second.
