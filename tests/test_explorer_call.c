/* Real end-to-end entrypoint-calling flow -- walking a real
   pair(int,string) type via T9 (one field, then the other, in the
   correct order), confirming, and reaching a real result (whatever the
   actual broadcast outcome is in this offline sandbox). This is the
   test for the single largest piece of new engineering this session:
   the walker finally wired into a real, working UI. */
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
/* real T9 tap sequence for "42" -- numeric keys, no multi-tap needed */
static void type_42(tezos_core_t *core) {
    tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_UP, 0);
    tap(core, TEZOS_INPUT_NUM, 2); tap(core, TEZOS_INPUT_UP, 0);
}
static void type_hi(tezos_core_t *core) {
    tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_UP, 0); /* h */
    tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_UP, 0); /* i */
}

int main(void) {
    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);

    const char *mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    wallet_set_active_signer_from_mnemonic(mnemonic, 12, "", 0);

    tezos_core_push(&core, explorer_test_inject_entrypoint(
        "KT1TxqZ8QtKvLu3V3JH7Gx58n7Co8pgtpQU5", "test_entry",
        "{\"prim\":\"pair\",\"args\":[{\"prim\":\"int\"},{\"prim\":\"string\"}]}"));
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 1, "injected entrypoints list pushed correctly");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* select the injected entrypoint -> starts the call */
    CHECK(core.depth == 1, "starting a call is an in-screen phase change (first leaf prompt), not a new pushed screen");
    tezos_fb_dump_ppm(fb, "explorer_call_leaf1.ppm");

    type_42(&core);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* NEXT -- submits the int, advances to the string prompt */
    CHECK(core.depth == 1, "after the int, still in-screen (now prompting for the string)");
    tezos_fb_dump_ppm(fb, "explorer_call_leaf2.ppm");

    type_hi(&core);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* NEXT -- submits the string, walker is now DONE -> pushes confirm popup */
    CHECK(core.depth == 2, "completing both leaves pushes a real confirmation popup, doesn't call immediately");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* confirm -- real build-value + forge + sign + broadcast-attempt */
    CHECK(core.depth == 1, "confirming pops back to Explorer with a real result (whatever the broadcast outcome)");
    tezos_fb_dump_ppm(fb, "explorer_call_result.ppm");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
