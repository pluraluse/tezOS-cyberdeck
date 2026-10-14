#include "tezos_pin_entry.h"
#include "tezos_gfx.h"
#include "tezos_fonts.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    const char *title;
    char buffer[TEZOS_PIN_MAX_LEN + 1];
    int length;
    char *pin_out;
    tezos_pin_result_t *result_out;
} pin_state_t;

static pin_state_t g_pin_state;
static tezos_screen_t g_pin_screen;

static void pin_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    pin_state_t *st = (pin_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, st->title, &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    /* masked: show a dot per entered digit, not the digits themselves --
       this is exactly the input a shoulder-surfing observer shouldn't
       be able to read */
    char masked[TEZOS_PIN_MAX_LEN + 1];
    for (int i = 0; i < st->length; i++) masked[i] = '*';
    masked[st->length] = '\0';
    tezos_draw_text(fb, d, 8, 60, masked, &TEZOS_FONT_HEADER, TEZOS_TEXT);

    char count[32];
    snprintf(count, sizeof count, "%d digits entered", st->length);
    tezos_draw_text(fb, d, 8, 100, count, &TEZOS_FONT_SMALL, TEZOS_MUTED);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "CONFIRM", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t pin_input(tezos_screen_t *self, tezos_input_event_t ev,
                                tezos_screen_t **next, bool *needs_redraw) {
    pin_state_t *st = (pin_state_t *)self->app_state;
    (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) {
        *st->result_out = TEZOS_PIN_CANCELLED;
        return TEZOS_ACTION_POP;
    }
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        if (st->length == 0) return TEZOS_ACTION_NONE; /* don't confirm an empty PIN */
        strcpy(st->pin_out, st->buffer);
        *st->result_out = TEZOS_PIN_CONFIRMED;
        return TEZOS_ACTION_POP;
    }
    if (ev.type == TEZOS_INPUT_NUM && st->length < TEZOS_PIN_MAX_LEN) {
        st->buffer[st->length++] = (char)('0' + ev.value);
        st->buffer[st->length] = '\0';
        *needs_redraw = true;
        return TEZOS_ACTION_NONE;
    }
    if (ev.type == TEZOS_INPUT_HASH && st->length > 0) {
        st->buffer[--st->length] = '\0';
        *needs_redraw = true;
        return TEZOS_ACTION_NONE;
    }
    return TEZOS_ACTION_NONE;
}

tezos_screen_t *tezos_pin_entry_get(const char *title, char *pin_out, tezos_pin_result_t *result_out) {
    g_pin_state.title = title;
    g_pin_state.buffer[0] = '\0';
    g_pin_state.length = 0;
    g_pin_state.pin_out = pin_out;
    g_pin_state.result_out = result_out;
    *result_out = TEZOS_PIN_PENDING;

    g_pin_screen = (tezos_screen_t){
        .render = pin_render, .handle_input = pin_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_pin_state, .debug_name = "pin_entry",
    };
    return &g_pin_screen;
}
