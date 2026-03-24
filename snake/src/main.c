#include <pebble.h>

#define CELL 6
#define GRID_W (144 / CELL)  // 24
#define GRID_H (168 / CELL)  // 28 (full screen, no status bar)
#define MAX_SNAKE (GRID_W * GRID_H)

#define STORAGE_KEY_HIGH 1

#define DEATH_ANIM_INTERVAL 40  // ms per segment removal
#define BONUS_LIFETIME 50       // ticks before bonus food disappears
#define SPEED_BOOST_DURATION 30 // ticks of speed boost

typedef enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT } Dir;
typedef enum { FOOD_NORMAL, FOOD_BONUS, FOOD_SPEED } FoodType;

typedef struct { int8_t x, y; } Pos;

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_timer;

static Pos s_snake[MAX_SNAKE];
static int s_length;
static Dir s_dir;
static Pos s_food;
static int s_score;
static int s_high_score;
static bool s_game_over;
static bool s_paused;
static char s_score_buf[32];

// Food type variety
static FoodType s_food_type;
static int s_bonus_timer;   // ticks remaining for bonus food
static int s_speed_timer;   // ticks remaining for speed boost
static int s_food_count;    // total foods eaten (for bonus spawn)

// Death animation
static bool s_death_animating;
static int s_death_segments;  // segments still visible during death anim
static AppTimer *s_death_timer;

// BT disconnect alert
static bool s_bt_connected;

// First-launch overlay
static bool s_first_launch;

// --- Snake body gradient coloring ---
static GColor snake_segment_color(int index, int total) {
#ifdef PBL_COLOR
    if (index == 0) return GColorGreen;
    int shade = index * 3 / total;
    switch (shade) {
        case 0: return GColorGreen;
        case 1: return GColorIslamicGreen;
        case 2: return GColorDarkGreen;
        default: return GColorArmyGreen;
    }
#else
    (void)index;
    (void)total;
    return GColorWhite;
#endif
}

// --- Difficulty scaling ---
static int get_tick_interval(void) {
    int base = 200;
    int reduction = (s_score / 5) * 10;
    int interval = base - reduction;
    if (interval < 80) interval = 80;
    // Speed boost: halve the interval
    if (s_speed_timer > 0) interval /= 2;
    return interval;
}

// --- Food placement ---
static void place_food(void) {
    bool valid;
    do {
        valid = true;
        s_food.x = rand() % GRID_W;
        s_food.y = rand() % GRID_H;
        for (int i = 0; i < s_length; i++) {
            if (s_snake[i].x == s_food.x && s_snake[i].y == s_food.y) {
                valid = false;
                break;
            }
        }
    } while (!valid);

    // Determine food type
    s_food_count++;
    if (s_food_count % 10 == 0) {
        s_food_type = FOOD_BONUS;
        s_bonus_timer = BONUS_LIFETIME;
    } else if (s_food_count % 7 == 0) {
        s_food_type = FOOD_SPEED;
        s_bonus_timer = 0;
    } else {
        s_food_type = FOOD_NORMAL;
        s_bonus_timer = 0;
    }
}

static void reset_game(void) {
    s_length = 3;
    s_dir = DIR_RIGHT;
    s_game_over = false;
    s_paused = false;
    s_score = 0;
    s_food_count = 0;
    s_food_type = FOOD_NORMAL;
    s_bonus_timer = 0;
    s_speed_timer = 0;
    s_death_animating = false;
    s_death_segments = 0;
    s_first_launch = false;

    // Start in center
    for (int i = 0; i < s_length; i++) {
        s_snake[i].x = GRID_W / 2 - i;
        s_snake[i].y = GRID_H / 2;
    }

    place_food();
}

// --- Death animation ---
static void death_anim_tick(void *data);

static void trigger_death(void) {
    s_game_over = true;
    if (s_score > s_high_score) {
        s_high_score = s_score;
        persist_write_int(STORAGE_KEY_HIGH, s_high_score);
    }
    // Start death animation
    s_death_animating = true;
    s_death_segments = s_length;
    static const uint32_t segs[] = {300, 100, 300};
    VibePattern pat = { .durations = segs, .num_segments = 3 };
    vibes_enqueue_custom_pattern(pat);
    s_death_timer = app_timer_register(DEATH_ANIM_INTERVAL, death_anim_tick, NULL);
}

static void death_anim_tick(void *data) {
    (void)data;
    if (s_death_segments > 0) {
        s_death_segments--;
        layer_mark_dirty(s_canvas);
        if (s_death_segments > 0) {
            s_death_timer = app_timer_register(DEATH_ANIM_INTERVAL, death_anim_tick, NULL);
        } else {
            s_death_animating = false;
            s_death_timer = NULL;
            layer_mark_dirty(s_canvas);
        }
    }
}

