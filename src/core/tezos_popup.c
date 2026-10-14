#define _POSIX_C_SOURCE 200809L
#include "tezos_popup.h"
#include "tezos_gfx.h"
#include "tezos_fonts.h"
#include <string.h>
#include <stdio.h>

#define MAX_WRAP_LINES 5

/* strncpy always leaves the compiler's -Wstringop-truncation unsatisfied
   when the copy count exactly equals the destination's usable capacity,
   even with explicit post-copy null-termination -- this is a known
   false-positive class for this warning against the textbook-correct
   "strncpy + explicit null terminate" idiom; the behavior here is
   genuinely, verifiably safe (confirmed by test_popup.c), the warning
   just can't recognize that from this shape of code. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
static void safe_copy(char *dst, size_t dst_cap, const char *src) {
    strncpy(dst, src, dst_cap - 1);
    dst[dst_cap - 1] = '\0';
}
#pragma GCC diagnostic pop

/* title/message/confirm_label/cancel_label are copied into these fixed
   buffers (via safe_copy below), NOT stored as raw caller pointers --
   a real stack-use-after-return bug, caught by AddressSanitizer while
   building Explorer's entrypoint-calling flow: multiple callers
   (messenger.c, scanner.c, and now explorer.c) pass a stack-local
   snprintf'd buffer for `message` specifically, and the previous
   version of this struct stored that pointer directly, valid only
   until the CALLER's function returns -- which happens before the
   popup is ever actually rendered, since pushing a screen and
   rendering it happen in separate steps of tezos_core_tick's dispatch.
   Fixed at the source here rather than patching every individual
   caller, since the same mistake had already been made independently
   in at least 3 different files without being caught until now --
   whether it was ever actually triggered in the other two is unclear
   (stack-use-after-return detection can be non-deterministic depending
   on exact stack reuse), but the bug was real and latent regardless of
   whether ASan happened to catch it there too. */
typedef struct {
    char title[32];
    char message[160];
    char confirm_label[16];
    char cancel_label[16];
    tezos_popup_result_t *result_out;
} popup_state_t;

static popup_state_t g_popup_state;
static tezos_screen_t g_popup_screen;

/* Simple greedy word-wrap: packs whole words onto each line while they
   fit within max_width, breaking to a new line otherwise. Doesn't
   hyphenate an over-long single word (rare for the short confirmation
   messages this is meant for) -- it just lets that one line overflow
   rather than doing anything more elaborate. */
static int wrap_text(const char *message, const tezos_font_t *font, int max_width,
                     char lines[MAX_WRAP_LINES][128]) {
    int line_count = 0;
    char buf[512];
    strncpy(buf, message, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';

    char current_line[128] = "";
    char *saveptr = NULL;
    char *word = strtok_r(buf, " ", &saveptr);
    while (word && line_count < MAX_WRAP_LINES) {
        char trial[300]; /* generous: worst case is a 127-char line + space + 127-char word */
        if (current_line[0] == '\0') snprintf(trial, sizeof trial, "%s", word);
        else snprintf(trial, sizeof trial, "%s %s", current_line, word);

        if (tezos_text_width(trial, font) <= max_width) {
            safe_copy(current_line, sizeof current_line, trial);
        } else {
            if (current_line[0] != '\0') {
                safe_copy(lines[line_count], sizeof lines[0], current_line);
                line_count++;
            }
            safe_copy(current_line, sizeof current_line, word);
        }
        word = strtok_r(NULL, " ", &saveptr);
    }
    if (current_line[0] != '\0' && line_count < MAX_WRAP_LINES) {
        safe_copy(lines[line_count], sizeof lines[0], current_line);
        line_count++;
    }
    return line_count;
}

static void popup_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    popup_state_t *st = (popup_state_t *)self->app_state;
    /* Deliberately NOT clearing the framebuffer first -- see header.
       Whatever the previous screen last drew stays visible around this
       box. */

    int box_w = fb->width - 32;
    int box_h = 180;
    int box_x = 16;
    int box_y = (fb->height - box_h) / 2;

    /* Box background a shade off TEZOS_BG so it reads as a distinct
       layer, not a continuation of whatever's behind it. */
    tezos_color_t box_bg = {0x18, 0x1E, 0x4A};
    tezos_draw_rect(fb, d, box_x, box_y, box_w, box_h, box_bg);
    tezos_draw_rect_outline(fb, d, box_x, box_y, box_w, box_h, TEZOS_PRIMARY);

    int inner_x = box_x + 16;
    int y = box_y + 14;
    tezos_draw_text(fb, d, inner_x, y, st->title, &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    y += 30;
    tezos_draw_rect(fb, d, box_x + 8, y, box_w - 16, 1, TEZOS_MUTED);
    y += 12;

    char lines[MAX_WRAP_LINES][128];
    int n = wrap_text(st->message, &TEZOS_FONT_SMALL, box_w - 32, lines);
    for (int i = 0; i < n; i++) {
        tezos_draw_text(fb, d, inner_x, y, lines[i], &TEZOS_FONT_SMALL, TEZOS_TEXT);
        y += 20;
    }

    /* button row, inside the box */
    int btn_y = box_y + box_h - 30;
    tezos_draw_rect(fb, d, box_x + 8, btn_y - 6, box_w - 16, 1, TEZOS_MUTED);
    tezos_draw_text(fb, d, inner_x, btn_y, st->confirm_label, &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int cancel_w = tezos_text_width(st->cancel_label, &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, box_x + box_w - cancel_w - 16, btn_y, st->cancel_label, &TEZOS_FONT_SMALL, TEZOS_PINK);

    /* mark the whole box (not the whole screen) dirty */
    if (d) tezos_dirty_mark(d, box_x, box_y, box_w, box_h);
}

static tezos_action_t popup_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                         tezos_screen_t **next, bool *needs_redraw) {
    popup_state_t *st = (popup_state_t *)self->app_state;
    (void)next; (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        *st->result_out = TEZOS_POPUP_CONFIRMED;
        return TEZOS_ACTION_POP;
    }
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) {
        *st->result_out = TEZOS_POPUP_CANCELLED;
        return TEZOS_ACTION_POP;
    }
    return TEZOS_ACTION_NONE;
}

tezos_screen_t *tezos_popup_confirm_get(const char *title, const char *message,
                                        const char *confirm_label, const char *cancel_label,
                                        tezos_popup_result_t *result_out) {
    safe_copy(g_popup_state.title, sizeof g_popup_state.title, title);
    safe_copy(g_popup_state.message, sizeof g_popup_state.message, message);
    safe_copy(g_popup_state.confirm_label, sizeof g_popup_state.confirm_label, confirm_label);
    safe_copy(g_popup_state.cancel_label, sizeof g_popup_state.cancel_label, cancel_label);
    g_popup_state.result_out = result_out;
    *result_out = TEZOS_POPUP_PENDING;
    g_popup_screen = (tezos_screen_t){
        .render = popup_render, .handle_input = popup_handle_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_popup_state, .debug_name = "popup_confirm",
    };
    return &g_popup_screen;
}
