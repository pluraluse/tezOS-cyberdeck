#include "tezos_widgets.h"
#include "tezos_fonts.h"
#include <string.h>

void tezos_list_init(tezos_list_state_t *list, const tezos_list_item_t *items, int item_count) {
    list->items = items;
    list->item_count = item_count;
    list->selected = 0;
    list->scroll_offset = 0;
    list->visible_rows = 0; /* set by render on first call */
}

bool tezos_list_handle_input(tezos_list_state_t *list, tezos_input_event_t ev) {
    if (list->item_count == 0) return false;
    switch (ev.type) {
        case TEZOS_INPUT_UP:
            if (list->selected > 0) {
                list->selected--;
                if (list->selected < list->scroll_offset) list->scroll_offset = list->selected;
                return true;
            }
            return false;
        case TEZOS_INPUT_DOWN:
            if (list->selected < list->item_count - 1) {
                list->selected++;
                if (list->visible_rows > 0 && list->selected >= list->scroll_offset + list->visible_rows) {
                    list->scroll_offset = list->selected - list->visible_rows + 1;
                }
                return true;
            }
            return false;
        default:
            return false;
    }
}

void tezos_list_render(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y, int w, int h,
                        const tezos_font_t *font, tezos_list_state_t *list) {
    int row_h = font->line_height + 6; /* a little breathing room per row */
    list->visible_rows = h / row_h;
    if (list->visible_rows < 1) list->visible_rows = 1;

    /* keep selection in view if the caller resized/rendered before an
       input event recalculated visible_rows (e.g. first render) */
    if (list->selected >= list->scroll_offset + list->visible_rows) {
        list->scroll_offset = list->selected - list->visible_rows + 1;
    }
    if (list->selected < list->scroll_offset) {
        list->scroll_offset = list->selected;
    }

    int row_y = y;
    int last = list->scroll_offset + list->visible_rows;
    if (last > list->item_count) last = list->item_count;

    for (int i = list->scroll_offset; i < last; i++) {
        const tezos_list_item_t *item = &list->items[i];
        bool sel = (i == list->selected);
        if (sel) {
            tezos_draw_rect(fb, d, x, row_y, w, row_h, TEZOS_PRIMARY);
        }
        tezos_color_t fg = sel ? TEZOS_BG : TEZOS_TEXT;
        tezos_draw_text(fb, d, x + 8, row_y + 3, item->label, font, fg);
        if (item->secondary) {
            int sw = tezos_text_width(item->secondary, font);
            tezos_color_t sfg = sel ? TEZOS_BG : TEZOS_MUTED;
            tezos_draw_text(fb, d, x + w - sw - 8, row_y + 3, item->secondary, font, sfg);
        }
        row_y += row_h;
    }
}

/* --- Generic stub screen --- */

#define TEZOS_MAX_STUB_INSTANCES 16
static tezos_screen_t g_stub_screens[TEZOS_MAX_STUB_INSTANCES];
static tezos_stub_state_t g_stub_states[TEZOS_MAX_STUB_INSTANCES];
static int g_stub_count = 0;

static void stub_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_stub_state_t *st = (tezos_stub_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 6, st->app_name, &TEZOS_FONT_SMALL, TEZOS_MUTED);

    int tw = tezos_text_width(st->app_name, &TEZOS_FONT_HEADER);
    tezos_draw_text(fb, d, (fb->width - tw) / 2, 180, st->app_name, &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    const char *msg = "not built yet";
    int mw = tezos_text_width(msg, &TEZOS_FONT_BODY);
    tezos_draw_text(fb, d, (fb->width - mw) / 2, 220, msg, &TEZOS_FONT_BODY, TEZOS_MUTED);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "-", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t stub_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                        tezos_screen_t **next, bool *needs_redraw) {
    (void)self; (void)next; (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) {
        return TEZOS_ACTION_POP; /* right softkey = Back, matching the established convention */
    }
    return TEZOS_ACTION_NONE;
}

tezos_screen_t *tezos_stub_screen_get(const char *app_name) {
    if (g_stub_count >= TEZOS_MAX_STUB_INSTANCES) return NULL; /* safe failure, not a crash */
    int idx = g_stub_count++;
    g_stub_states[idx].app_name = app_name;
    g_stub_screens[idx] = (tezos_screen_t){
        .render = stub_render,
        .handle_input = stub_handle_input,
        .on_resume = NULL,
        .app_state = &g_stub_states[idx],
        .debug_name = "stub",
    };
    return &g_stub_screens[idx];
}
