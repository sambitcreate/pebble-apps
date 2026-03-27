# Tetromino - Pastel Theme Color Mapping

| UI Element              | Previous Color                          | Theme Role           |
|-------------------------|-----------------------------------------|----------------------|
| Block base color        | 7 classic Tetris colors (per-piece)     | `s_theme.accent`     |
| Block highlight (bevel) | 7 lighter Tetris colors                 | `s_theme.secondary`  |
| Block shadow (bevel)    | 7 darker Tetris colors                  | `s_theme.muted`      |
| Date text               | LightGray / White                       | `s_theme.secondary`  |
| Lines counter text      | DarkGray / White                        | `s_theme.muted`      |
| Grid lines              | `GColorFromHEX(0x111111)` / White       | Unchanged (subtle)   |

## Theme Selection

Configurable via phone settings (Settings > Tetromino). Default: Warm Sunset.

## Notes

- The `get_tetris_color`, `get_highlight_color`, and `get_shadow_color` functions were simplified to return theme-based colors instead of per-piece colors.
- All blocks now share a unified color from the active theme, giving a cohesive pastel look.
- On B&W platforms (diorite), blocks remain white with dark borders.
