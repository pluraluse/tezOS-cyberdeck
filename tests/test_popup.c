/* Popup/confirmation dialog -- verifies the popup genuinely overlays
   (background still visible around it, not a full-screen replacement),
   and that confirm/cancel correctly flow results back via on_resume. */
#include "../src/core/tezos_popup.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include "../src/core/tezos_fonts.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

typedef struct { tezos_popup_result_t last_result; int resume_count; } host_state_t;
static host_state_t g_host_state;
static void host_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    (void)self;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_AMBER);
}
static void host_on_resume(tezos_screen_t *self) { ((host_state_t *)self->app_state)->resume_count++; }
static tezos_popup_result_t g_popup_result;
static tezos_action_t host_handle_input(tezos_screen_t *self, tezos_input_event_t ev, tezos_screen_t **next, bool *needs_redraw) {
    (void)self; (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        *next = tezos_popup_confirm_get("Confirm", "Test message for the popup overlay check.", "OK", "CANCEL", &g_popup_result);
        return TEZOS_ACTION_PUSH;
    }
    return TEZOS_ACTION_NONE;
}
static tezos_screen_t g_host_screen = {
    .render = host_render, .handle_input = host_handle_input,
    .on_resume = host_on_resume, .on_tick = NULL,
    .app_state = &g_host_state, .debug_name = "host",
};

int main(void) {
    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, &g_host_screen);
    core.needs_render = true;
    tezos_core_tick(&core, NULL);

    tezos_input_event_t select = {TEZOS_INPUT_SOFTKEY_L, 0};
    tezos_core_tick(&core, &select);
    CHECK(core.depth == 2, "popup pushed correctly");

    bool corners_still_amber = true;
    uint16_t amber565 = (uint16_t)(((0xFF & 0xF8) << 8) | ((0xA5 & 0xFC) << 3) | (0x3D >> 3));
    int corners[4][2] = {{2,2},{317,2},{2,477},{317,477}};
    for (int i = 0; i < 4; i++) {
        uint16_t px = fb->pixels[corners[i][1] * fb->width + corners[i][0]];
        if (px != amber565) corners_still_amber = false;
    }
    CHECK(corners_still_amber, "host screen's background is still visible around the popup box -- true overlay");

    uint16_t center_px = fb->pixels[240 * fb->width + 160];
    CHECK(center_px != amber565, "the popup box itself correctly overrides the center");

    tezos_core_tick(&core, &select); /* confirm */
    CHECK(core.depth == 1, "confirming pops back to the host");
    CHECK(g_popup_result == TEZOS_POPUP_CONFIRMED, "result correctly recorded as CONFIRMED");
    CHECK(g_host_state.resume_count == 1, "on_resume fired after the popup closed");

    tezos_core_tick(&core, &select);
    tezos_input_event_t back = {TEZOS_INPUT_SOFTKEY_R, 0};
    tezos_core_tick(&core, &back);
    CHECK(core.depth == 1, "cancelling also pops back");
    CHECK(g_popup_result == TEZOS_POPUP_CANCELLED, "result correctly recorded as CANCELLED");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
