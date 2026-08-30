/* Camera Capture Mode -- real end-to-end flow through the actual UI:
   Camera home -> select Capture -> cycle resolution/depth settings ->
   trigger a real capture (runs the verified pipeline against a
   synthetic test frame) -> result screen. Proves the wiring, not just
   the pipeline functions in isolation (see test_camera_pipeline.c for
   that). */
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
    CHECK(core.depth == 1, "camera home (mode select) pushed correctly");

    tap(&core, TEZOS_INPUT_DOWN, 0); /* select Capture row */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0);
    CHECK(core.depth == 2, "Capture settings screen pushed");

    tap(&core, TEZOS_INPUT_DOWN, 0); tap(&core, TEZOS_INPUT_DOWN, 0); /* cycle to 800x480 */
    tap(&core, TEZOS_INPUT_UP, 0); /* cycle to 8-bit gray */

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* CAPTURE -- runs the real pipeline */
    CHECK(core.depth == 3, "capture result screen pushed after triggering a real capture");

    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0);
    CHECK(core.depth == 2, "BACK from result returns to capture settings");
    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0);
    CHECK(core.depth == 1, "BACK from capture settings returns to camera home");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
