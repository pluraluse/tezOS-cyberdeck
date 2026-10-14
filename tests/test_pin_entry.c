/* PIN entry widget -- confirm/cancel/backspace/empty-rejection, and
   correct interaction with tezos_core_pop()'s "never empty the stack"
   guarantee (a real screen must always have a valid render function --
   this test caught that exact mistake in its own first draft). */
#include "../src/core/tezos_pin_entry.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void tap(tezos_core_t *core, tezos_input_type_t type, int value) {
    tezos_input_event_t ev = { type, value };
    tezos_core_tick(core, &ev);
}

static void host_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    (void)self;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
}

int main(void) {
    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);

    char pin[TEZOS_PIN_MAX_LEN + 1];
    tezos_pin_result_t result;
    tezos_screen_t host_screen;
    memset(&host_screen, 0, sizeof host_screen);
    host_screen.render = host_render;
    tezos_core_push(&core, &host_screen);
    core.needs_render = false;

    tezos_core_push(&core, tezos_pin_entry_get("SET PIN", pin, &result));
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 2, "PIN entry pushed on top of a host screen");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0);
    CHECK(core.depth == 2, "confirming an EMPTY PIN is correctly rejected (no pop)");

    tap(&core, TEZOS_INPUT_NUM, 1);
    tap(&core, TEZOS_INPUT_NUM, 2);
    tap(&core, TEZOS_INPUT_NUM, 3);
    tap(&core, TEZOS_INPUT_HASH, 0);
    tap(&core, TEZOS_INPUT_NUM, 4);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0);
    CHECK(core.depth == 1, "confirming pops back to the host screen");
    CHECK(result == TEZOS_PIN_CONFIRMED, "result correctly recorded as CONFIRMED");
    CHECK(strcmp(pin, "124") == 0, "backspace + digit entry produces the exact expected PIN");

    tezos_core_push(&core, tezos_pin_entry_get("SET PIN", pin, &result));
    tap(&core, TEZOS_INPUT_NUM, 9);
    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0);
    CHECK(result == TEZOS_PIN_CANCELLED, "cancelling correctly records CANCELLED");
    CHECK(core.depth == 1, "cancelling also pops back to the host screen");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
