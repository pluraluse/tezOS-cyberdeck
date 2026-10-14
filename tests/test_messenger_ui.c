/* Real UI flow -- compose (receiver/subject/content via real T9),
   confirmation popup, result screen. Same pattern as test_scanner.c:
   verifies the flow reaches a real result via genuine navigation
   depth changes, not asserting on broadcast success specifically
   (network-dependent, same reasoning as every other real-broadcast
   test in this project). */
#include "../src/apps/messenger/messenger.h"
#include "../src/apps/wallet/wallet.h"
#include "../src/apps/settings/settings.h"
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
/* real T9 tap sequence for "hi" -- same technique already verified
   elsewhere in this project's T9 tests */
static void type_hi(tezos_core_t *core) {
    tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_UP, 0); /* h */
    tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_NUM, 4); tap(core, TEZOS_INPUT_UP, 0); /* i */
}

int main(void) {
    remove("/tmp/tezos_cyberdeck_settings.txt");
    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);

    /* establish an active signer first -- Messenger needs one to sign with */
    const char *mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    wallet_set_active_signer_from_mnemonic(mnemonic, 12, "", 0);

    tezos_core_push(&core, messenger_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 1, "messenger pushed");
    CHECK(strlen(settings_get_messenger_contract()) > 0,
          "the confirmed default contract address is present -- compose should be reachable, not blocked");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* select "Compose" (first home row) */
    CHECK(core.depth == 1, "compose is an in-screen phase change (receiver entry), not a new pushed screen");

    /* receiver address -- typed via real T9 is exhaustively verified
       elsewhere (import_recovery.c's tests); here just confirm the
       flow accepts SOFTKEY_L to advance between fields */
    type_hi(&core); /* not a real address, but this test is about UI flow + confirm gating,
        not re-proving T9 correctness or address validation, which the structural test
        (test_messenger_build_value.c) and tezos_forge_address's own tests already cover */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* NEXT -> subject */
    type_hi(&core);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* NEXT -> content */
    type_hi(&core);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* NEXT on content -> pushes confirmation popup */
    CHECK(core.depth == 2, "completing the compose flow pushes a real confirmation popup, doesn't send immediately");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* confirm -- real build-value+forge+sign+broadcast-attempt */
    CHECK(core.depth == 1, "confirming pops back to Messenger with a real result (whatever the broadcast outcome)");

    tezos_fb_destroy(fb);
    remove("/tmp/tezos_cyberdeck_settings.txt");
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
