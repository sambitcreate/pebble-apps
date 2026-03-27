#include <pebble.h>
#include "../../shared/pebble_pastel.h"

#define SCREEN_W 144
#define SCREEN_H 168
#define FRAME_MS 50

// Alien grid
#define ALIEN_COLS 8
#define ALIEN_ROWS 4
#define ALIEN_W 12
#define ALIEN_H 8
#define ALIEN_PAD 4

// Player
#define PLAYER_W 16
#define PLAYER_H 8
#define PLAYER_Y (SCREEN_H - 20)
#define PLAYER_SPEED 4

// Player bullets
#define BULLET_W 2
#define BULLET_H 6
#define BULLET_SPEED 4
#define MAX_BULLETS 3

// Alien bullets
#define MAX_ALIEN_BULLETS 3
#define ALIEN_BULLET_W 2
#define ALIEN_BULLET_H 4
#define ALIEN_BULLET_SPEED 3

// Explosions
#define MAX_EXPLOSIONS 4

// Shields
#define NUM_SHIELDS 3
#define SHIELD_W 20
#define SHIELD_H 6
#define SHIELD_MAX_HP 3

// High score persistence
#define STORAGE_KEY_HI 1

// Config persistence
#define STORAGE_KEY_LIVES 2
#define STORAGE_KEY_DIFFICULTY 3

// AppMessage keys (must match messageKeys in package.json)
#define MSG_KEY_LIVES 0
#define MSG_KEY_DIFFICULTY 1

// BT overlay / first-launch overlay timing (frames)
#define OVERLAY_DURATION 60  // 3 seconds at 50ms/frame
#define WAVE_BANNER_DURATION 40  // 2 seconds

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_timer;

// Configurable settings (persisted via AppMessage)
static int s_cfg_lives = 3;        // starting lives: 3, 5, or 7
static int s_cfg_difficulty = 1;   // 0=Easy, 1=Normal, 2=Hard

static int s_player_x;
static int s_score;
static int s_lives;
static bool s_game_over;

// Wave system
static int s_wave;
static int s_wave_banner_timer; // countdown frames showing "WAVE N"

// High score
static int s_high_score;
static bool s_new_high_score;

// Alien state
static bool s_aliens[ALIEN_ROWS][ALIEN_COLS];
static int s_alien_x;
static int s_alien_y;
static int s_alien_dx;
static int s_aliens_alive;
static int s_move_counter;
static int s_move_speed;

// Player bullets
typedef struct {
  int x, y;
  bool active;
} Bullet;
static Bullet s_bullets[MAX_BULLETS];

// Alien bullets
static Bullet s_alien_bullets[MAX_ALIEN_BULLETS];

// Explosions
typedef struct {
  int x, y;
  int frame; // 0-3, then deactivated
  bool active;
} Explosion;
static Explosion s_explosions[MAX_EXPLOSIONS];

// Shields
static int s_shield_hp[NUM_SHIELDS];

// BT disconnect alert
static bool s_bt_disconnected;
static int s_bt_alert_timer;

// First-launch overlay
static bool s_first_launch;
static int s_first_launch_timer;

// Frame counter for alien fire timing
static int s_frame_counter;

static char s_score_buf[16];
static char s_lives_buf[8];
static char s_wave_buf[16];
static char s_hi_buf[24];

// Pastel theme
static PastelTheme s_theme;
static int s_theme_id = THEME_LAVENDER_DREAM;  // default for games

// Shield positions (computed once)
static int shield_x(int i) {
  // Evenly space 3 shields across the screen
  int total_gap = SCREEN_W - NUM_SHIELDS * SHIELD_W;
  int gap = total_gap / (NUM_SHIELDS + 1);
  return gap + i * (SHIELD_W + gap);
}

#define SHIELD_Y (PLAYER_Y - 20)

static void spawn_explosion(int x, int y) {
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (!s_explosions[i].active) {
      s_explosions[i].x = x;
      s_explosions[i].y = y;
      s_explosions[i].frame = 0;
      s_explosions[i].active = true;
      return;
    }
  }
}

static void reset_shields(void) {
  for (int i = 0; i < NUM_SHIELDS; i++) {
    s_shield_hp[i] = SHIELD_MAX_HP;
  }
}