// --- Game step ---
static void step(void) {
    if (s_game_over || s_paused) return;

    // Decrement speed timer
    if (s_speed_timer > 0) s_speed_timer--;

    // Decrement bonus food timer
    if (s_food_type == FOOD_BONUS && s_bonus_timer > 0) {
        s_bonus_timer--;
        if (s_bonus_timer == 0) {
            // Bonus expired, replace with normal food
            s_food_type = FOOD_NORMAL;
            place_food();
        }
    }

    // Calculate new head
    Pos head = s_snake[0];
    switch (s_dir) {
        case DIR_UP:    head.y--; break;
        case DIR_DOWN:  head.y++; break;
        case DIR_LEFT:  head.x--; break;
        case DIR_RIGHT: head.x++; break;
    }

    // Wall collision
    if (head.x < 0 || head.x >= GRID_W || head.y < 0 || head.y >= GRID_H) {
        trigger_death();
        return;
    }

    // Self collision
    for (int i = 0; i < s_length; i++) {
        if (s_snake[i].x == head.x && s_snake[i].y == head.y) {
            trigger_death();
            return;
        }
    }

    // Check food
    bool ate = (head.x == s_food.x && head.y == s_food.y);

    // Move body
    if (!ate) {
        for (int i = s_length - 1; i > 0; i--) {
            s_snake[i] = s_snake[i - 1];
        }
    } else {
        // Grow: shift everything, keep tail
        if (s_length < MAX_SNAKE) {
            for (int i = s_length; i > 0; i--) {
                s_snake[i] = s_snake[i - 1];
            }
            s_length++;
        }
        // Score depends on food type
        switch (s_food_type) {
            case FOOD_BONUS:
                s_score += 3;
                break;
            case FOOD_SPEED:
                s_score += 1;
                s_speed_timer = SPEED_BOOST_DURATION;
                break;
            default:
                s_score += 1;
                break;
        }
        place_food();
        vibes_short_pulse();
    }

    s_snake[0] = head;
}

// --- Draw food ---
static void draw_food(GContext *ctx) {
    int fx = s_food.x * CELL;
    int fy = s_food.y * CELL;

    switch (s_food_type) {
        case FOOD_NORMAL:
            // Circle (filled rect with round corners on color, simple rect on BW)
            graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
            graphics_fill_rect(ctx, GRect(fx + 1, fy + 1, CELL - 2, CELL - 2), 2, GCornersAll);
            break;
        case FOOD_BONUS:
            // Yellow square
            graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
            graphics_fill_rect(ctx, GRect(fx, fy, CELL, CELL), 0, GCornerNone);
            break;
        case FOOD_SPEED:
            // Cyan diamond (draw as rotated square using lines)
            graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));
            {
                int cx = fx + CELL / 2;
                int cy = fy + CELL / 2;
                int r = CELL / 2;
                // Draw diamond with 4 triangles by filling a path-like shape
                GPoint diamond[4] = {
                    {(int16_t)cx, (int16_t)(cy - r)},        // top
                    {(int16_t)(cx + r), (int16_t)cy},        // right
                    {(int16_t)cx, (int16_t)(cy + r)},        // bottom
                    {(int16_t)(cx - r), (int16_t)cy}         // left
                };
                GPathInfo diamond_info = {
                    .num_points = 4,
                    .points = diamond
                };
                GPath *path = gpath_create(&diamond_info);
                gpath_draw_filled(ctx, path);
                gpath_destroy(path);
            }
            break;
    }
}

