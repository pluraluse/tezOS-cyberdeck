#include "settings.h"
#include "import_recovery.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../core/tezos_t9.h"
#include <string.h>
#include <stdio.h>

/* Device name persists only in memory for now -- no storage layer
   exists yet (see ROADMAP.md), so this resets on restart. Honest
   limitation, not hidden. */
#define DEVICE_NAME_DEFAULT "tezOS-Device"
static char g_device_name[TEZOS_T9_MAX_LEN + 1] = DEVICE_NAME_DEFAULT;

/* ============ Device Name edit screen (real T9 consumer) ============ */

typedef struct { tezos_t9_state_t t9; } device_name_state_t;
static device_name_state_t g_device_name_state;
static tezos_screen_t g_device_name_screen;

static void device_name_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    device_name_state_t *st = (device_name_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "DEVICE NAME", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    tezos_draw_text(fb, d, 8, 50, "Shown in Messenger profile", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 68, "and Beacon pairing.", &TEZOS_FONT_SMALL, TEZOS_MUTED);

    tezos_draw_rect(fb, d, 8, 110, fb->width - 16, 2, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 120, &TEZOS_FONT_BODY, &st->t9);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SAVE", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void device_name_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    device_name_state_t *st = (device_name_state_t *)self->app_state;
    *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
}

static tezos_action_t device_name_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                               tezos_screen_t **next, bool *needs_redraw) {
    device_name_state_t *st = (device_name_state_t *)self->app_state;
    (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) { /* SAVE */
        const char *typed = tezos_t9_get_text(&st->t9);
        if (typed[0] != '\0') { /* don't save an empty name over a real one */
            strncpy(g_device_name, typed, TEZOS_T9_MAX_LEN);
            g_device_name[TEZOS_T9_MAX_LEN] = '\0';
        }
        return TEZOS_ACTION_POP;
    }
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) { /* CANCEL -- discard edits */
        return TEZOS_ACTION_POP;
    }
    *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *device_name_screen_get(void) {
    tezos_t9_init(&g_device_name_state.t9);
    /* pre-fill with the current name so editing feels like editing, not
       starting from scratch -- t9_state_t's buffer is a plain field,
       fine to set directly from within this module */
    strncpy(g_device_name_state.t9.buffer, g_device_name, TEZOS_T9_MAX_LEN);
    g_device_name_state.t9.buffer[TEZOS_T9_MAX_LEN] = '\0';
    g_device_name_state.t9.length = (int)strlen(g_device_name_state.t9.buffer);

    g_device_name_screen = (tezos_screen_t){
        .render = device_name_render, .handle_input = device_name_handle_input,
        .on_resume = NULL, .on_tick = device_name_on_tick,
        .app_state = &g_device_name_state, .debug_name = "settings_device_name",
    };
    return &g_device_name_screen;
}

/* ============ About screen (real, honest about mock values) ============ */

static void about_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    (void)self;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "ABOUT", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    struct { const char *label; const char *value; bool mock; } rows[] = {
        {"tezOS version",    "0.1.0-dev",           false},
        {"Signer firmware",  "not built yet",        true},
        {"Board",            "Pi Zero 2 W",          false},
        {"Display panel",    "ILI9486 3.5\"",        false},
        {"Storage free",     "-- (no storage layer)", true},
        {"Battery",          "-- (no PiSugar link)",  true},
        {"Wi-Fi",             "-- (no network layer)", true},
        {"Signer link",       "disconnected (mock)",  true},
        {"Device name",       g_device_name,          false},
    };
    int y = 40;
    for (size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); i++) {
        tezos_draw_text(fb, d, 8, y, rows[i].label, &TEZOS_FONT_SMALL, TEZOS_MUTED);
        int vw = tezos_text_width(rows[i].value, &TEZOS_FONT_SMALL);
        tezos_draw_text(fb, d, fb->width - vw - 8, y, rows[i].value, &TEZOS_FONT_SMALL,
                        rows[i].mock ? TEZOS_MUTED : TEZOS_TEXT);
        y += 22;
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t about_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                         tezos_screen_t **next, bool *needs_redraw) {
    (void)self; (void)next; (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t g_about_screen = {
    .render = about_render, .handle_input = about_handle_input,
    .on_resume = NULL, .on_tick = NULL,
    .app_state = NULL, .debug_name = "settings_about",
};

/* ============ Main Settings list ============ */

typedef struct { tezos_list_state_t list; } settings_home_state_t;
static settings_home_state_t g_settings_home_state;

static tezos_list_item_t g_settings_items[8] = {
    {"Device Name", g_device_name}, /* secondary points at the live buffer --
                                        stable address, contents update live */
    {"Profiles", NULL},
    {"Lock / Passcode", NULL},
    {"Wi-Fi", NULL},
    {"Storage", NULL},
    {"Services", NULL},
    {"About", NULL},
    {"Import Recovery Phrase", NULL}, /* real -- T9 + BIP39 wordlist candidate
                                          entry, verified with a full 12-word
                                          real-typed test; see import_recovery.c */
};

static void settings_home_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    settings_home_state_t *st = (settings_home_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SETTINGS", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->list);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t settings_home_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                                 tezos_screen_t **next, bool *needs_redraw) {
    settings_home_state_t *st = (settings_home_state_t *)self->app_state;
    if (tezos_list_handle_input(&st->list, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }

    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;

    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        switch (st->list.selected) {
            case 0: *next = device_name_screen_get(); return TEZOS_ACTION_PUSH; /* real */
            case 6: *next = &g_about_screen; return TEZOS_ACTION_PUSH;          /* real */
            case 7: *next = import_recovery_entry_screen(); return TEZOS_ACTION_PUSH; /* real */
            default: /* Profiles, Lock, Wi-Fi, Storage, Services -- genuinely
                        blocked on backend pieces that don't exist yet */
                *next = tezos_stub_screen_get(g_settings_items[st->list.selected].label);
                return TEZOS_ACTION_PUSH;
        }
    }
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t g_settings_home_screen = {
    .render = settings_home_render, .handle_input = settings_home_handle_input,
    .on_resume = NULL, .on_tick = NULL,
    .app_state = &g_settings_home_state, .debug_name = "settings_home",
};

tezos_screen_t *settings_entry_screen(void) {
    tezos_list_init(&g_settings_home_state.list, g_settings_items, 8);
    return &g_settings_home_screen;
}

const char *settings_get_device_name(void) {
    return g_device_name;
}
