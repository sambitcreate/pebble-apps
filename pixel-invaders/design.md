# Pixel Invaders -- Design

## Default Theme
Lavender Dream (ID: 1)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | HUD text (score, wave, lives), game over score, instruction text |
| `accent` | Player ship, "GAME OVER" title, "PIXEL INVADERS" title, wave banner |
| `secondary` | Alien sprites |
| `highlight` | Player bullets, alien bullets, explosions, "NEW HIGH SCORE!" text, BT disconnect alert |
| `muted` | Shields |

## Notes
- Previously pure B&W; color support added via pastel theme system
- Background remains GColorBlack on all platforms
- Alien eyes remain GColorBlack cutouts for contrast
- Theme is persisted via PASTEL_STORAGE_KEY_THEME (key 200)
- Theme change triggers immediate canvas redraw
