# Pastel Design System — Pebble Apps Monorepo

**Date:** 2026-03-26
**Status:** Approved

## Overview

A shared pastel color design system with analogous color schemes across all 19 Pebble apps. Users can select from 4 preset themes per app via phone settings. Each app is standalone — theme choice is per-app, not global.

## Color System

### Themes

All themes use **black background** and **5 semantic color roles**. Two roles are fixed (PRIMARY=White, MUTED=DarkGray), three vary by theme (ACCENT, SECONDARY, HIGHLIGHT). The three variable colors form an analogous group on the color wheel.

| Role | Purpose | Warm Sunset | Lavender Dream | Cool Ocean | Forest Meadow | B&W Fallback |
|------|---------|-------------|----------------|------------|---------------|--------------|
| PRIMARY | Main content (time, score) | White | White | White | White | White |
| ACCENT | Labels, status, indicators | Rajah `#FFAA55` | BabyBlueEyes `#AAAAFF` | PictonBlue `#55AAFF` | MintGreen `#AAFFAA` | White |
| SECONDARY | Supporting info, subtitles | Melon `#FFAAAA` | RichBrilliantLavender `#FFAAFF` | Celeste `#AAFFFF` | Inchworm `#AAFF55` | LightGray |
| HIGHLIGHT | Progress arcs, active elements | BrilliantRose `#FF55AA` | ShockingPink `#FF55FF` | ElectricBlue `#55FFFF` | ScreaminGreen `#55FF55` | White |
| MUTED | Hints, gridlines, inactive | DarkGray `#555555` | DarkGray `#555555` | DarkGray `#555555` | DarkGray `#555555` | DarkGray |

### Default Theme Assignment

- **Watchfaces** (binary-clock, constellation, morse-time, orbit, pendulum, ripple, tetromino): Warm Sunset
- **Games** (snake, pixel-invaders, maze, game-of-life): Lavender Dream
- **Utilities** (compass, stopwatch, pomodoro-timer, tally-counter, breathing, dice-roller, reaction-tester, magic-8ball): Warm Sunset

### B&W Fallback (Diorite)

On Pebble 2 (no color), all themes collapse via `PBL_IF_COLOR_ELSE` to: PRIMARY=White, ACCENT=White, SECONDARY=LightGray, HIGHLIGHT=White, MUTED=DarkGray.

## Architecture

### Shared Header: `shared/pebble_pastel.h`

```c
#pragma once
#include <pebble.h>

// Theme IDs
#define THEME_WARM_SUNSET    0
#define THEME_LAVENDER_DREAM 1
#define THEME_COOL_OCEAN     2
#define THEME_FOREST_MEADOW  3
#define THEME_COUNT          4

// Shared storage/message keys for theme
#define PASTEL_STORAGE_KEY_THEME 200
#define PASTEL_MSG_KEY_THEME     200

typedef struct {
  GColor primary;
  GColor accent;
  GColor secondary;
  GColor highlight;
  GColor muted;
} PastelTheme;

static inline PastelTheme pastel_get_theme(int theme_id) {
  PastelTheme t;
  t.primary = GColorWhite;
  t.muted = GColorDarkGray;

#ifdef PBL_COLOR
  switch (theme_id) {
    case THEME_LAVENDER_DREAM:
      t.accent    = GColorBabyBlueEyes;
      t.secondary = GColorRichBrilliantLavender;
      t.highlight = GColorShockingPink;
      break;
    case THEME_COOL_OCEAN:
      t.accent    = GColorPictonBlue;
      t.secondary = GColorCeleste;
      t.highlight = GColorElectricBlue;
      break;
    case THEME_FOREST_MEADOW:
      t.accent    = GColorMintGreen;
      t.secondary = GColorInchworm;
      t.highlight = GColorScreaminGreen;
      break;
    default: // THEME_WARM_SUNSET
      t.accent    = GColorRajah;
      t.secondary = GColorMelon;
      t.highlight = GColorBrilliantRose;
      break;
  }
#else
  t.accent    = GColorWhite;
  t.secondary = GColorLightGray;
  t.highlight = GColorWhite;
#endif

  return t;
}
```

### Per-App Integration

