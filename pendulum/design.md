# Pendulum — Design

## Default Theme
Warm Sunset (ID: 0)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Pendulum arm, pivot point |
| `accent` | Inactive hour dots (outline) |
| `secondary` | (unused) |
| `highlight` | Current hour dot (filled), pendulum bob |
| `muted` | Tick marks at 15s intervals, trail arc |

## Notes
- Day number text inside the bob remains GColorBlack since it is drawn on top of the filled bob
- On B&W (diorite), the bob ring outline remains for differentiation
- This app had no prior AppMessage infrastructure; it was added to support the theme picker
