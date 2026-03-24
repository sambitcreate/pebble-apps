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
| Background stars     | 35 dots with 3 brightness levels (1px dim, 2px normal, 3px bright); ~33% cycle each second |
| Hour markers         | 12 dots on a 60 px radius circle; current hour is larger and red (color)    |
| Constellation lines  | 3 line segments connecting hour-marker dots; pattern changes each hour      |
| Constellation name   | Name of current hour's zodiac constellation shown alongside time text       |
| Shooting star        | Animated arc with 5-point fading trail; triggers at second 30 each minute (~2s duration) |
| Pole star            | Fixed star at top center; size/color reflects battery percentage            |
| Digital time         | GOTHIC_14 text at the bottom: "HH:MM  ConstellationName"                   |

### Color mapping

| Element           | Color (Basalt/Emery)                      | B&W (Diorite) |
|-------------------|-------------------------------------------|---------------|
| Stars             | White                                     | White          |
| Constellation     | Cyan                                      | White          |
| Shooting star     | ChromeYellow -> Yellow -> White (trail)    | White          |
| Current hour dot  | Red                                       | White          |
| Pole star         | Yellow (>60%), ChromeYellow (>20%), Red    | White          |
| Time text         | Light Gray                                | White          |

### Platforms

Targets **basalt**, **diorite**, and **emery**. Uses `#ifdef PBL_COLOR` for color/BW branching.

### Memory

All data is stack/static -- no heap allocations beyond Pebble SDK objects (Window, Layer, TextLayer). Well under the 64 KB limit for diorite.

### Tick subscription

Subscribed to `SECOND_UNIT` so background stars twinkle every second. Shooting star animation uses `AppTimer` at ~66ms intervals (~15 fps).

### Battery service

Subscribed to `battery_state_service` to update the pole star brightness/size in real time.
