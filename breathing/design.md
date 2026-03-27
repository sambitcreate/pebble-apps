# Breathe — Design

## Default Theme
Warm Sunset (ID: 0)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Phase text ("Breathe In", "Hold", "Breathe Out") |
| `accent` | Middle breathing ring |
| `secondary` | Pattern label (4-7-8, Box, Simple), outer breathing ring |
| `highlight` | Inner breathing ring, center fill circle |
| `muted` | Duration text, cycle count, outer boundary guide circle |

## Notes
- The multi-color phase-specific ring system (Cyan/Yellow/Magenta per phase) has been replaced with consistent theme colors across all phases.
- Inner ring = highlight (boldest), middle ring = accent, outer ring = secondary (softest).
- On B&W (diorite), the theme automatically collapses to high-contrast White/LightGray via pebble_pastel.h.