// Initial move speed based on difficulty: Easy=10, Normal=8, Hard=5
static int difficulty_move_speed(void) {
  switch (s_cfg_difficulty) {
    case 0:  return 10; // Easy
    case 2:  return 5;  // Hard
    default: return 8;  // Normal
  }
}

static void spawn_aliens(void) {
  s_alien_x = 10;
  s_alien_y = 10;
  s_alien_dx = 2;
  s_aliens_alive = ALIEN_ROWS * ALIEN_COLS;
  s_move_counter = 0;
  s_move_speed = difficulty_move_speed();

  for (int r = 0; r < ALIEN_ROWS; r++)
    for (int c = 0; c < ALIEN_COLS; c++)
      s_aliens[r][c] = true;
}

static void reset_game(void) {
  s_player_x = SCREEN_W / 2 - PLAYER_W / 2;
  s_score = 0;
  s_lives = s_cfg_lives;
  s_game_over = false;
  s_new_high_score = false;
  s_wave = 1;
  s_wave_banner_timer = WAVE_BANNER_DURATION;
  s_frame_counter = 0;

  spawn_aliens();
  reset_shields();

  for (int i = 0; i < MAX_BULLETS; i++)
    s_bullets[i].active = false;

  for (int i = 0; i < MAX_ALIEN_BULLETS; i++)
    s_alien_bullets[i].active = false;

  for (int i = 0; i < MAX_EXPLOSIONS; i++)
    s_explosions[i].active = false;
}

static void fire_bullet(void) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!s_bullets[i].active) {
      s_bullets[i].x = s_player_x + PLAYER_W / 2 - BULLET_W / 2;
      s_bullets[i].y = PLAYER_Y - BULLET_H;
      s_bullets[i].active = true;
      return;
    }
  }
}

static void alien_fire(void) {
  // Determine fire probability based on wave: 1 in (30 - wave*2), min 1 in 10
  int chance = 30 - s_wave * 2;
  if (chance < 10) chance = 10;

  if (rand() % chance != 0) return;

  // Find a free alien bullet slot
  int slot = -1;
  for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
    if (!s_alien_bullets[i].active) {
      slot = i;
      break;
    }
  }
  if (slot < 0) return;

  // Pick a random alive alien from the bottom of each column
  // First, collect bottom-row aliens for each column
  int candidates_x[ALIEN_COLS];
  int candidates_y[ALIEN_COLS];
  int num_candidates = 0;

  for (int c = 0; c < ALIEN_COLS; c++) {
    for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
      if (s_aliens[r][c]) {
        candidates_x[num_candidates] = s_alien_x + c * (ALIEN_W + ALIEN_PAD) + ALIEN_W / 2;
        candidates_y[num_candidates] = s_alien_y + r * (ALIEN_H + ALIEN_PAD) + ALIEN_H;
        num_candidates++;
        break;
      }
    }
  }

  if (num_candidates == 0) return;

  int pick = rand() % num_candidates;
  s_alien_bullets[slot].x = candidates_x[pick] - ALIEN_BULLET_W / 2;
  s_alien_bullets[slot].y = candidates_y[pick];
  s_alien_bullets[slot].active = true;
}

// Check if a rect overlaps a shield, return shield index or -1
static int check_shield_hit(int bx, int by, int bw, int bh) {
  for (int i = 0; i < NUM_SHIELDS; i++) {
    if (s_shield_hp[i] <= 0) continue;
    int sx = shield_x(i);
    if (bx + bw > sx && bx < sx + SHIELD_W &&
        by + bh > SHIELD_Y && by < SHIELD_Y + SHIELD_H) {
      return i;
    }
  }
  return -1;
}

