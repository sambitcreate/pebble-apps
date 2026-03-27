# Ripple - Pastel Theme Color Mapping

| UI Element            | Previous Color                        | Theme Role         |
|-----------------------|---------------------------------------|--------------------|
| Hour ring (Ring 1)    | Time-of-day cycle (8 colors)          | `s_theme.accent`   |
| Minute ring (Ring 2)  | Complementary time-of-day cycle       | `s_theme.highlight` |
| Pulse ring (Ring 3)   | Yellow / LightGray                    | `s_theme.accent`   |
| Crosshair lines       | White                                 | `s_theme.primary`  |
| Date text             | White                                 | `s_theme.primary`  |
| Time text (bottom)    | DarkGray / LightGray                  | `s_theme.muted`    |

## Theme Selection

Configurable via phone settings (Settings > Ripple). Default: Warm Sunset.

## Notes

- The `hour_color_for_time` and `minute_color_for_time` functions were removed in favor of unified theme colors.
- On B&W platforms (diorite), all theme roles collapse to White/LightGray/DarkGray per `pebble_pastel.h`.
