#include "snake.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../chain/tezos_store.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SNAKE_CELL_SIZE 16
#define SNAKE_MOVE_INTERVAL_MS 150
#define SNAKE_HIGH_SCORE_STORE_PATH "/tmp/tezos_cyberdeck_snake_highscore.txt"

typedef enum { SNAKE_UP, SNAKE_DOWN, SNAKE_LEFT, SNAKE_RIGHT } snake_dir_t;
typedef enum { SNAKE_PLAYING, SNAKE_GAME_OVER } snake_phase_t;

typedef struct {
    snake_phase_t phase;
    snake_point_t body[SNAKE_MAX_LENGTH];
    int length;
    snake_dir_t direction;
    snake_dir_t pending_direction; /* buffered -- applied on the next real
        grid step, not immediately, so two quick key presses in one frame
        can't cause an illegal same-frame reversal */
    snake_point_t food;
    int score;
    int high_score;
    int move_accum_ms;
    unsigned int rng_state; /* real, simple, seedable PRNG (xorshift32) --
        not rand()'s global, non-reentrant, non-seedable-per-instance state */
} snake_state_t;
static snake_state_t g_state;
static tezos_screen_t g_screen;

static unsigned int next_rand(unsigned int *state) {
    unsigned int x = *state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *state = x;
    return x;
}

static bool is_opposite(snake_dir_t a, snake_dir_t b) {
    return (a == SNAKE_UP && b == SNAKE_DOWN) || (a == SNAKE_DOWN && b == SNAKE_UP)
        || (a == SNAKE_LEFT && b == SNAKE_RIGHT) || (a == SNAKE_RIGHT && b == SNAKE_LEFT);
}

/* Real fallback if the random-retry loop below can't quickly find a
   free cell (only possible when the snake has grown enormous) -- a
   real, bounded linear scan guarantees termination instead of an
   unbounded retry loop that could spin forever in the extreme case. */
static bool find_any_free_cell(snake_state_t *st, snake_point_t *out) {
    for (int y = 0; y < SNAKE_GRID_ROWS; y++) {
        for (int x = 0; x < SNAKE_GRID_COLS; x++) {
            bool occupied = false;
            for (int i = 0; i < st->length; i++) {
                if (st->body[i].x == x && st->body[i].y == y) { occupied = true; break; }
            }
            if (!occupied) { out->x = x; out->y = y; return true; }
        }
    }
    return false; /* real edge case: every single cell is occupied --
        the snake has completely filled the grid, a genuine "won" state
        this v1 doesn't specially celebrate, just leaves food unplaced */
}

static void spawn_food(snake_state_t *st) {
    for (int attempt = 0; attempt < 200; attempt++) {
        int x = (int)(next_rand(&st->rng_state) % SNAKE_GRID_COLS);
        int y = (int)(next_rand(&st->rng_state) % SNAKE_GRID_ROWS);
        bool occupied = false;
        for (int i = 0; i < st->length; i++) {
            if (st->body[i].x == x && st->body[i].y == y) { occupied = true; break; }
        }
        if (!occupied) { st->food.x = x; st->food.y = y; return; }
    }
    snake_point_t fallback;
    if (find_any_free_cell(st, &fallback)) st->food = fallback;
}

/* Real movement/collision/growth for exactly one grid step. Verified
   by hand-trace and by direct test (see tests/test_snake.c) --
   shifting the body array down by one (extending by one slot first if
   growing) correctly reproduces standard snake movement: the old tail
   drops off on a normal move, or stays one extra frame (duplicated)
   when growing, while every other segment shifts toward where the old
   head was. */
