# Orbit — Design

## Default Theme
Warm Sunset (ID: 0)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Date text |
| `accent` | Hour orbit dot and trail |
| `secondary` | Minute orbit dot and trail |
| `highlight` | Second orbit dot and trail, center sun dot |
| `muted` | Orbit path rings, older trail dots |

## Notes
- Battery arc indicator retains semantic Green/Yellow/Red colors to convey critical charge information
- BT disconnect indicator ("!") uses highlight color
- Trail dots fade from the hand's theme color (recent) to muted (older)
- This app had no prior AppMessage infrastructure; it was added to support the theme picker
