# Stopwatch — Design

## Default Theme
Warm Sunset (ID: 0)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Main time display (MM:SS.t), B&W fallback for lap deltas |
| `accent` | State label ("Running", "Stopped", "Ready") |
| `secondary` | Lap time entries (last 3 laps) |
| `highlight` | Elapsed-time arc fill, overlay instruction text |
| `muted` | Arc background ring |

## Notes
- Lap delta colors (red for slower, green for faster) are kept as semantic colors and not themed, since they convey directional meaning.
- On B&W (diorite), the theme automatically collapses to high-contrast White/LightGray via pebble_pastel.h.
