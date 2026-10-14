#include "../src/core/tezos_boot_splash.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);

    tezos_screen_t *splash = tezos_boot_splash_screen_get();
    tezos_core_push(&core, splash);
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(!tezos_boot_splash_is_finished(splash), "splash correctly reports not finished at t=0");
    tezos_fb_dump_ppm(fb, "splash_t0.ppm");

    /* advance partway through the sweep */
    tezos_core_on_tick(&core, 400);
    tezos_fb_dump_ppm(fb, "splash_t400.ppm");
    CHECK(!tezos_boot_splash_is_finished(splash), "not finished partway through the sweep");

    tezos_core_on_tick(&core, 400);
    tezos_fb_dump_ppm(fb, "splash_t800.ppm");

    tezos_core_on_tick(&core, 400);
    tezos_fb_dump_ppm(fb, "splash_t1200.ppm");

    /* past the sweep, into the hold */
    tezos_core_on_tick(&core, 400);
    tezos_fb_dump_ppm(fb, "splash_t1600.ppm");
    CHECK(!tezos_boot_splash_is_finished(splash), "still not finished during the hold phase");

    /* past total duration -- should now report finished */
    tezos_core_on_tick(&core, 500);
    CHECK(tezos_boot_splash_is_finished(splash), "correctly reports finished once total duration elapses");

    /* a stale/wrong screen pointer should safely report not-finished, not crash or misreport */
    tezos_screen_t fake_screen;
    CHECK(!tezos_boot_splash_is_finished(&fake_screen), "a non-splash screen pointer safely reports not finished");
    CHECK(!tezos_boot_splash_is_finished(NULL), "a NULL pointer safely reports not finished, doesn't crash");

    /* === real input skip path -- separate fresh instance === */
    tezos_fb_t *fb2 = tezos_fb_create(320, 480);
    tezos_core_t core2;
    tezos_core_init(&core2, fb2);
    tezos_screen_t *splash2 = tezos_boot_splash_screen_get();
    tezos_core_push(&core2, splash2);
    core2.needs_render = true;
    tezos_core_tick(&core2, NULL);
    CHECK(core2.depth == 1, "splash pushed as the only/first screen");
    CHECK(!tezos_boot_splash_is_finished(splash2), "not finished before any key press");

    tezos_input_event_t any_key = { TEZOS_INPUT_SOFTKEY_L, 0 };
    tezos_core_tick(&core2, &any_key);
    /* the splash's own handle_input deliberately returns ACTION_NONE
       (not POP -- see tezos_boot_splash.h for why), so depth doesn't
       change here; the real point is that is_finished() now reports
       true, which is the actual mechanism the calling shim uses to
       perform the real transition */
    CHECK(core2.depth == 1, "the splash screen's own handle_input doesn't change the stack directly");
    CHECK(tezos_boot_splash_is_finished(splash2), "a real key press correctly marks the splash as finished, the real skip mechanism");

    tezos_fb_destroy(fb);
    tezos_fb_destroy(fb2);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
