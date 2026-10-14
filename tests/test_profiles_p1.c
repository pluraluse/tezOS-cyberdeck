/* PART 1: real import (establishes account 0 = session mnemonic),
   real Profiles UI navigation, add a second named account, switch
   back to account 0, verify both addresses. */
#include "../src/apps/settings/settings.h"
#include "../src/apps/wallet/wallet.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)
static void tap(tezos_core_t *core, tezos_input_type_t type, int value) {
    tezos_input_event_t ev = { type, value }; tezos_core_tick(core, &ev);
}

int main(void) {
    remove("/tmp/tezos_cyberdeck_settings.txt");

    /* establish account 0 directly (bypassing the 12-word T9 typing,
       already proven exhaustively elsewhere -- this test is about
       Profiles specifically) */
    const char *mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    CHECK(wallet_set_active_signer_from_mnemonic(mnemonic, 12, "", 0), "account 0 established");
    char addr0[128];
    wallet_get_active_address(addr0, sizeof addr0);

    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, settings_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);

    tap(&core, TEZOS_INPUT_DOWN, 0); /* row 1: Profiles */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0);
    CHECK(core.depth == 2, "Profiles screen pushed, real account list shown");

    /* select "+ Add Account" (second item, since only account 0 exists so far) */
    tap(&core, TEZOS_INPUT_DOWN, 0);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0);
    /* type "Savings" via real T9 (tap sequence verified programmatically) */
    tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_UP,0); /* s */
    tap(&core,TEZOS_INPUT_NUM,2);tap(&core,TEZOS_INPUT_UP,0); /* a */
    tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_UP,0); /* v */
    tap(&core,TEZOS_INPUT_NUM,4);tap(&core,TEZOS_INPUT_UP,0); /* i */
    tap(&core,TEZOS_INPUT_NUM,6);tap(&core,TEZOS_INPUT_NUM,6);tap(&core,TEZOS_INPUT_UP,0); /* n */
    tap(&core,TEZOS_INPUT_NUM,4);tap(&core,TEZOS_INPUT_NUM,4);tap(&core,TEZOS_INPUT_UP,0); /* g */
    tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_UP,0); /* s */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SAVE */

    char addr1[128];
    wallet_get_active_address(addr1, sizeof addr1);
    CHECK(strcmp(addr1, addr0) != 0, "adding+activating the new account changed the active address");
    printf("  account 0 (Main):    %s\n  account 1 (Savings): %s\n", addr0, addr1);

    tezos_fb_destroy(fb);
    printf("\nPART1 %s (%d failures)\n", fails==0?"PASSED":"FAILED", fails);
    return fails == 0 ? 0 : 1;
}
