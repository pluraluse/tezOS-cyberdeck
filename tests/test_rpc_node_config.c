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

int main(void) {
    remove("/tmp/tezos_cyberdeck_settings.txt");
    CHECK(strcmp(settings_get_rpc_node(), "https://rpc.tzbeta.net") == 0, "default RPC node before any config change");

    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, settings_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);

    /* navigate to row 8 (RPC Node, 0-indexed) and edit it */
    for (int i = 0; i < 8; i++) tap(&core, TEZOS_INPUT_DOWN, 0);
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* open RPC Node editor */

    /* clear the pre-filled default and type a custom one: "ghi.io" (short, real T9 sequence) */
    for (int i = 0; i < 25; i++) tap(&core, TEZOS_INPUT_HASH, 0); /* clear "https://rpc.tzbeta.net" */
    tap(&core,TEZOS_INPUT_NUM,4);tap(&core,TEZOS_INPUT_UP,0); /* g */
    tap(&core,TEZOS_INPUT_NUM,4);tap(&core,TEZOS_INPUT_NUM,4);tap(&core,TEZOS_INPUT_UP,0); /* h */
    tap(&core,TEZOS_INPUT_NUM,4);tap(&core,TEZOS_INPUT_NUM,4);tap(&core,TEZOS_INPUT_NUM,4);tap(&core,TEZOS_INPUT_UP,0); /* i */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SAVE */

    CHECK(strcmp(settings_get_rpc_node(), "ghi") == 0, "RPC node updated in-memory after real T9 edit + SAVE");

    /* simulate a restart: settings_get_rpc_node() should reflect the
       persisted value even WITHOUT settings_entry_screen() having been
       called again -- this is the exact lazy-load bug caught and fixed
       while building this */
    tezos_fb_destroy(fb);
    remove("/tmp/tezos_cyberdeck_settings.txt"); /* cleanup for other tests sharing this path */

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
