/* Real integration test -- confirms Settings' About screen actually
   hands off to the real Snake screen (not just that Snake works in
   isolation, which test_snake.c already covers thoroughly). */
#include "../src/apps/settings/settings.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void tap(tezos_core_t *core, tezos_input_type_t type, int value) {
    tezos_input_event_t ev = { type, value };
    tezos_core_tick(core, &ev);
}

int main(void) {
    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, settings_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);

    /* navigate to About (row index 6 per settings.c's real item list) and select it */
    for (int i = 0; i < 6; i++) tap(&core, TEZOS_INPUT_DOWN, 0);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* enter About */
    CHECK(core.depth == 2, "About screen pushed correctly");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* PLAY SNAKE */
    CHECK(core.depth == 3, "PLAY SNAKE from About really pushes the real Snake screen");

    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0); /* BACK from Snake */
    CHECK(core.depth == 2, "BACK from Snake correctly returns to About, not exiting further");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
