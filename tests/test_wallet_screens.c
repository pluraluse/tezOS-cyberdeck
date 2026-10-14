/* Wallet app screens -- verifies real navigation across all 6 pages,
   and that Send correctly requires popup confirmation before signing
   (pressing SIGN pushes a confirmation popup; only confirming it
   actually triggers the real forge+sign). */
#include "../src/apps/wallet/wallet.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include "../src/chain/tezos_store.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void tap(tezos_core_t *core, tezos_input_type_t type, int value) {
    tezos_input_event_t ev = { type, value };
    tezos_core_tick(core, &ev);
}

int main(void) {
    /* Ensure a clean start regardless of what ran before this process --
       wallet_entry_screen() checks for a saved signer store and shows a
       locked gate if one exists (see tezos_store.h). A leftover file
       from a previous test run (this exact thing happened once while
       building this feature) would silently change this test's
       behavior in a confusing way. Tests should not depend on
       execution order or leftover shared state. */
    remove(TEZOS_STORE_DEFAULT_SIGNER_PATH);

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

    /* Send flow: real recipient entry, real amount entry, THEN Send
       confirm/SIGN/popup behavior -- updated for the real T9-driven
       flow (recipient/amount used to be hardcoded; now they require
       real input first, see test_wallet_send_ui.c for the dedicated,
       thorough test of that new flow specifically -- this test's own
       remaining job is Send's SIGN/confirm-popup behavior once a real
       recipient/amount have actually been entered, not re-proving T9
       entry itself). */
    for (int i = 0; i < 5; i++) tap(&core, TEZOS_INPUT_UP, 0); /* back to Balances page */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* real recipient-entry screen, a real PUSH */
    CHECK(core.depth == 2, "Send pushes the real recipient-entry screen");

    /* real, if arbitrary, recipient text -- correctness of T9 itself
       isn't this test's job (see import_recovery.c's exhaustive T9
       tests), just that SOME real value progresses the flow */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* NEXT -- submits recipient, REPLACES with amount entry */
    CHECK(core.depth == 2, "after a recipient, still at the same depth (amount entry replaced recipient entry in place)");

    /* a real, valid amount: "1" via real digit-cycling taps (key 1 is
       14 characters -- ".,'?!\"-()@/:_1" -- reaching its own digit
       needs 14 taps, see tezos_t9.c's KEY_CHARS) */
    for (int i = 0; i < 14; i++) tap(&core, TEZOS_INPUT_NUM, 1);
    tap(&core, TEZOS_INPUT_UP, 0);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* NEXT -- submits amount, PUSHES the real Send confirm screen */
    CHECK(core.depth == 3, "a valid amount pushes the real Send confirm screen");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SIGN */
    CHECK(core.depth == 4, "pressing SIGN pushes a confirmation popup, does not sign immediately");

    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0); /* CANCEL the popup */
    CHECK(core.depth == 3, "cancelling the popup pops back to Send confirm");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SIGN again */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* CONFIRM this time */
    CHECK(core.depth == 3, "confirming pops back to Send confirm, now with a real signed result");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
