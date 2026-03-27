# Morse Time — Design

## Default Theme
Warm Sunset (ID: 0)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Digital time display |
| `accent` | Completed/inactive morse bars, speed label |
| `secondary` | (unused) |
| `highlight` | Active morse bar during playback, overlay text |
| `muted` | Colon separator dots, hint text |

## Notes
- The active bar (currently vibrating element) uses highlight for emphasis against the accent-colored inactive bars
- Overlay text (first-launch help screen) uses highlight for visibility
- Status bar colors are unchanged (system default)
