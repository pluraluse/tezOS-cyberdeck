/* Settings: Import Recovery Phrase -- types a real 12-word BIP39
   phrase through the ACTUAL T9 + live-wordlist-candidate-filtering UI
   (tap sequence generated programmatically, not hand-transcribed --
   this caught a real transcription bug once already), sets a real PIN
   through the new PIN-gated save flow, and verifies -- with a REAL
   assertion, not just "didn't crash" -- that the resulting active
   signer's address matches an independently-computed expected value.
   (An earlier version of this test had zero real CHECK() assertions at
   all and would report "ALL CHECKS PASSED" regardless of outcome --
   caught when the PIN requirement was added and this test kept
   reporting success despite no longer even reaching the success
   screen. Fixed properly here, not just patched around.) */
#include "../src/apps/settings/import_recovery.h"
#include "../src/apps/wallet/wallet.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include "../src/chain/tezos_signer.h"
#include "../src/chain/tezos_store.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void tap(tezos_core_t *core, tezos_input_type_t type, int value) {
    tezos_input_event_t ev = { type, value };
    tezos_core_tick(core, &ev);
}

int main(void) {
    /* clean start regardless of what ran before this process (see
       test_wallet_screens.c for the exact same real issue this caught) */
    remove(TEZOS_STORE_DEFAULT_SIGNER_PATH);

    const char *mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    tezos_mock_signer_t expected_signer;
    tezos_mock_signer_from_mnemonic(&expected_signer, mnemonic, 12, "", 0);
    char expected_address[128];
    tezos_mock_signer_address_b58(&expected_signer, expected_address, sizeof expected_address);
    printf("  independently computed expected address: %s\n", expected_address);

    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, import_recovery_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);

    /* type all 12 real words through the actual T9 + candidate-select UI flow */
    /* word 1: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 2: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 3: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 4: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 5: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 6: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 7: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 8: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 9: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 10: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 11: "abandon" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_NUM, 3); tap(&core, TEZOS_INPUT_UP, 0); /* d */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* n */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT */
    /* word 12: "about" */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* a */
    tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_NUM, 2); tap(&core, TEZOS_INPUT_UP, 0); /* b */
    tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_NUM, 6); tap(&core, TEZOS_INPUT_UP, 0); /* o */
    tap(&core, TEZOS_INPUT_NUM, 8); tap(&core, TEZOS_INPUT_NUM, 8); tap(&core, TEZOS_INPUT_UP, 0); /* u */
    tap(&core, TEZOS_INPUT_NUM, 8); tap(&core, TEZOS_INPUT_UP, 0); /* t */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT -- checksum now validates, PIN entry should push */
    CHECK(core.depth == 2, "checksum validated after the 12th word -- PIN entry screen pushed");

    /* enter a real PIN and confirm, completing the new PIN-gated save flow */
    tap(&core, TEZOS_INPUT_NUM, 1);
    tap(&core, TEZOS_INPUT_NUM, 1);
    tap(&core, TEZOS_INPUT_NUM, 1);
    tap(&core, TEZOS_INPUT_NUM, 1);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* confirm PIN "1111" */
    CHECK(core.depth == 1, "confirming the PIN pops back to the import screen (now showing IMPORTED)");

    /* the actual real assertions -- not just "didn't crash" */
    CHECK(tezos_store_exists(TEZOS_STORE_DEFAULT_SIGNER_PATH),
          "the encrypted store file now exists on disk after import+PIN confirm");

    char actual_address[128];
    CHECK(wallet_get_active_address(actual_address, sizeof actual_address),
          "wallet_get_active_address succeeds -- a real signer was set active by the import");
    printf("  expected: %s\n  actual:   %s\n", expected_address, actual_address);
    CHECK(strcmp(actual_address, expected_address) == 0,
          "the active signer's address matches the independently-computed expected address EXACTLY -- "
          "the full 12-word T9 entry + PIN-gated save flow genuinely succeeded, not just \"didn't crash\"");

    tezos_fb_dump_ppm(fb, "/tmp/import_test_result.ppm");
    tezos_fb_destroy(fb);

    remove(TEZOS_STORE_DEFAULT_SIGNER_PATH); /* clean up after ourselves too */

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
