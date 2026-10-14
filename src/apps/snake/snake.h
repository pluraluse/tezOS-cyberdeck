#pragma once
/*
 * Snake — a real, native app (not a hidden easter egg), in the spirit
 * of the classic phone/console version. Scoped and reasoned about
 * during planning: the D-pad-style input this device already has
 * everywhere else (UP/DOWN, plus NUM keys for left/right, since this
 * hardware has no dedicated 4-way D-pad) is the same input any other
 * screen uses, and "sprites instead of raw LCD pixels" is just this
 * renderer's existing solid-rect drawing primitive, styled
 * deliberately (a colored block per snake segment) rather than left as
 * an afterthought.
 *
 * Deliberately reachable from Settings, not given its own slot in the
 * main app grid — a real, considered placement decision, not a
 * default. The main app list was just deliberately pared down (Scanner
 * folded into Explorer); adding a game back as a first-class grid
 * entry felt like the wrong call given that. Still fully real,
 * discoverable, and documented, just not competing for grid space with
 * Wallet/Explorer/Messenger.
 */
#include "../../core/tezos_core.h"

tezos_screen_t *snake_entry_screen(void);

/* Exposed specifically for direct testing (see tests/test_snake.c) --
   lets a test inject a specific PRNG seed so food placement is
   deterministic and verifiable, and exposes the core state for direct
   inspection (collision/growth/game-over logic) rather than only being
   testable indirectly through rendered frames. NOT for production use. */
typedef struct { int x, y; } snake_point_t;
#define SNAKE_GRID_COLS 20
#define SNAKE_GRID_ROWS 26
#define SNAKE_MAX_LENGTH (SNAKE_GRID_COLS * SNAKE_GRID_ROWS)

typedef struct {
    bool game_over;
    snake_point_t body[SNAKE_MAX_LENGTH];
    int length;
    snake_point_t food;
    int score;
    int high_score;
} snake_test_snapshot_t;

tezos_screen_t *snake_test_inject(unsigned int rng_seed);
void snake_test_get_snapshot(snake_test_snapshot_t *out);
/* Advances the game by exactly one grid step, as if move_interval_ms
   had elapsed -- the real movement/collision/growth logic, without
   needing to wait out real animation timing in a test. */
void snake_test_step(void);
