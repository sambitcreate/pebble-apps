# Pomodoro Timer — Design

## Default Theme
Warm Sunset (ID: 0)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Time display (MM:SS countdown) |
| `accent` | Phase label ("WORK", "BREAK", "LONG BREAK"), filled session dots |
| `secondary` | Pomo count text ("Pomos: N") |
| `highlight` | Countdown progress arc |
| `muted` | Arc background track, hollow session dots |

## Notes
- Phase-specific color logic (Red/Green/Blue per work/break phase) has been removed. All phases now use the unified theme accent color. The phase NAME text tells the user what phase they are in.
- On B&W (diorite), the theme automatically collapses to high-contrast White/LightGray via pebble_pastel.h.