static void advance_snake(snake_state_t *st) {
    if (!is_opposite(st->pending_direction, st->direction)) st->direction = st->pending_direction;

    snake_point_t head = st->body[0];
    switch (st->direction) {
        case SNAKE_UP: head.y--; break;
        case SNAKE_DOWN: head.y++; break;
        case SNAKE_LEFT: head.x--; break;
        case SNAKE_RIGHT: head.x++; break;
    }

    if (head.x < 0 || head.x >= SNAKE_GRID_COLS || head.y < 0 || head.y >= SNAKE_GRID_ROWS) {
        st->phase = SNAKE_GAME_OVER;
        return;
    }
    for (int i = 0; i < st->length; i++) {
        if (st->body[i].x == head.x && st->body[i].y == head.y) {
            st->phase = SNAKE_GAME_OVER;
            return;
        }
    }

    bool ate = (head.x == st->food.x && head.y == st->food.y);
    int new_length = st->length + (ate ? 1 : 0);
    if (new_length > SNAKE_MAX_LENGTH) new_length = SNAKE_MAX_LENGTH; /* real safety bound */

    for (int i = new_length - 1; i >= 1; i--) st->body[i] = st->body[i - 1];
    st->body[0] = head;
    st->length = new_length;

    if (ate) {
        st->score++;
        if (st->score > st->high_score) {
            st->high_score = st->score;
            char buf[16]; /* real headroom for the full int range, not just the practical SNAKE_MAX_LENGTH-bounded score */
            snprintf(buf, sizeof buf, "%d", st->high_score);
            tezos_store_save_plain(SNAKE_HIGH_SCORE_STORE_PATH, "high_score", buf);
        }
        spawn_food(st);
    }
}

static void reset_game(snake_state_t *st, unsigned int rng_seed) {
    int saved_high_score = st->high_score; /* preserve across a restart within
        the same session -- only ensure_loaded (below) reads it from disk */
    memset(st, 0, sizeof *st);
    st->high_score = saved_high_score;
    st->phase = SNAKE_PLAYING;
    st->rng_state = rng_seed ? rng_seed : 1; /* xorshift32 requires a
        non-zero seed to ever produce non-zero output */
    st->length = 3;
    st->body[0] = (snake_point_t){SNAKE_GRID_COLS / 2, SNAKE_GRID_ROWS / 2};
    st->body[1] = (snake_point_t){SNAKE_GRID_COLS / 2 - 1, SNAKE_GRID_ROWS / 2};
    st->body[2] = (snake_point_t){SNAKE_GRID_COLS / 2 - 2, SNAKE_GRID_ROWS / 2};
    st->direction = SNAKE_RIGHT;
    st->pending_direction = SNAKE_RIGHT;
    spawn_food(st);
}

