#include "tezos_core.h"
#include "tezos_gfx.h"
#include "tezos_fonts.h"
#include "tezos_widgets.h"
#include <stdio.h>

/* --- Idle screen: static singleton, per the core's memory model.
   Now uses the shared list widget instead of hand-rolled row drawing,
   and actually pushes into an app on SELECT instead of doing nothing. */

typedef struct {
    tezos_list_state_t menu;
} idle_state_t;

static const tezos_list_item_t g_idle_menu_items[] = {
    {"Wallet", NULL},
    {"Explorer", NULL},
    {"Scanner", NULL},
    {"Gallery", NULL},
};

static idle_state_t g_idle_state;

static void idle_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    idle_state_t *st = (idle_state_t *)self->app_state;

    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);

    tezos_draw_text(fb, d, 8, 6, "WiFi ...", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 220, 6, "82%", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    tezos_draw_text(fb, d, 60, 60, "14:32", &TEZOS_FONT_HERO, TEZOS_TEXT);
    tezos_draw_text(fb, d, 90, 105, "SUN 26 JUL", &TEZOS_FONT_SMALL, TEZOS_MUTED);

    int logo_w = tezos_text_width("tezOS", &TEZOS_FONT_HEADER);
    tezos_draw_text(fb, d, (fb->width - logo_w) / 2, 150, "tezOS", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);

    tezos_draw_rect(fb, d, 0, 195, fb->width, 1, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 205, "NEW ORIGINATION: KT1a9w...", &TEZOS_FONT_SMALL, TEZOS_PINK);
    tezos_draw_rect(fb, d, 0, 230, fb->width, 1, TEZOS_MUTED);

    tezos_list_render(fb, d, 8, 250, fb->width - 16, 130, &TEZOS_FONT_BODY, &st->menu);

    /* Icon verification row — not final placement/mapping, just proving
       tezos_draw_icon actually blits MEK-Dings glyphs correctly through
       the real pipeline. Codepoints here are arbitrary samples, not a
       real mapping — see docs/design-system/ICONS.md. */
    tezos_draw_icon(fb, d, 16, 400, &TEZOS_ICON_FONT, '4', TEZOS_CYAN);
    tezos_draw_icon(fb, d, 56, 400, &TEZOS_ICON_FONT, 'A', TEZOS_PINK);
    tezos_draw_icon(fb, d, 96, 400, &TEZOS_ICON_FONT, 'S', TEZOS_AMBER);
    tezos_draw_icon(fb, d, 136, 400, &TEZOS_ICON_FONT, 'i', TEZOS_TEXT);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int menu_w = tezos_text_width("MENU", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - menu_w - 8, fb->height - 26, "MENU", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t idle_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                         tezos_screen_t **next, bool *needs_redraw) {
    idle_state_t *st = (idle_state_t *)self->app_state;

    if (tezos_list_handle_input(&st->menu, ev)) {
        *needs_redraw = true;
        return TEZOS_ACTION_NONE;
    }

    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        /* SELECT: push a stub screen for whichever app is highlighted.
           Real apps will replace tezos_stub_screen_get() with their own
           entry-screen constructor once each app's actual screen exists —
           this is the wiring that proves PUSH/POP works end-to-end today. */
        const char *app_name = st->menu.items[st->menu.selected].label;
        *next = tezos_stub_screen_get(app_name);
        return TEZOS_ACTION_PUSH;
    }

    return TEZOS_ACTION_NONE;
}

static tezos_screen_t g_idle_screen = {
    .render = idle_render,
    .handle_input = idle_handle_input,
    .on_resume = NULL,
    .app_state = &g_idle_state,
    .debug_name = "idle",
};

int main(void) {
    tezos_list_init(&g_idle_state.menu, g_idle_menu_items, 4);

    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);

    tezos_core_push(&core, &g_idle_screen);
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    tezos_fb_dump_ppm(fb, "01_idle.ppm");
    printf("01_idle.ppm — initial idle screen, Wallet selected\n");

    /* move selection down to Scanner */
    tezos_input_event_t down = {TEZOS_INPUT_DOWN, 0};
    tezos_core_tick(&core, &down);
    tezos_core_tick(&core, &down);
    tezos_fb_dump_ppm(fb, "02_idle_scanner_selected.ppm");
    printf("02_idle_scanner_selected.ppm — Down x2, Scanner now selected\n");

    /* press SELECT — should genuinely push the Scanner stub screen */
    tezos_input_event_t select = {TEZOS_INPUT_SOFTKEY_L, 0};
    tezos_core_tick(&core, &select);
    tezos_fb_dump_ppm(fb, "03_scanner_stub_pushed.ppm");
    printf("03_scanner_stub_pushed.ppm — SELECT pressed, Scanner stub screen pushed (depth=%d)\n", core.depth);

    /* press BACK (right softkey on the stub screen) — should pop back to idle */
    tezos_input_event_t back = {TEZOS_INPUT_SOFTKEY_R, 0};
    tezos_core_tick(&core, &back);
    tezos_fb_dump_ppm(fb, "04_popped_back_to_idle.ppm");
    printf("04_popped_back_to_idle.ppm — BACK pressed, popped back to idle (depth=%d)\n", core.depth);

    tezos_fb_destroy(fb);
    return 0;
}