Each app's `main.c`:
1. `#include "../shared/pebble_pastel.h"`
2. Static `PastelTheme s_theme;` and `int s_theme_id;`
3. Load from persist: `s_theme_id = persist_exists(PASTEL_STORAGE_KEY_THEME) ? persist_read_int(PASTEL_STORAGE_KEY_THEME) : DEFAULT_THEME;`
4. `s_theme = pastel_get_theme(s_theme_id);`
5. Replace hardcoded GColor calls with `s_theme.accent`, `s_theme.secondary`, etc.
6. Handle theme changes in `inbox_received` — save, rebuild theme, mark dirty.

Each app's `pkjs/index.js`:
1. Add theme radio buttons to existing config HTML (or create config if app lacks one)
2. Theme names: "Warm Sunset", "Lavender Dream", "Cool Ocean", "Forest Meadow"
3. Send `Theme` message key with integer ID
4. Persist selection in localStorage

### Message Key Convention

Each app's `package.json` adds `"Theme": 0` to messageKeys (or uses next available index). The C-side define `PASTEL_MSG_KEY_THEME` maps to this.

## Per-App Color Role Mapping

Each app maps its specific UI elements to the 5 roles. Documented in `<app>/design.md`.

### Watchfaces

**binary-clock:** ACCENT=active dots/colon, SECONDARY=date text, HIGHLIGHT=column labels, MUTED=off dots
**constellation:** ACCENT=star glow, SECONDARY=time text, HIGHLIGHT=constellation lines, MUTED=dim stars
**morse-time:** ACCENT=active signal, SECONDARY=speed label, HIGHLIGHT=completed signals, MUTED=hint text
**orbit:** ACCENT=hour orbit, SECONDARY=minute orbit, HIGHLIGHT=second orbit/center dot, MUTED=orbit paths
**pendulum:** ACCENT=beat markers, SECONDARY=time digits, HIGHLIGHT=pendulum bob, MUTED=guide lines
**ripple:** ACCENT=hour ripple, SECONDARY=minute ripple, HIGHLIGHT=center time, MUTED=time text
**tetromino:** ACCENT=active piece, SECONDARY=date/lines text, HIGHLIGHT=landed pieces, MUTED=grid lines

### Games

**snake:** ACCENT=snake head, SECONDARY=snake body gradient, HIGHLIGHT=food, MUTED=grid/hints
**pixel-invaders:** ACCENT=player ship, SECONDARY=invader rows, HIGHLIGHT=bullets/explosions, MUTED=score text
**maze:** ACCENT=player dot, SECONDARY=trail, HIGHLIGHT=exit marker, MUTED=walls
**game-of-life:** ACCENT=living cells, SECONDARY=aging cells, HIGHLIGHT=newborn cells, MUTED=generation count

### Utilities

**compass:** ACCENT=cardinal labels, SECONDARY=heading text, HIGHLIGHT=needle, MUTED=ring/ticks
**stopwatch:** ACCENT=state label, SECONDARY=lap times, HIGHLIGHT=progress indicator, MUTED=hints
**pomodoro-timer:** ACCENT=phase label, SECONDARY=session count, HIGHLIGHT=progress arc, MUTED=dots/guides
**tally-counter:** ACCENT=count display, SECONDARY=goal text, HIGHLIGHT=progress arc, MUTED=hints
**breathing:** ACCENT=phase text, SECONDARY=duration, HIGHLIGHT=breathing rings, MUTED=pattern label
**dice-roller:** ACCENT=die type label, SECONDARY=stats text, HIGHLIGHT=result number, MUTED=history
**reaction-tester:** ACCENT=round indicator, SECONDARY=result text, HIGHLIGHT=go signal, MUTED=hints
**magic-8ball:** ACCENT=prompt text, SECONDARY=answer text, HIGHLIGHT=triangle/ball accent, MUTED=shake hint

## Design.md File Format

Each app gets a `design.md` with:
- Default theme
- Role-to-UI-element mapping table
- App-specific color notes (e.g., magic-8ball's colored background becomes black to match system)
- B&W fallback behavior

## CLAUDE.md Updates

Add to root CLAUDE.md:
- Design system reference pointing to `shared/pebble_pastel.h`
- Rule: all apps must use `PastelTheme` roles, not hardcoded GColor
- Rule: every app must have a `design.md` kept in sync with main.c

## AGENTS.md

Create root AGENTS.md with:
- Instruction for agents to read `design.md` before modifying any app's colors
- Instruction to update `design.md` when changing color usage in an app
- Reference to the 5-role system and theme definitions
