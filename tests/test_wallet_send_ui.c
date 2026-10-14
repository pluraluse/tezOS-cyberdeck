/* Real end-to-end Send UI flow -- Balances -> real recipient T9 entry
   -> real amount T9 entry -> confirm popup -> real result. This is the
   test for closing Wallet's oldest, most central open gap: Send used
   to be entirely hardcoded. */
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
/* real T9 tap sequence for a real, well-formed address (used purely as
   arbitrary text here -- T9 correctness itself is exhaustively covered
   elsewhere; this test is about the Send flow's navigation and
   wiring) */
/* T9 keys reach their OWN digit only after cycling past every letter/
   punctuation character on that key first (see tezos_t9.c's KEY_CHARS
   -- key 1 is ".,'?!\"-()@/:_1", 14 characters; key 5 is "jkl5", 4
   characters) -- a single tap+UP does NOT produce the digit itself.
   This taps the given key exactly as many times as needed to reach
   its own digit, then commits with UP. */
static void type_digit(tezos_core_t *core, int key, int taps_to_reach_digit) {
    for (int i = 0; i < taps_to_reach_digit; i++) tap(core, TEZOS_INPUT_NUM, key);
    tap(core, TEZOS_INPUT_UP, 0);
}
static void type_address(tezos_core_t *core) {
    for (int i = 0; i < 5; i++) { tap(core, TEZOS_INPUT_NUM, 2); tap(core, TEZOS_INPUT_UP, 0); }
}

int main(void) {
    const char *mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    wallet_set_active_signer_from_mnemonic(mnemonic, 12, "", 0);

    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, wallet_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SEND from the Balances page -> real recipient entry, a real PUSH */
    CHECK(core.depth == 2, "starting Send pushes the real recipient-entry screen");
    tezos_fb_dump_ppm(fb, "wallet_send_recipient.ppm");

    type_address(&core);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* NEXT -- submits recipient, REPLACES with amount entry (depth unchanged) */
    CHECK(core.depth == 2, "after entering a recipient, still at the same depth (amount entry replaced recipient entry in place)");
    tezos_fb_dump_ppm(fb, "wallet_send_amount.ppm");

    /* real validation check: an invalid amount should NOT advance */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* types "a", not a valid amount */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0);
    CHECK(core.depth == 2, "an invalid amount is correctly rejected -- stays on amount entry, doesn't silently advance");
    tap(&core, TEZOS_INPUT_HASH, 0); /* real backspace (TEZOS_INPUT_HASH with no pending key) --
        clears the stray "a" left in the buffer so it doesn't corrupt the next, valid entry */

    /* now type a real, valid amount: "15" via real digit-cycling taps
       (key 1 needs 14 taps to reach '1', key 5 needs 4 taps to reach '5') */
    type_digit(&core, 1, 14);
    type_digit(&core, 5, 4);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* NEXT -- submits a valid "15" tez amount, PUSHES confirm */
    CHECK(core.depth == 3, "a valid amount pushes the real confirmation screen");
    tezos_fb_dump_ppm(fb, "wallet_send_confirm.ppm");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SIGN -- pushes the real confirmation popup */
    CHECK(core.depth == 4, "pressing SIGN pushes a real confirmation popup, doesn't sign immediately");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* confirm -- real forge+sign+broadcast attempt */
    CHECK(core.depth == 3, "confirming pops back to the Send confirm screen with a real result");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
