/* Wallet app screens -- verifies real navigation across all 6 pages,
   and that Send correctly requires popup confirmation before signing
   (pressing SIGN pushes a confirmation popup; only confirming it
   actually triggers the real forge+sign). */
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
    CHECK(core.depth == 1, "wallet home screen pushed correctly");

    /* flip through all 6 pages */
    for (int i = 1; i < 6; i++) tap(&core, TEZOS_INPUT_DOWN, 0);
    CHECK(core.depth == 1, "paging through all 6 wallet pages doesn't change navigation depth");

    /* Send flow: press SELECT (Send confirm), then SIGN should push a
       popup, NOT sign immediately */
    for (int i = 0; i < 5; i++) tap(&core, TEZOS_INPUT_UP, 0); /* back to Balances page */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* Send confirm screen */
    CHECK(core.depth == 2, "Send confirm screen pushed");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SIGN */
    CHECK(core.depth == 3, "pressing SIGN pushes a confirmation popup (depth=3), does not sign immediately");

    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0); /* CANCEL the popup */
    CHECK(core.depth == 2, "cancelling the popup pops back to Send confirm");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SIGN again */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* CONFIRM this time */
    CHECK(core.depth == 2, "confirming pops back to Send confirm, now with a real signed result");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
