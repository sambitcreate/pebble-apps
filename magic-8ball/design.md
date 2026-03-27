# Magic 8-Ball - Pastel Theme Color Mapping

| UI Element                | Previous Color               | Theme Role           |
|---------------------------|------------------------------|----------------------|
| Window background         | DarkCandyAppleRed / White    | Black                |
| Status bar                | DarkCandyAppleRed / White    | Black / `s_theme.primary` |
| Ball fill                 | Black                        | Unchanged (Black)    |
| Triangle (positive)       | Green                        | `s_theme.accent`     |
| Triangle (neutral)        | ChromeYellow                 | `s_theme.secondary`  |
| Triangle (negative)       | Red                          | `s_theme.highlight`  |
| Answer text               | Black                        | `s_theme.primary`    |
| Prompt text               | White / Black                | `s_theme.accent`     |
| History dots              | Category-colored / Black     | `s_theme.muted`      |
| Overlay text              | White                        | `s_theme.highlight`  |
| "8" text on ball          | White                        | Unchanged (White)    |

## Theme Selection

Configurable via phone settings (Settings > Magic 8-Ball). Default: Warm Sunset.

## Notes

- Background changed from DarkCandyAppleRed to black for consistency with the design system.
- Category colors (positive/neutral/negative) now use theme roles instead of hardcoded green/yellow/red.
- The `#ifdef PBL_COLOR` guards around category functions were removed since `pebble_pastel.h` handles B&W fallback internally.
- Theme picker is added alongside existing Vibration and Animation Speed options.
