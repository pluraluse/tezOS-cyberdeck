#pragma once
/*
 * tezOS shared widgets — built on tezos_gfx primitives, used by
 * tezos_core screens. This is the layer that stops every app from
 * re-solving "highlight a row, scroll when needed" from scratch.
 *
 * Same memory model as everywhere else in core: no malloc. Callers own
 * a static tezos_list_item_t array and pass it in; the widget only
 * tracks selection/scroll state, never allocates.
 */
#include "tezos_gfx.h"
#include "tezos_core.h"

typedef struct {
    const char *label;      /* left-aligned */
    const char *secondary;  /* right-aligned, or NULL */
} tezos_list_item_t;

typedef struct {
    const tezos_list_item_t *items;
    int item_count;
    int selected;       /* index into items */
    int scroll_offset;  /* index of first visible row */
    int visible_rows;   /* computed by tezos_list_render from the rect height */
} tezos_list_state_t;

void tezos_list_init(tezos_list_state_t *list, const tezos_list_item_t *items, int item_count);

/* Handles Up/Down. Returns true if selection moved (i.e. a redraw is
   needed) — mirrors the handle_input needs_redraw convention in
   tezos_core.h, so a screen can just forward its own out-param:
     *needs_redraw = tezos_list_handle_input(&st->list, ev) || *needs_redraw;
   Does not handle softkeys/select — that's the screen's job, since what
   "select" does (push a detail screen, trigger an action, etc.) is
   screen-specific, not something a generic list widget should assume. */
bool tezos_list_handle_input(tezos_list_state_t *list, tezos_input_event_t ev);

/* Renders visible rows within the given rect, scrolling automatically to
   keep the selected row in view. Selected row: TEZOS_PRIMARY background,
   TEZOS_BG text. Others: TEZOS_TEXT label, TEZOS_MUTED secondary. */
void tezos_list_render(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y, int w, int h,
                        const tezos_font_t *font, tezos_list_state_t *list);

/* --- Generic placeholder screen, for apps that don't have a real
   screen written yet --- */
/* Every not-yet-built app can push this instead of nothing, so PUSH/POP
   and the idle→app flow can be proven end-to-end before each app's real
   screen exists. Shows the app name and "not yet built" — never meant to
   ship, just unblocks wiring order. */
typedef struct {
    const char *app_name;
} tezos_stub_state_t;

tezos_screen_t *tezos_stub_screen_get(const char *app_name);
