# Snake -- Design

## Default Theme
Lavender Dream (ID: 1)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | Score text, high score, instruction text |
| `accent` | Snake head, "GAME OVER" title, "SNAKE" title on first-launch |
| `secondary` | Snake body segments |
| `highlight` | All food types (normal, bonus, speed), "PAUSED" text, speed boost indicator, BT disconnect alert |
| `muted` | Grid border |

## Notes
- Snake body was simplified from a 4-shade gradient to a single `secondary` color, with the head using `accent`
- All three food types (normal, bonus, speed) now use `highlight` instead of separate Red/Yellow/Cyan
- Theme is persisted via PASTEL_STORAGE_KEY_THEME (key 200)
- Theme change triggers immediate canvas redraw
