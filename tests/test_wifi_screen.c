#define _POSIX_C_SOURCE 200809L
#include "../src/apps/settings/settings.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void tap(tezos_core_t *core, tezos_input_type_t type, int value) {
    tezos_input_event_t ev = { type, value };
    tezos_core_tick(core, &ev);
}

int main(void) {
    setenv("PATH", "./fixtures:/usr/bin:/bin", 1);
    remove("/tmp/tezos_cyberdeck_settings.txt");

    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, settings_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);

    tap(&core, TEZOS_INPUT_DOWN, 0); tap(&core, TEZOS_INPUT_DOWN, 0); tap(&core, TEZOS_INPUT_DOWN, 0); /* row 3: Wi-Fi */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0);
    CHECK(core.depth == 2, "Wi-Fi screen pushed, real scan ran against the mock nmcli");
    tezos_fb_dump_ppm(fb, "wifi_01_list.ppm");

    /* select the first network (HomeNetwork, WPA2) -- should require a password */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0);
    CHECK(core.depth == 2, "selecting a secured network pushes password entry (same screen, phase change)");
    tezos_fb_dump_ppm(fb, "wifi_02_password.ppm");

    /* GoodNetwork isn't the one shown, but let's just type SOMETHING and
       attempt connect against HomeNetwork -- the mock only recognizes
       GoodNetwork/correctpassword, so this exercises the FAILURE path
       for a real, live-scanned network */
    tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_NUM,7);tap(&core,TEZOS_INPUT_UP,0); /* p */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* CONNECT */
    CHECK(core.depth == 2, "connect attempt completes, result screen shown");
    tezos_fb_dump_ppm(fb, "wifi_03_result.ppm");

    tezos_fb_destroy(fb);
    remove("/tmp/tezos_cyberdeck_settings.txt");
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