// --- Canvas update ---
static void canvas_update(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    // Black background
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    if (s_game_over && !s_death_animating) {
        // Game over screen (after death animation finishes)
        graphics_context_set_text_color(ctx, GColorWhite);
        graphics_draw_text(ctx, "GAME OVER",
                           fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                           GRect(0, 40, bounds.size.w, 30),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);

        snprintf(s_score_buf, sizeof(s_score_buf), "Score: %d  Hi: %d", s_score, s_high_score);
        graphics_draw_text(ctx, s_score_buf,
                           fonts_get_system_font(FONT_KEY_GOTHIC_18),
                           GRect(0, 75, bounds.size.w, 24),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);

        graphics_draw_text(ctx, "Select: Restart",
                           fonts_get_system_font(FONT_KEY_GOTHIC_14),
                           GRect(0, 105, bounds.size.w, 20),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);
        return;
    }

    // Draw grid border
    int play_w = GRID_W * CELL;
    int play_h = GRID_H * CELL;
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
    graphics_draw_rect(ctx, GRect(0, 0, play_w, play_h));

    // Draw food (not during death animation)
    if (!s_death_animating) {
        draw_food(ctx);
    }

    // Draw snake (with gradient or death animation)
    int segments_to_draw = s_death_animating ? s_death_segments : s_length;
    for (int i = 0; i < segments_to_draw; i++) {
        GColor color = snake_segment_color(i, s_length);
        graphics_context_set_fill_color(ctx, color);
        graphics_fill_rect(ctx, GRect(s_snake[i].x * CELL, s_snake[i].y * CELL,
                                       CELL - 1, CELL - 1), 0, GCornerNone);
    }

    // Score overlay in top-left corner
    if (!s_death_animating) {
        snprintf(s_score_buf, sizeof(s_score_buf), "%d  Hi:%d", s_score, s_high_score);
        // Draw semi-transparent background for score text
        graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
        graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, 14), 0, GCornerNone);
        graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, GColorWhite));
        graphics_draw_text(ctx, s_score_buf,
                           fonts_get_system_font(FONT_KEY_GOTHIC_14),
                           GRect(4, -2, bounds.size.w - 8, 14),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);

        // Show speed boost indicator
        if (s_speed_timer > 0) {
            graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));
            graphics_draw_text(ctx, "FAST!",
                               fonts_get_system_font(FONT_KEY_GOTHIC_14),
                               GRect(0, -2, bounds.size.w - 4, 14),
                               GTextOverflowModeTrailingEllipsis,
                               GTextAlignmentRight, NULL);
        }
    }

    // Paused overlay
    if (s_paused) {
        graphics_context_set_text_color(ctx, GColorWhite);
        graphics_draw_text(ctx, "PAUSED",
                           fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                           GRect(0, 60, bounds.size.w, 30),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);
    }

    // First-launch overlay
    if (s_first_launch) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_rect(ctx, GRect(10, 30, bounds.size.w - 20, 110), 4, GCornersAll);
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_draw_round_rect(ctx, GRect(10, 30, bounds.size.w - 20, 110), 4);

        graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
        graphics_draw_text(ctx, "SNAKE",
                           fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                           GRect(12, 32, bounds.size.w - 24, 28),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);

        graphics_context_set_text_color(ctx, GColorWhite);
        graphics_draw_text(ctx, "Up: Turn Left\nDown: Turn Right\nSelect: Pause",
                           fonts_get_system_font(FONT_KEY_GOTHIC_18),
                           GRect(16, 60, bounds.size.w - 32, 60),
                           GTextOverflowModeWordWrap,
                           GTextAlignmentCenter, NULL);

        graphics_draw_text(ctx, "Press Select to start",
                           fonts_get_system_font(FONT_KEY_GOTHIC_14),
                           GRect(12, 118, bounds.size.w - 24, 18),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);
    }

    // BT disconnect alert
    if (!s_bt_connected) {
        graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
        graphics_draw_text(ctx, "BT Disconnected",
                           fonts_get_system_font(FONT_KEY_GOTHIC_14),
                           GRect(0, bounds.size.h - 16, bounds.size.w, 16),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);
    }
}

// --- Game loop ---
static void game_loop(void *data) {
    (void)data;
    if (!s_first_launch) {
        step();
    }
    layer_mark_dirty(s_canvas);
    s_timer = app_timer_register(get_tick_interval(), game_loop, NULL);
}

// --- Button handlers ---
static void up_click(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_first_launch) return;
    if (!s_game_over && !s_paused) {
        s_dir = (s_dir + 3) % 4;  // Counter-clockwise
    }
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_first_launch) return;
    if (!s_game_over && !s_paused) {
        s_dir = (s_dir + 1) % 4;  // Clockwise
    }
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
    (void)recognizer;
    (void)context;
    if (s_first_launch) {
        s_first_launch = false;
        layer_mark_dirty(s_canvas);
        return;
    }
    if (s_game_over && !s_death_animating) {
        reset_game();
        layer_mark_dirty(s_canvas);
    } else if (!s_game_over) {
        s_paused = !s_paused;
    }
}

static void click_config(void *context) {
    (void)context;
    window_single_click_subscribe(BUTTON_ID_UP, up_click);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

// --- BT connection handler ---
static void bt_handler(bool connected) {
    s_bt_connected = connected;
    if (!connected) {
        vibes_double_pulse();
    }
    layer_mark_dirty(s_canvas);
}

// --- Window handlers ---
static void window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);

    s_canvas = layer_create(bounds);
    layer_set_update_proc(s_canvas, canvas_update);
    layer_add_child(root, s_canvas);

    if (persist_exists(STORAGE_KEY_HIGH)) {
        s_high_score = persist_read_int(STORAGE_KEY_HIGH);
    }

    s_bt_connected = connection_service_peek_pebble_app_connection();
    connection_service_subscribe((ConnectionHandlers) {
        .pebble_app_connection_handler = bt_handler
    });

    reset_game();
    s_first_launch = true;
    s_paused = true;  // Paused until user dismisses first-launch overlay
}

static void window_unload(Window *window) {
    (void)window;
    connection_service_unsubscribe();
    layer_destroy(s_canvas);
}

// --- Init / Deinit ---
static void init(void) {
    srand(time(NULL));
    s_window = window_create();
    window_set_click_config_provider(s_window, click_config);
    window_set_window_handlers(s_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_window, true);
    s_timer = app_timer_register(get_tick_interval(), game_loop, NULL);
}

static void deinit(void) {
    if (s_timer) app_timer_cancel(s_timer);
    if (s_death_timer) app_timer_cancel(s_death_timer);
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
