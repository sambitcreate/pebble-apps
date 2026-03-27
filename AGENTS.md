# Agent Instructions — Pebble Apps Monorepo

## Design System Compliance

Before modifying any app's color-related code:
1. Read the app's `design.md` to understand its color role mapping.
2. Read `shared/pebble_pastel.h` for the theme definitions.
3. Use `s_theme.accent`, `s_theme.secondary`, `s_theme.highlight`, `s_theme.primary`, `s_theme.muted` — never hardcode GColor values for themed elements.
4. After modifying color usage, update the app's `design.md` to reflect changes.

## Design.md Sync Rule

Every app's `design.md` must accurately reflect the current color role mapping in `main.c`. If you change which role a UI element uses, update `design.md` in the same edit session. The `design.md` is the source of truth for *intent*; the code is the source of truth for *implementation*. They must agree.

## Theme Configuration Pattern

All apps support user-selectable themes via phone settings:
- `package.json` must include `"Theme"` in messageKeys
- `src/pkjs/index.js` must include the theme picker radio buttons
- `src/main.c` must handle `PASTEL_MSG_KEY_THEME` in `inbox_received`
- Theme is persisted with `persist_write_int(PASTEL_STORAGE_KEY_THEME, ...)`

## Adding a New App

1. Create `src/main.c` with `#include "../shared/pebble_pastel.h"`
2. Add `PastelTheme s_theme` and load/save logic
3. Add theme picker to phone config
4. Create `design.md` with the role mapping table
5. Choose appropriate default theme (see CLAUDE.md for category defaults)
