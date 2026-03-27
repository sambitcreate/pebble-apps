# Compass — Design

## Default Theme
Warm Sunset (ID: 0)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Heading degrees text, tick marks, cardinal labels (E/S/W), center dot |
| `accent` | Cardinal direction label below compass (N, NE, etc.) |
| `secondary` | Calibration status text, calibration instruction text, calibration dashes |
| `highlight` | Compass needle (north), "N" label on dial, lock indicator |
| `muted` | Compass ring, south needle half |

## Notes
- The "N" cardinal label on the dial uses `highlight` to distinguish it from other cardinal labels.
- On B&W (diorite), the theme automatically collapses to high-contrast White/LightGray via pebble_pastel.h.
