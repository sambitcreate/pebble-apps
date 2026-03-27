# Tally Counter — Design

## Default Theme
Warm Sunset (ID: 0)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Count display (big number) |
| `accent` | Circle multiplier label ("x1", "x2"), status bar text, milestone flash arc |
| `secondary` | (reserved for future use) |
| `highlight` | Progress arc (normal), goal marker dot |
| `muted` | Goal text, yesterday text, hint text, background ring |

## Notes
- The status bar foreground color uses `accent` to match the circle label.
- The milestone flash effect uses `accent` instead of yellow for visual consistency.
- On B&W (diorite), the theme automatically collapses to high-contrast White/LightGray via pebble_pastel.h.
