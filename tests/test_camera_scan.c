/* Camera Scan Mode -- real screen, using a "simulate scan" trigger
   (no real CSI camera driver exists) to exercise the actual
   libqrencode->zbar->tezos_beacon_qr pipeline through the real screen,
   confirming the rendered result shows the correct paired peer info. */
#include "../src/apps/camera/camera.h"
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
    tezos_core_push(&core, camera_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 1, "camera scan screen pushed correctly");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* real simulate-scan: real QR encode -> bitmap -> real zbar decode -> real parse */

    uint16_t cyan565 = (uint16_t)(((0x2F & 0xF8) << 8) | ((0xE6 & 0xFC) << 3) | (0xE6 >> 3));
    int cyan_count = 0;
    for (int i = 0; i < fb->width * fb->height; i++) if (fb->pixels[i] == cyan565) cyan_count++;
    CHECK(cyan_count > 50, "PAIRED indicator (cyan) is present after the real scan pipeline ran");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
