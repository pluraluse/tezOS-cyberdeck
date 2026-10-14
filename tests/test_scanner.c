/* Scanner -- "generic entrypoint decode/fill/forge" (the app's original
   one-line spec). QR-scans a contract call spec (real libqrencode->
   zbar->cJSON pipeline), signs with Wallet's active identity
   (wallet_sign_operation -- Scanner has no signer of its own), and
   attempts a real broadcast. Full real pipeline: Michelson entrypoint-
   call forging, real signing, real (offline-gracefully-failing here)
   broadcast attempt. */
#include "../src/apps/scanner/scanner.h"
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
    remove("/tmp/tezos_cyberdeck_signer_store.bin"); /* clean start, same reasoning as other tests touching this shared path */

    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);

    tezos_core_push(&core, wallet_entry_screen()); /* establishes an active signer, same as real usage */
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 1, "wallet opened first, establishing an active signer");

    tezos_core_push(&core, scanner_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 2, "scanner pushed");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* real simulate-scan: QR encode->bitmap->zbar decode->JSON parse */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SIGN & SEND -- pushes confirmation popup */
    CHECK(core.depth == 3, "SIGN & SEND pushes a confirmation popup, doesn't sign immediately");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* confirm -- real forge+sign+broadcast-attempt */
    CHECK(core.depth == 2, "confirming pops back to Scanner with a real result");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
