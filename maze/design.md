# Maze -- Design

## Default Theme
Lavender Dream (ID: 1)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Time text below maze |
| `accent` | Trail dots along solved path |
| `secondary` | (not used) |
| `highlight` | Pulsing player dot at current minute position |
| `muted` | Maze walls and outer border |

## Notes
- This watchface had no config page previously; full AppMessage infrastructure was added
- Trail dot radius remains platform-dependent (2px color, 1px B&W) via PBL_IF_COLOR_ELSE
- Theme is persisted via PASTEL_STORAGE_KEY_THEME (key 200)
- Theme change triggers immediate canvas redraw
- New pkjs/index.js created with theme-only config page
