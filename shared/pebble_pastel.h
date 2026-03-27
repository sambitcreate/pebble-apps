#pragma once
#include <pebble.h>

// ---------------------------------------------------------------------------
// Pebble Pastel Design System
// Shared analogous-pastel color themes for all apps in this monorepo.
// ---------------------------------------------------------------------------

// Theme IDs — keep in sync with pkjs/index.js radio values
#define THEME_WARM_SUNSET    0
#define THEME_LAVENDER_DREAM 1
#define THEME_COOL_OCEAN     2
#define THEME_FOREST_MEADOW  3
#define THEME_COUNT          4

// Persist / AppMessage key for theme selection (high value to avoid collisions)
#define PASTEL_STORAGE_KEY_THEME 200
#define PASTEL_MSG_KEY_THEME     200

// The five semantic color roles every app uses
typedef struct {
  GColor primary;    // Main content: time, score, count (always White)
  GColor accent;     // Labels, status, active indicators (theme color 1)
  GColor secondary;  // Supporting info, subtitles (theme color 2 — softest pastel)
  GColor highlight;  // Progress arcs, game objects, bold accents (theme color 3)
  GColor muted;      // Hints, gridlines, inactive elements (always DarkGray)
} PastelTheme;

// Return the theme struct for a given theme ID.
// On B&W platforms (diorite), all themes collapse to high-contrast White/LightGray.
static inline PastelTheme pastel_get_theme(int theme_id) {
  PastelTheme t;
  t.primary = GColorWhite;
  t.muted   = GColorDarkGray;

#ifdef PBL_COLOR
  switch (theme_id) {
    case THEME_LAVENDER_DREAM:
      t.accent    = GColorBabyBlueEyes;              // #AAAAFF
      t.secondary = GColorRichBrilliantLavender;      // #FFAAFF
      t.highlight = GColorShockingPink;               // #FF55FF
      break;
    case THEME_COOL_OCEAN:
      t.accent    = GColorPictonBlue;                 // #55AAFF
      t.secondary = GColorCeleste;                    // #AAFFFF
      t.highlight = GColorElectricBlue;               // #55FFFF
      break;
    case THEME_FOREST_MEADOW:
      t.accent    = GColorMintGreen;                  // #AAFFAA
      t.secondary = GColorInchworm;                   // #AAFF55
      t.highlight = GColorScreaminGreen;              // #55FF55
      break;
    default: // THEME_WARM_SUNSET
      t.accent    = GColorRajah;                      // #FFAA55
      t.secondary = GColorMelon;                      // #FFAAAA
      t.highlight = GColorBrilliantRose;              // #FF55AA
      break;
  }
#else
  (void)theme_id;
  t.accent    = GColorWhite;
  t.secondary = GColorLightGray;
  t.highlight = GColorWhite;
#endif

  return t;
}
