# Constellation — Design

## Default Theme
Warm Sunset (ID: 0)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Digital time text |
| `accent` | Constellation lines, minute hand dot, pole star (medium battery) |
| `secondary` | Pole star (low battery), shooting star trail (oldest) |
| `highlight` | Hour hand dot, second hand dot, pole star (high battery), shooting star head |
| `muted` | (unused — background stars remain white for visibility) |

## Notes
- Background stars remain GColorWhite to maintain visibility against the black sky
- Hour markers (tiny reference dots) remain GColorWhite
- Pole star battery colors use theme roles instead of semantic Green/Yellow/Red since the size already conveys charge level
- Shooting star trail fades from secondary (oldest) through accent to highlight (head)
