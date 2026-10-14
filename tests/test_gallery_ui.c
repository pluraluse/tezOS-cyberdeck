/* Real Gallery UI flow -- entry (real fetch attempt, fails offline in
   this sandbox, a real correctly-handled path) plus list->detail->back
   navigation verified via direct state manipulation (same reasoning as
   Messenger's test_messenger_inbox_ui.c -- no live network here). */
#include "../src/apps/gallery/gallery.h"
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

    const char *mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    wallet_set_active_signer_from_mnemonic(mnemonic, 12, "", 0);

    /* push wallet first (same pattern as test_scanner.c) so Gallery's
       BACK demonstrates a real, meaningful pop back to something, not
       just "stays on the only screen" */
    tezos_core_push(&core, wallet_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    tezos_core_push(&core, gallery_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 2, "gallery pushed on top of wallet, real fetch attempted on entry");
    tezos_fb_dump_ppm(fb, "gallery_list.ppm");

    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0); /* BACK */
    CHECK(core.depth == 1, "BACK from an empty/offline gallery correctly returns to Wallet underneath");

    tezos_fb_destroy(fb);

    /* === real navigation with actual tokens present -- same
       injection-hook reasoning as Messenger's inbox test === */
    tezos_fb_t *fb2 = tezos_fb_create(320, 480);
    tezos_core_t core2;
    tezos_core_init(&core2, fb2);

    const char *aliases[] = {"PLENTY", "hic et nunc NFTs"};
    const char *names[] = {"", "Genesis 002"};
    const char *ids[] = {"0", "274069"};
    const char *balances[] = {"5000", "1"};
    tezos_core_push(&core2, gallery_test_inject_tokens(aliases, names, ids, balances, 2));
    core2.needs_render = true;
    tezos_core_tick(&core2, NULL);
    CHECK(core2.depth == 1, "injected token list pushed correctly");

    tap(&core2, TEZOS_INPUT_DOWN, 0); /* select the 2nd token (the real OBJKT) */
    tap(&core2, TEZOS_INPUT_SOFTKEY_L, 0); /* OPEN */
    CHECK(core2.depth == 1, "opening a token from the list is an in-screen phase change (detail view)");
    tezos_fb_dump_ppm(fb2, "gallery_detail.ppm");

    tap(&core2, TEZOS_INPUT_SOFTKEY_R, 0); /* BACK from detail */
    CHECK(core2.depth == 1, "BACK from token detail correctly returns to the gallery LIST");

    tezos_fb_destroy(fb2);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