static void check_collisions(void) {
  // Player bullets vs aliens
  for (int b = 0; b < MAX_BULLETS; b++) {
    if (!s_bullets[b].active) continue;

    // Check vs shields first
    int si = check_shield_hit(s_bullets[b].x, s_bullets[b].y, BULLET_W, BULLET_H);
    if (si >= 0) {
      s_shield_hp[si]--;
      s_bullets[b].active = false;
      continue;
    }

    for (int r = 0; r < ALIEN_ROWS; r++) {
      for (int c = 0; c < ALIEN_COLS; c++) {
        if (!s_aliens[r][c]) continue;

        int ax = s_alien_x + c * (ALIEN_W + ALIEN_PAD);
        int ay = s_alien_y + r * (ALIEN_H + ALIEN_PAD);

        if (s_bullets[b].x + BULLET_W > ax &&
            s_bullets[b].x < ax + ALIEN_W &&
            s_bullets[b].y < ay + ALIEN_H &&
            s_bullets[b].y + BULLET_H > ay) {
          // Hit!
          s_aliens[r][c] = false;
          s_bullets[b].active = false;
          s_aliens_alive--;
          s_score += 10 * (ALIEN_ROWS - r);

          // Spawn explosion at alien center
          spawn_explosion(ax + ALIEN_W / 2, ay + ALIEN_H / 2);
          vibes_short_pulse();

          // Speed up
          if (s_aliens_alive > 0) {
            s_move_speed = 2 + (6 * s_aliens_alive) / (ALIEN_ROWS * ALIEN_COLS);
          }

          // Wave cleared
          if (s_aliens_alive == 0) {
            s_wave++;
            s_wave_banner_timer = WAVE_BANNER_DURATION;
            spawn_aliens();
            reset_shields();
            // Clear all alien bullets on new wave
            for (int i = 0; i < MAX_ALIEN_BULLETS; i++)
              s_alien_bullets[i].active = false;
          }
          goto next_bullet;
        }
      }
    }
    next_bullet:;
  }

  // Alien bullets vs player and shields
  for (int b = 0; b < MAX_ALIEN_BULLETS; b++) {
    if (!s_alien_bullets[b].active) continue;

    // Check vs shields
    int si = check_shield_hit(s_alien_bullets[b].x, s_alien_bullets[b].y,
                               ALIEN_BULLET_W, ALIEN_BULLET_H);
    if (si >= 0) {
      s_shield_hp[si]--;
      s_alien_bullets[b].active = false;
      continue;
    }

    // Check vs player
    if (s_alien_bullets[b].x + ALIEN_BULLET_W > s_player_x &&
        s_alien_bullets[b].x < s_player_x + PLAYER_W &&
        s_alien_bullets[b].y + ALIEN_BULLET_H > PLAYER_Y &&
        s_alien_bullets[b].y < PLAYER_Y + PLAYER_H) {
      // Player hit!
      s_alien_bullets[b].active = false;
      s_lives--;
      vibes_short_pulse();

      if (s_lives <= 0) {
        s_game_over = true;
        // Check high score
        if (s_score > s_high_score) {
          s_high_score = s_score;
          s_new_high_score = true;
          persist_write_int(STORAGE_KEY_HI, s_high_score);
        }
        static const uint32_t segs[] = {500, 100, 500};
        VibePattern pat = { .durations = segs, .num_segments = 3 };
        vibes_enqueue_custom_pattern(pat);
      }
    }
  }
}

static void move_aliens(void) {
  s_move_counter++;
  if (s_move_counter < s_move_speed) return;
  s_move_counter = 0;

  s_alien_x += s_alien_dx;

  int left_edge = ALIEN_COLS, right_edge = -1;
  int bottom_edge = -1;
  for (int r = 0; r < ALIEN_ROWS; r++) {
    for (int c = 0; c < ALIEN_COLS; c++) {
      if (s_aliens[r][c]) {
        if (c < left_edge) left_edge = c;
        if (c > right_edge) right_edge = c;
        if (r > bottom_edge) bottom_edge = r;
      }
    }
  }

  if (right_edge < 0) return; // no aliens alive

  int actual_left = s_alien_x + left_edge * (ALIEN_W + ALIEN_PAD);
  int actual_right = s_alien_x + right_edge * (ALIEN_W + ALIEN_PAD) + ALIEN_W;

  if (actual_right >= SCREEN_W - 2 || actual_left <= 2) {
    s_alien_dx = -s_alien_dx;
    s_alien_y += ALIEN_H;
  }

  // Check if aliens reached player
  int actual_bottom = s_alien_y + bottom_edge * (ALIEN_H + ALIEN_PAD) + ALIEN_H;
  if (actual_bottom >= PLAYER_Y) {
    s_game_over = true;
    // Check high score
    if (s_score > s_high_score) {
      s_high_score = s_score;
      s_new_high_score = true;
      persist_write_int(STORAGE_KEY_HI, s_high_score);
    }
    static const uint32_t segs[] = {500, 100, 500};
    VibePattern pat = { .durations = segs, .num_segments = 3 };
    vibes_enqueue_custom_pattern(pat);
  }
}

