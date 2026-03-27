# Game of Life -- Design

## Default Theme
Lavender Dream (ID: 1)

## Color Role Mapping

| Role | UI Element |
|------|-----------|
| `primary` | (not directly drawn; White is always primary) |
| `accent` | Generation counter text, first-launch overlay text, established cells (age 1-5) |
| `secondary` | Old cells (age > 5), speed indicator text |
| `highlight` | Newborn cells (age 0), BT disconnect warning |
| `muted` | Population graph bars |

## Notes
- Replaced age-based color heat map (Green/Yellow/Orange/Red) with 3-tier theme coloring
- The `color_for_age` function now returns theme colors instead of hardcoded GColors
- The `#ifdef PBL_COLOR` guard was removed from cell drawing since the theme handles B&W fallback
- Generation counter TextLayer color is updated on theme change
- Theme is persisted via PASTEL_STORAGE_KEY_THEME (key 200)