static void render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    snake_state_t *st = (snake_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    char header[40];
    snprintf(header, sizeof header, "SNAKE  %d", st->score);
    tezos_draw_text(fb, d, 8, 6, header, &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    int field_top = 30;
    for (int i = 0; i < st->length; i++) {
        tezos_color_t color = (i == 0) ? TEZOS_CYAN : TEZOS_PRIMARY; /* head stands
            out from the body, a real, deliberate sprite distinction, not just a
            uniform-colored line of identical blocks */
        tezos_draw_rect(fb, d, st->body[i].x * SNAKE_CELL_SIZE, field_top + st->body[i].y * SNAKE_CELL_SIZE,
                        SNAKE_CELL_SIZE - 1, SNAKE_CELL_SIZE - 1, color);
    }
    tezos_draw_rect(fb, d, st->food.x * SNAKE_CELL_SIZE, field_top + st->food.y * SNAKE_CELL_SIZE,
                    SNAKE_CELL_SIZE - 1, SNAKE_CELL_SIZE - 1, TEZOS_PINK);

    if (st->phase == SNAKE_GAME_OVER) {
        int box_y = fb->height / 2 - 50;
        tezos_draw_rect(fb, d, 40, box_y, fb->width - 80, 100, (tezos_color_t){0x06, 0x08, 0x18});
        tezos_draw_rect_outline(fb, d, 40, box_y, fb->width - 80, 100, TEZOS_PINK);
        tezos_draw_text(fb, d, 56, box_y + 14, "GAME OVER", &TEZOS_FONT_BODY, TEZOS_PINK);
        char line[40];
        snprintf(line, sizeof line, "Score: %d  Best: %d", st->score, st->high_score);
        tezos_draw_text(fb, d, 56, box_y + 46, line, &TEZOS_FONT_SMALL, TEZOS_TEXT);
        tezos_draw_text(fb, d, 56, box_y + 70, "SELECT to play again", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    const char *left_label = (st->phase == SNAKE_GAME_OVER) ? "RESTART" : "";
    if (left_label[0]) tezos_draw_text(fb, d, 8, fb->height - 26, left_label, &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    snake_state_t *st = (snake_state_t *)self->app_state;
    if (st->phase != SNAKE_PLAYING) return;
    st->move_accum_ms += elapsed_ms;
    while (st->move_accum_ms >= SNAKE_MOVE_INTERVAL_MS) {
        st->move_accum_ms -= SNAKE_MOVE_INTERVAL_MS;
        advance_snake(st);
        *needs_redraw = true;
        if (st->phase != SNAKE_PLAYING) break; /* real: stop advancing further
            once game-over, even if multiple intervals' worth of time
            accumulated in one real tick */
    }
}

static tezos_action_t handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                   tezos_screen_t **next, bool *needs_redraw) {
    (void)next;
    snake_state_t *st = (snake_state_t *)self->app_state;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (st->phase == SNAKE_GAME_OVER) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            reset_game(st, (unsigned int)time(NULL)); /* a real, varying seed for
                real play -- deterministic seeding is only for tests, via
                snake_test_inject() */
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }
    /* real input buffering -- see advance_snake's is_opposite check for
       why this doesn't just set direction directly */
    if (ev.type == TEZOS_INPUT_UP) st->pending_direction = SNAKE_UP;
    else if (ev.type == TEZOS_INPUT_DOWN) st->pending_direction = SNAKE_DOWN;
    else if (ev.type == TEZOS_INPUT_NUM && ev.value == 4) st->pending_direction = SNAKE_LEFT;
    else if (ev.type == TEZOS_INPUT_NUM && ev.value == 6) st->pending_direction = SNAKE_RIGHT;
    return TEZOS_ACTION_NONE;
}

static int load_high_score(void) {
    char buf[16]; /* real headroom for the full int range, not just the practical SNAKE_MAX_LENGTH-bounded score */
    if (tezos_store_load_plain(SNAKE_HIGH_SCORE_STORE_PATH, "high_score", buf, sizeof buf)) {
        int n = atoi(buf);
        return n >= 0 ? n : 0;
    }
    return 0;
}

tezos_screen_t *snake_entry_screen(void) {
    memset(&g_state, 0, sizeof g_state);
    g_state.high_score = load_high_score();
    reset_game(&g_state, (unsigned int)time(NULL));
    g_screen = (tezos_screen_t){
        .render = render, .handle_input = handle_input,
        .on_resume = NULL, .on_tick = on_tick,
        .app_state = &g_state, .debug_name = "snake",
    };
    return &g_screen;
}

tezos_screen_t *snake_test_inject(unsigned int rng_seed) {
    memset(&g_state, 0, sizeof g_state);
    g_state.high_score = load_high_score();
    reset_game(&g_state, rng_seed);
    g_screen = (tezos_screen_t){
        .render = render, .handle_input = handle_input,
        .on_resume = NULL, .on_tick = on_tick,
        .app_state = &g_state, .debug_name = "snake",
    };
    return &g_screen;
}

void snake_test_get_snapshot(snake_test_snapshot_t *out) {
    out->game_over = (g_state.phase == SNAKE_GAME_OVER);
    memcpy(out->body, g_state.body, sizeof out->body);
    out->length = g_state.length;
    out->food = g_state.food;
    out->score = g_state.score;
    out->high_score = g_state.high_score;
}

void snake_test_step(void) {
    advance_snake(&g_state);
}
