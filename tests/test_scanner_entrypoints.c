#include "../src/apps/scanner/scanner.h"
#include "../src/apps/wallet/wallet.h"
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

    tezos_core_push(&core, wallet_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    tezos_core_push(&core, scanner_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 2, "scanner pushed");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* simulate scan -> SCAN_DECODED */
    CHECK(core.depth == 2, "still at depth 2 after decoding (in-screen phase change)");
    tezos_fb_dump_ppm(fb, "scan_ep_01_decoded.ppm");

    /* THE KEY TEST: press UP to browse real entrypoints */
    tap(&core, TEZOS_INPUT_UP, 0);
    CHECK(core.depth == 2, "browsing entrypoints is an in-screen phase change, not a new pushed screen");
    tezos_fb_dump_ppm(fb, "scan_ep_02_entrypoints.ppm");

    /* THE CRITICAL NAVIGATION TEST: BACK from entrypoints must return
       to the decoded call, NOT exit Scanner entirely */
    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0);
    CHECK(core.depth == 2, "BACK from entrypoints correctly stays within Scanner (depth unchanged)");
    tezos_fb_dump_ppm(fb, "scan_ep_03_back_to_decoded.ppm");

    /* confirm the decoded screen's own BACK still works correctly afterward */
    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0);
    CHECK(core.depth == 1, "a SECOND back press from the decoded screen correctly exits Scanner");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