static void update(void) {
  // Tick down overlays even during wave banner
  if (s_first_launch && s_first_launch_timer > 0) {
    s_first_launch_timer--;
    if (s_first_launch_timer <= 0) s_first_launch = false;
    return; // Pause game during first-launch overlay
  }

  if (s_bt_alert_timer > 0) {
    s_bt_alert_timer--;
  }

  if (s_game_over) return;

  // Wave banner countdown (game paused during banner)
  if (s_wave_banner_timer > 0) {
    s_wave_banner_timer--;
    return;
  }

  s_frame_counter++;

  // Move player bullets
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (s_bullets[i].active) {
      s_bullets[i].y -= BULLET_SPEED;
      if (s_bullets[i].y < 0) s_bullets[i].active = false;
    }
  }

  // Move alien bullets
  for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
    if (s_alien_bullets[i].active) {
      s_alien_bullets[i].y += ALIEN_BULLET_SPEED;
      if (s_alien_bullets[i].y > SCREEN_H) s_alien_bullets[i].active = false;
    }
  }

  // Step explosions
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (s_explosions[i].active) {
      s_explosions[i].frame++;
      if (s_explosions[i].frame > 3) {
        s_explosions[i].active = false;
      }
    }
  }

  // Alien fire
  alien_fire();

  check_collisions();
  move_aliens();
}

