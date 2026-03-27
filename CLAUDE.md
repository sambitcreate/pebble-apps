# Pebble Apps Monorepo

Collection of fun Pebble smartwatch apps targeting **Pebble 2 (diorite) and up**.

## Target Platforms
- **diorite** — Pebble 2 (144×168, B&W, 64KB)
- **basalt** — Pebble Time (144×168, 64-color, 64KB)
- **emery** — Pebble Time 2 (200×228, 64-color, 128KB)

## Building
Each subdirectory is a standalone Pebble project. Build with:
```bash
cd <project-name>
pebble build
```
Or import into CloudPebble at cloudpebble.repebble.com.

## Testing
```bash
pebble install --emulator diorite
pebble install --emulator basalt
pebble install --emulator emery
```

## Project Structure
Each app contains:
- `package.json` — Pebble project manifest
- `wscript` — Build configuration
- `src/main.c` — Application source
- `CLAUDE.md` — Project-specific docs

## Language
All apps are written in C targeting the Pebble SDK 4.x API.
Use `PBL_IF_COLOR_ELSE()` and `PBL_IF_ROUND_ELSE()` for cross-platform support.

## Pastel Design System
All apps use a shared color system defined in `shared/pebble_pastel.h`.

### Rules
- **Never hardcode GColor values for themed elements.** Use `s_theme.accent`, `s_theme.secondary`, `s_theme.highlight`, `s_theme.primary`, or `s_theme.muted` from the `PastelTheme` struct.
- Every app must `#include "../shared/pebble_pastel.h"` and load the theme from persist storage on init.
- Every app must have a `design.md` documenting how the 5 color roles map to its UI elements.
- When modifying color usage in any app, update its `design.md` to stay in sync.
- The phone config page (pkjs/index.js) must include a theme picker with all 4 themes.

### Color Roles
| Role | Purpose | Fixed? |
|------|---------|--------|
| `primary` | Main content (time, score) | Always GColorWhite |
| `accent` | Labels, status, indicators | Theme-dependent |
| `secondary` | Supporting info, subtitles | Theme-dependent |
| `highlight` | Progress arcs, active elements | Theme-dependent |
| `muted` | Hints, gridlines, inactive | Always GColorDarkGray |

### Themes
| ID | Name | Accent | Secondary | Highlight |
|----|------|--------|-----------|-----------|
| 0 | Warm Sunset | Rajah | Melon | BrilliantRose |
| 1 | Lavender Dream | BabyBlueEyes | RichBrilliantLavender | ShockingPink |
| 2 | Cool Ocean | PictonBlue | Celeste | ElectricBlue |
| 3 | Forest Meadow | MintGreen | Inchworm | ScreaminGreen |

### Default Themes
- **Watchfaces** (binary-clock, constellation, morse-time, orbit, pendulum, ripple, tetromino): Warm Sunset (0)
- **Games** (snake, pixel-invaders, maze, game-of-life): Lavender Dream (1)
- **Utilities** (everything else): Warm Sunset (0)
