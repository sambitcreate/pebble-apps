# Reaction Tester - Pastel Theme Color Mapping

| UI Element               | Previous Color                    | Theme Role           |
|---------------------------|-----------------------------------|----------------------|
| Idle title text           | White                             | `s_theme.primary`    |
| "Wait..." text            | White on DarkCandyAppleRed bg     | `s_theme.muted` on black bg |
| "NOW!" text               | Black on White bg                 | Black on `s_theme.highlight` bg |
| "TOO EARLY!" text         | Black on Orange bg                | `s_theme.highlight` on black bg |
| Result (ms) text          | White on IslamicGreen bg          | `s_theme.primary` on black bg |
| Rating text               | White                             | White (unchanged)    |
| Best time text            | Yellow / White                    | `s_theme.accent`     |
| Round indicator text      | Yellow / White                    | `s_theme.accent`     |
| Summary title             | White on DukeBlue bg              | `s_theme.accent` on black bg |
| Stats title               | White on OxfordBlue bg            | `s_theme.primary` on black bg |
| Overlay title             | White on DukeBlue bg              | `s_theme.primary` on black bg |

## Theme Selection

Configurable via phone settings (Settings > Reaction Tester). Default: Warm Sunset.

## Notes

- Background color changes were removed in favor of a consistent black background.
- The "NOW!" tap signal uses `s_theme.highlight` as background with black text for visual contrast.
- State is now communicated through text color changes rather than background color changes.
- Theme picker is added alongside existing Rounds per Session option.
