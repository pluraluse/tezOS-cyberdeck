#pragma once
/*
 * Popups/confirmation dialogs — the most repeatedly-flagged open gap in
 * this project. Closes it for the most common case: a Yes/No
 * confirmation (e.g. "Send 1.5 tez to tz1...? CONFIRM / CANCEL"),
 * which is what most of Wallet's real flows actually need before they
 * can be considered complete.
 *
 * HOW THIS FITS THE EXISTING ARCHITECTURE, no core changes needed:
 * A popup is just a regular pushed tezos_screen_t. The one thing that
 * makes it look like an overlay rather than a full new screen: its
 * render() deliberately does NOT clear the whole framebuffer first
 * (unlike every other screen so far). The framebuffer's contents
 * persist between renders — nothing auto-clears it — so whatever the
 * PREVIOUS screen last drew stays visible around the edges, and the
 * popup only draws its own centered box on top. No alpha blending, no
 * new core primitive needed: just restraint about what NOT to draw.
 *
 * HOW A RESULT GETS BACK TO THE CALLER: the existing on_resume screen
 * callback already exists for exactly this "I was resumed after a
 * child screen popped" case — no new core mechanism needed there either.
 * The caller supplies a pointer to its own tezos_popup_result_t (a
 * field in its own state struct); the popup writes CONFIRMED or
 * CANCELLED into it right before popping itself, and the caller's
 * on_resume() checks it.
 */
#include "tezos_core.h"

typedef enum {
    TEZOS_POPUP_PENDING,   /* not yet answered -- set this before pushing */
    TEZOS_POPUP_CONFIRMED,
    TEZOS_POPUP_CANCELLED,
} tezos_popup_result_t;

/* Returns a configured confirm/cancel popup screen ready to push.
   `result_out` is caller-owned (NOT copied) -- keep it alive at least
   until the popup pops (a field in the caller's own static/persistent
   screen state satisfies this trivially). Left softkey = confirm
   (writes TEZOS_POPUP_CONFIRMED), right softkey = cancel (writes
   TEZOS_POPUP_CANCELLED) -- matches the SELECT/BACK softkey convention
   used everywhere else in the project. */
tezos_screen_t *tezos_popup_confirm_get(const char *title, const char *message,
                                        const char *confirm_label, const char *cancel_label,
                                        tezos_popup_result_t *result_out);
