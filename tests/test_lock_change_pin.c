#include "../src/apps/settings/settings.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include "../src/chain/tezos_store.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void tap(tezos_core_t *core, tezos_input_type_t type, int value) {
    tezos_input_event_t ev = { type, value };
    tezos_core_tick(core, &ev);
}
static void enter_pin(tezos_core_t *core, const char *digits) {
    for (const char *p = digits; *p; p++) tap(core, TEZOS_INPUT_NUM, *p - '0');
    tap(core, TEZOS_INPUT_SOFTKEY_L, 0); /* confirm */
}

int main(void) {
    remove(TEZOS_STORE_DEFAULT_SIGNER_PATH);

    /* pre-seed a saved phrase under an "old" PIN, simulating a
       previous import (not going through the full import UI here --
       that flow is tested elsewhere; this test is specifically about
       change-PIN, given an existing store) */
    tezos_store_signer_data_t data = {0};
    strcpy(data.mnemonic, "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about");
    data.word_count = 12;
    tezos_store_save_signer(TEZOS_STORE_DEFAULT_SIGNER_PATH, "1111", &data);

    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, settings_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);

    tap(&core, TEZOS_INPUT_DOWN, 0); tap(&core, TEZOS_INPUT_DOWN, 0); /* row 2: Lock/Passcode */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0);
    CHECK(core.depth == 2, "Lock/Passcode screen pushed");

    /* try the WRONG old PIN first */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* CHANGE PIN -> pushes PIN entry for current PIN */
    enter_pin(&core, "9999");
    CHECK(core.depth == 2, "back at Lock/Passcode after wrong-PIN attempt");

    /* now the CORRECT old PIN */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* RETRY -> pushes PIN entry again */
    enter_pin(&core, "1111");
    CHECK(core.depth == 2, "back at Lock/Passcode after correct old PIN");

    /* now the new PIN */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SET NEW PIN -> pushes PIN entry */
    enter_pin(&core, "2222");
    CHECK(core.depth == 2, "back at Lock/Passcode after setting new PIN");

    tezos_fb_destroy(fb);

    /* the actual real assertions: old PIN invalidated, new PIN works,
       same underlying data recovered */
    tezos_store_signer_data_t check;
    CHECK(!tezos_store_load_signer(TEZOS_STORE_DEFAULT_SIGNER_PATH, "1111", &check),
          "the OLD PIN no longer works after the change -- genuinely re-encrypted, not just relabeled");
    CHECK(tezos_store_load_signer(TEZOS_STORE_DEFAULT_SIGNER_PATH, "2222", &check),
          "the NEW PIN successfully decrypts");
    CHECK(strcmp(check.mnemonic, data.mnemonic) == 0,
          "the recovered mnemonic under the NEW PIN matches the original exactly -- same data, just re-keyed");

    remove(TEZOS_STORE_DEFAULT_SIGNER_PATH);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
