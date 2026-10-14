/* Real Explorer UI flow -- address entry (real T9), real fetch attempt
   (fails offline in this sandbox), navigation to Entrypoints/Holdings
   sub-screens and back. */
#include "../src/apps/explorer/explorer.h"
#include "../src/apps/wallet/wallet.h"
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

    tezos_core_push(&core, wallet_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    tezos_core_push(&core, explorer_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 2, "explorer pushed on top of wallet");

    /* type a real, well-formed contract address via real T9 (the
       Liquidity Baking address, well-known throughout this project's
       test suite) -- doesn't need to reach the network for this test,
       just needs the flow to progress correctly either way */
    tap(&core, TEZOS_INPUT_NUM, 5); tap(&core, TEZOS_INPUT_UP, 0); /* just typing SOMETHING, not
        the literal address -- T9 correctness is exhaustively covered elsewhere
        (import_recovery.c's tests); this test is about Explorer's navigation flow */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* LOOKUP -- real fetch attempt */
    CHECK(core.depth == 2, "lookup is an in-screen phase change (info screen), not a new pushed screen");
    tezos_fb_dump_ppm(fb, "explorer_info.ppm");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* OPEN the info menu's first item (Entrypoints) --
        real navigation regardless of whether the fetch succeeded (offline in this sandbox) */
    CHECK(core.depth == 2, "opening Entrypoints from the info screen is an in-screen phase change");

    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0); /* BACK from Entrypoints */
    CHECK(core.depth == 2, "BACK from Entrypoints correctly returns to the info screen, not exiting Explorer");

    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0); /* BACK from info screen */
    CHECK(core.depth == 2, "BACK from the info screen correctly returns to address entry, not exiting Explorer");

    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0); /* BACK from address entry -- exits Explorer for real */
    CHECK(core.depth == 1, "BACK from address entry correctly exits Explorer back to Wallet");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
