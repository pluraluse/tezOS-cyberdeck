/* Device name persistence, PART 1 of 2 -- run test_device_name_persist_p2
   immediately after this to prove real cross-restart persistence (two
   separate process invocations sharing only the file on disk, same
   rigor as the signer store's persistence test in
   docs/build-notes/wallet-app-build-notes.md). */
#include "../src/apps/settings/settings.h"
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
    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, settings_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(strcmp(settings_get_device_name(), "tezOS-Device") == 0, "default name before any save");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* edit Device Name */
    for (int i = 0; i < 12; i++) tap(&core, TEZOS_INPUT_HASH, 0); /* clear "tezOS-Device" */
    /* type "cyber" (already-verified tap sequence from earlier this project) */
    tap(&core,TEZOS_INPUT_NUM,2);tap(&core,TEZOS_INPUT_NUM,2);tap(&core,TEZOS_INPUT_NUM,2);tap(&core,TEZOS_INPUT_UP,0);
    tap(&core,TEZOS_INPUT_NUM,9);tap(&core,TEZOS_INPUT_NUM,9);tap(&core,TEZOS_INPUT_NUM,9);tap(&core,TEZOS_INPUT_UP,0);
    tap(&core,TEZOS_INPUT_NUM,2);tap(&core,TEZOS_INPUT_NUM,2);tap(&core,TEZOS_INPUT_UP,0);
    tap(&core,TEZOS_INPUT_NUM,3);tap(&core,TEZOS_INPUT_NUM,3);tap(&core,TEZOS_INPUT_UP,0);
    tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_UP,0);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SAVE */
    CHECK(strcmp(settings_get_device_name(), "cyber") == 0, "device name updated in-memory after save");
    tezos_fb_destroy(fb);
    printf("\nPART1 %s (%d failures)\n", fails==0?"PASSED":"FAILED", fails);
    return fails == 0 ? 0 : 1;
}
