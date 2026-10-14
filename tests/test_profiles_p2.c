/* PART 2: a completely SEPARATE process from part 1 -- only the
   settings file on disk persists. Simulates what happens after a real
   restart + unlock: labels and active account should be recovered from
   disk, not reset to defaults. */
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
    /* fresh process -- simulates the post-restart unlock establishing
       the same session mnemonic again (this is what a real PIN-unlock
       would do; not re-testing PIN unlock itself here, that's covered
       by test_lock_change_pin.c) */
    const char *mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    wallet_set_active_signer_from_mnemonic(mnemonic, 12, "", 0);

    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, settings_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);

    tap(&core, TEZOS_INPUT_DOWN, 0);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0);
    CHECK(core.depth == 2, "Profiles screen pushed in a fresh process");

    /* The real check: after profiles_screen_get() runs in this fresh
       process, wallet's active account should have been restored to
       whatever was active when part 1 exited (account 1, "Savings") --
       verified via the actual address, not just trusting the label. */
    char addr[128];
    wallet_get_active_address(addr, sizeof addr);
    CHECK(strcmp(addr, "tz1gvekQVEwFFdxT2KrcY6kZgFK1qMN3mmWF") == 0,
          "a SEPARATE PROCESS correctly restored the previously-active account (Savings, index 1) from disk -- "
          "real persistence, not reset to account 0's default");

    tezos_fb_dump_ppm(fb, "/tmp/profiles_p2_result.ppm");
    tezos_fb_destroy(fb);
    remove("/tmp/tezos_cyberdeck_settings.txt");
    printf("\nPART2 %s (%d failures)\n", fails==0?"PASSED":"FAILED", fails);
    return fails == 0 ? 0 : 1;
}
