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
| Hour markers         | 12 tiny dots (radius 1) as reference points on the 60px circle              |
| Hour hand dot        | Largest dot (radius 4, red on color) at smooth hour angle (hour*30+min/2)   |
| Minute hand dot      | Medium dot (radius 3, cyan on color) at minute angle (min*6)                |
| Second hand dot      | Small dot (radius 1, yellow on color) at second angle (sec*6)               |
| Constellation lines  | Triangle connecting H↔M, M↔S, H↔S in cyan; shifts dynamically with time    |
| Shooting star        | Animated arc with 5-point fading trail; triggers at second 30 each minute (~2s duration) |
| Pole star            | Fixed star at top center; size/color reflects battery percentage            |
| Digital time         | GOTHIC_14 text at the bottom: "HH:MM"                                      |

### Color mapping

| Element             | Color (Basalt/Emery)                      | B&W (Diorite) |
|---------------------|-------------------------------------------|---------------|
| Stars               | White                                     | White          |
| Hour hand dot       | Red                                       | White          |
| Minute hand dot     | Cyan                                      | White          |
| Second hand dot     | Yellow                                    | White          |
| Constellation lines | Cyan                                      | White          |
| Shooting star       | ChromeYellow -> Yellow -> White (trail)    | White          |
| Pole star           | Yellow (>60%), ChromeYellow (>20%), Red    | White          |
| Time text           | Light Gray                                | White          |

### Platforms

Targets **basalt**, **diorite**, and **emery**. Uses `#ifdef PBL_COLOR` for color/BW branching.

### Memory

All data is stack/static -- no heap allocations beyond Pebble SDK objects (Window, Layer, TextLayer). Well under the 64 KB limit for diorite.

### Tick subscription

Subscribed to `SECOND_UNIT` so background stars twinkle every second. Shooting star animation uses `AppTimer` at ~66ms intervals (~15 fps).

### Battery service

Subscribed to `battery_state_service` to update the pole star brightness/size in real time.