static void draw_explosion(GContext *ctx, Explosion *e) {
  int cx = e->x;
  int cy = e->y;
  int r = e->frame + 1; // radius grows 1..4

  // Draw expanding X/burst pattern
  graphics_context_set_fill_color(ctx, s_theme.highlight);

  // Center dot
  graphics_fill_rect(ctx, GRect(cx - 1, cy - 1, 2, 2), 0, GCornerNone);

  // Four diagonal arms expanding outward
  for (int d = 1; d <= r; d++) {
    graphics_fill_rect(ctx, GRect(cx + d, cy + d, 2, 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(cx - d - 1, cy + d, 2, 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(cx + d, cy - d - 1, 2, 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(cx - d - 1, cy - d - 1, 2, 2), 0, GCornerNone);
  }

  // Cross arms
  if (r >= 2) {
    graphics_fill_rect(ctx, GRect(cx - r, cy - 1, r * 2, 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(cx - 1, cy - r, 2, r * 2), 0, GCornerNone);
  }
}

static void draw_shield(GContext *ctx, int i) {
  if (s_shield_hp[i] <= 0) return;

  int sx = shield_x(i);
  int hp = s_shield_hp[i];

  // Draw shield - visual degradation based on HP
  graphics_context_set_fill_color(ctx, s_theme.muted);

  if (hp == 3) {
    // Full shield - solid rect
    graphics_fill_rect(ctx, GRect(sx, SHIELD_Y, SHIELD_W, SHIELD_H), 0, GCornerNone);
  } else if (hp == 2) {
    // Damaged - gaps in middle
    graphics_fill_rect(ctx, GRect(sx, SHIELD_Y, SHIELD_W, 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(sx, SHIELD_Y + 2, 4, SHIELD_H - 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(sx + SHIELD_W - 4, SHIELD_Y + 2, 4, SHIELD_H - 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(sx + 8, SHIELD_Y + 4, 4, 2), 0, GCornerNone);
  } else if (hp == 1) {
    // Critical - sparse fragments
    graphics_fill_rect(ctx, GRect(sx, SHIELD_Y, 3, 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(sx + SHIELD_W - 3, SHIELD_Y, 3, 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(sx + 8, SHIELD_Y + 3, 4, 3), 0, GCornerNone);
  }
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // First-launch overlay
  if (s_first_launch && s_first_launch_timer > 0) {
    graphics_context_set_text_color(ctx, s_theme.accent);
    graphics_draw_text(ctx, "PIXEL INVADERS",
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(0, 30, SCREEN_W, 30),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, s_theme.primary);
    graphics_draw_text(ctx, "UP: Move Left",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 70, SCREEN_W, 20),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "DOWN: Move Right",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 88, SCREEN_W, 20),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "SELECT: Fire",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 106, SCREEN_W, 20),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "Press SELECT to start",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 134, SCREEN_W, 20),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    return;
  }

  if (s_game_over) {
    graphics_context_set_text_color(ctx, s_theme.accent);
    graphics_draw_text(ctx, "GAME OVER",
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(0, 35, SCREEN_W, 30),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, s_theme.primary);
    snprintf(s_score_buf, sizeof(s_score_buf), "Score: %d", s_score);
    graphics_draw_text(ctx, s_score_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(0, 65, SCREEN_W, 24),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    if (s_new_high_score) {
      graphics_context_set_text_color(ctx, s_theme.highlight);
      graphics_draw_text(ctx, "NEW HIGH SCORE!",
                         fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                         GRect(0, 88, SCREEN_W, 24),
                         GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
    } else {
      snprintf(s_hi_buf, sizeof(s_hi_buf), "Best: %d", s_high_score);
      graphics_draw_text(ctx, s_hi_buf,
                         fonts_get_system_font(FONT_KEY_GOTHIC_14),
                         GRect(0, 90, SCREEN_W, 20),
                         GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
    }

    graphics_draw_text(ctx, "Select: Restart",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 120, SCREEN_W, 20),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    return;
  }

  // Wave banner overlay (drawn on top of game)
  if (s_wave_banner_timer > 0) {
    graphics_context_set_text_color(ctx, s_theme.accent);
    snprintf(s_wave_buf, sizeof(s_wave_buf), "WAVE %d", s_wave);
    graphics_draw_text(ctx, s_wave_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(0, 60, SCREEN_W, 30),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    return;
  }

  // Draw aliens
  graphics_context_set_fill_color(ctx, s_theme.secondary);
  for (int r = 0; r < ALIEN_ROWS; r++) {
    for (int c = 0; c < ALIEN_COLS; c++) {
      if (!s_aliens[r][c]) continue;
      int ax = s_alien_x + c * (ALIEN_W + ALIEN_PAD);
      int ay = s_alien_y + r * (ALIEN_H + ALIEN_PAD);

      // Simple alien sprite: body + antennae
      graphics_fill_rect(ctx, GRect(ax + 2, ay + 2, ALIEN_W - 4, ALIEN_H - 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(ax, ay + 4, ALIEN_W, ALIEN_H - 4), 0, GCornerNone);
      // Eyes (black pixels)
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, GRect(ax + 3, ay + 4, 2, 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(ax + 7, ay + 4, 2, 2), 0, GCornerNone);
      graphics_context_set_fill_color(ctx, s_theme.secondary);
    }
  }

  // Draw shields
  for (int i = 0; i < NUM_SHIELDS; i++) {
    draw_shield(ctx, i);
  }

  // Draw player ship
  graphics_context_set_fill_color(ctx, s_theme.accent);
  graphics_fill_rect(ctx, GRect(s_player_x + 6, PLAYER_Y - 2, 4, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(s_player_x + 2, PLAYER_Y, 12, 4), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(s_player_x, PLAYER_Y + 4, PLAYER_W, 4), 0, GCornerNone);

  // Draw player bullets
  graphics_context_set_fill_color(ctx, s_theme.highlight);
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (s_bullets[i].active) {
      graphics_fill_rect(ctx, GRect(s_bullets[i].x, s_bullets[i].y, BULLET_W, BULLET_H), 0, GCornerNone);
    }
  }

  // Draw alien bullets (dashed look - 2 px wide, alternating)
  graphics_context_set_fill_color(ctx, s_theme.highlight);
  for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
    if (s_alien_bullets[i].active) {
      graphics_fill_rect(ctx, GRect(s_alien_bullets[i].x, s_alien_bullets[i].y,
                                     ALIEN_BULLET_W, 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(s_alien_bullets[i].x, s_alien_bullets[i].y + 3,
                                     ALIEN_BULLET_W, 1), 0, GCornerNone);
    }
  }

  // Draw explosions
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (s_explosions[i].active) {
      draw_explosion(ctx, &s_explosions[i]);
    }
  }

  // HUD: score left, wave center, lives right
  graphics_context_set_text_color(ctx, s_theme.primary);
  snprintf(s_score_buf, sizeof(s_score_buf), "%d", s_score);
  graphics_draw_text(ctx, s_score_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(2, SCREEN_H - 16, 50, 16),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  snprintf(s_wave_buf, sizeof(s_wave_buf), "W%d", s_wave);
  graphics_draw_text(ctx, s_wave_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(50, SCREEN_H - 16, 44, 16),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  snprintf(s_lives_buf, sizeof(s_lives_buf), "x%d", s_lives);
  graphics_draw_text(ctx, s_lives_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(SCREEN_W - 30, SCREEN_H - 16, 28, 16),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);

  // BT disconnect alert overlay (drawn on top)
  if (s_bt_disconnected || s_bt_alert_timer > 0) {
    graphics_context_set_text_color(ctx, s_theme.highlight);
    graphics_draw_text(ctx, "BT Lost",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 0, SCREEN_W, 16),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void game_loop(void *data) {
  update();
  layer_mark_dirty(s_canvas);
  s_timer = app_timer_register(FRAME_MS, game_loop, NULL);
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (s_first_launch) return;
  if (!s_game_over && s_player_x > 0) {
    s_player_x -= PLAYER_SPEED;
  }
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (s_first_launch) return;
  if (!s_game_over && s_player_x < SCREEN_W - PLAYER_W) {
    s_player_x += PLAYER_SPEED;
  }
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_first_launch) {
    s_first_launch = false;
    s_first_launch_timer = 0;
    return;
  }
  if (s_game_over) {
    reset_game();
  } else {
    fire_bullet();
  }
}

static void up_repeat(ClickRecognizerRef recognizer, void *context) {
  up_click(recognizer, context);
}

static void down_repeat(ClickRecognizerRef recognizer, void *context) {
  down_click(recognizer, context);
}

static void click_config(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 50, up_repeat);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 50, down_repeat);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

// AppMessage: receive config from phone
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *lives_t = dict_find(iter, MSG_KEY_LIVES);
  if (lives_t) {
    int val = lives_t->value->int32;
    if (val == 3 || val == 5 || val == 7) {
      s_cfg_lives = val;
      persist_write_int(STORAGE_KEY_LIVES, s_cfg_lives);
    }
  }

  Tuple *diff_t = dict_find(iter, MSG_KEY_DIFFICULTY);
  if (diff_t) {
    int val = diff_t->value->int32;
    if (val >= 0 && val <= 2) {
      s_cfg_difficulty = val;
      persist_write_int(STORAGE_KEY_DIFFICULTY, s_cfg_difficulty);
    }
  }

  Tuple *theme_t = dict_find(iter, PASTEL_MSG_KEY_THEME);
  if (theme_t) {
    s_theme_id = theme_t->value->int32;
    persist_write_int(PASTEL_STORAGE_KEY_THEME, s_theme_id);
    s_theme = pastel_get_theme(s_theme_id);
    layer_mark_dirty(s_canvas);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

static void bt_handler(bool connected) {
  if (!connected) {
    s_bt_disconnected = true;
    s_bt_alert_timer = OVERLAY_DURATION;
    vibes_double_pulse();
  } else {
    s_bt_disconnected = false;
    s_bt_alert_timer = 0;
  }
  layer_mark_dirty(s_canvas);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  // Load high score
  if (persist_exists(STORAGE_KEY_HI)) {
    s_high_score = persist_read_int(STORAGE_KEY_HI);
  } else {
    s_high_score = 0;
  }

  // Show first-launch overlay
  s_first_launch = true;
  s_first_launch_timer = OVERLAY_DURATION * 3; // stays until SELECT pressed

  reset_game();

  // Subscribe to BT events
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_handler
  });
  s_bt_disconnected = !connection_service_peek_pebble_app_connection();
}

static void window_unload(Window *window) {
  connection_service_unsubscribe();
  layer_destroy(s_canvas);
}

static void init(void) {
  srand(time(NULL));

  // Load persisted config
  if (persist_exists(STORAGE_KEY_LIVES)) {
    s_cfg_lives = persist_read_int(STORAGE_KEY_LIVES);
  }
  if (persist_exists(STORAGE_KEY_DIFFICULTY)) {
    s_cfg_difficulty = persist_read_int(STORAGE_KEY_DIFFICULTY);
  }

  // Load persisted theme
  if (persist_exists(PASTEL_STORAGE_KEY_THEME)) {
    s_theme_id = persist_read_int(PASTEL_STORAGE_KEY_THEME);
  }
  s_theme = pastel_get_theme(s_theme_id);

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  s_timer = app_timer_register(FRAME_MS, game_loop, NULL);

  // Set up AppMessage for config
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(128, 64);
}

static void deinit(void) {
  if (s_timer) app_timer_cancel(s_timer);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
