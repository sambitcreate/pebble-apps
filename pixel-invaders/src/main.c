#include <pebble.h>

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

// Bullet
#define BULLET_W 2
#define BULLET_H 6
#define BULLET_SPEED 4

#define MAX_BULLETS 3

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_timer;

static int s_player_x;
static int s_score;
static int s_lives;
static bool s_game_over;

// Alien state
static bool s_aliens[ALIEN_ROWS][ALIEN_COLS];
static int s_alien_x;  // top-left x of alien formation
static int s_alien_y;  // top-left y
static int s_alien_dx; // direction: +2 or -2
static int s_aliens_alive;
static int s_move_counter;
static int s_move_speed;  // frames between alien moves

// Bullets
typedef struct {
  int x, y;
  bool active;
} Bullet;
static Bullet s_bullets[MAX_BULLETS];

static char s_score_buf[16];
static char s_lives_buf[8];

static void reset_game(void) {
  s_player_x = SCREEN_W / 2 - PLAYER_W / 2;
  s_score = 0;
  s_lives = 3;
  s_game_over = false;
  s_alien_x = 10;
  s_alien_y = 10;
  s_alien_dx = 2;
  s_aliens_alive = ALIEN_ROWS * ALIEN_COLS;
  s_move_counter = 0;
  s_move_speed = 8;

  for (int r = 0; r < ALIEN_ROWS; r++)
    for (int c = 0; c < ALIEN_COLS; c++)
      s_aliens[r][c] = true;

  for (int i = 0; i < MAX_BULLETS; i++)
    s_bullets[i].active = false;
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

static void check_collisions(void) {
  for (int b = 0; b < MAX_BULLETS; b++) {
    if (!s_bullets[b].active) continue;

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
          s_score += 10 * (ALIEN_ROWS - r); // top rows worth more

          vibes_short_pulse();

          // Speed up
          if (s_aliens_alive > 0) {
            s_move_speed = 2 + (6 * s_aliens_alive) / (ALIEN_ROWS * ALIEN_COLS);
          }

          if (s_aliens_alive == 0) {
            // Wave cleared - reset aliens, keep score
            s_alien_y = 10;
            s_alien_x = 10;
            s_aliens_alive = ALIEN_ROWS * ALIEN_COLS;
            s_move_speed = 8;
            for (int rr = 0; rr < ALIEN_ROWS; rr++)
              for (int cc = 0; cc < ALIEN_COLS; cc++)
                s_aliens[rr][cc] = true;
          }
          return;
        }
      }
    }
  }
}

static void move_aliens(void) {
  s_move_counter++;
  if (s_move_counter < s_move_speed) return;
  s_move_counter = 0;

  s_alien_x += s_alien_dx;

  // Find rightmost and leftmost alive alien
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
    static const uint32_t segs[] = {500, 100, 500};
    VibePattern pat = { .durations = segs, .num_segments = 3 };
    vibes_enqueue_custom_pattern(pat);
  }
}

static void update(void) {
  if (s_game_over) return;

  // Move bullets
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (s_bullets[i].active) {
      s_bullets[i].y -= BULLET_SPEED;
      if (s_bullets[i].y < 0) s_bullets[i].active = false;
    }
  }

  check_collisions();
  move_aliens();
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (s_game_over) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "GAME OVER",
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(0, 50, SCREEN_W, 30),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    snprintf(s_score_buf, sizeof(s_score_buf), "Score: %d", s_score);
    graphics_draw_text(ctx, s_score_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(0, 80, SCREEN_W, 24),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "Select: Restart",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 110, SCREEN_W, 20),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    return;
  }

  // Draw aliens
  graphics_context_set_fill_color(ctx, GColorWhite);
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
      graphics_context_set_fill_color(ctx, GColorWhite);
    }
  }

  // Draw player ship
  graphics_fill_rect(ctx, GRect(s_player_x + 6, PLAYER_Y - 2, 4, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(s_player_x + 2, PLAYER_Y, 12, 4), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(s_player_x, PLAYER_Y + 4, PLAYER_W, 4), 0, GCornerNone);

  // Draw bullets
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (s_bullets[i].active) {
      graphics_fill_rect(ctx, GRect(s_bullets[i].x, s_bullets[i].y, BULLET_W, BULLET_H), 0, GCornerNone);
    }
  }

  // HUD
  graphics_context_set_text_color(ctx, GColorWhite);
  snprintf(s_score_buf, sizeof(s_score_buf), "%d", s_score);
  graphics_draw_text(ctx, s_score_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(2, SCREEN_H - 16, 60, 16),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  snprintf(s_lives_buf, sizeof(s_lives_buf), "x%d", s_lives);
  graphics_draw_text(ctx, s_lives_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(SCREEN_W - 30, SCREEN_H - 16, 28, 16),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);
}

static void game_loop(void *data) {
  update();
  layer_mark_dirty(s_canvas);
  s_timer = app_timer_register(FRAME_MS, game_loop, NULL);
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  if (!s_game_over && s_player_x > 0) {
    s_player_x -= PLAYER_SPEED;
  }
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  if (!s_game_over && s_player_x < SCREEN_W - PLAYER_W) {
    s_player_x += PLAYER_SPEED;
  }
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
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

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  reset_game();
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

static void init(void) {
  srand(time(NULL));
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  s_timer = app_timer_register(FRAME_MS, game_loop, NULL);
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
