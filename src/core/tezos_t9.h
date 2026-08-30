#pragma once
/*
 * T9 multi-tap text entry — the missing widget blocking Settings,
 * Messenger, Scanner, and Discover from having real (non-hardcoded)
 * text input.
 *
 * Standard phone-keypad multi-tap: press 2 once for 'a', twice for 'b',
 * three times for 'c', four times for the digit '2' itself.
 *
 * THREE WAYS A CYCLING LETTER COMMITS, in order of how they arise:
 *   1. Pressing a DIFFERENT digit — commits the previous letter, starts
 *      cycling the new key.
 *   2. Pressing Up — explicit "confirm now" fast-path. Also what makes
 *      typing two letters from the SAME key back to back (the double L
 *      in "hello") possible without waiting.
 *   3. A short pause with no key pressed (see tezos_t9_tick below) —
 *      matches how real phones behave, so typing "hello" doesn't
 *      *require* knowing about Up at all; it's a fast-path, not the
 *      only path.
 *
 * Why (3) needs its own function rather than being "free": tezos_core's
 * input dispatch (tezos_core_tick) only runs when a real input event
 * arrives — there's no built-in "time passed with nothing pressed"
 * event, and adding one to the core screen/input model for one widget's
 * benefit would be a bigger, more invasive change than this needed.
 * Instead, tezos_t9_tick() is a small, separate, self-contained
 * function: whatever drives the main loop (the platform shim, a test
 * harness) calls it on every wake-up with the elapsed milliseconds,
 * whether or not real input arrived that iteration. See
 * src/shim-linux/linux_shim.c's poll() loop for the reference
 * integration — it now polls with a timeout instead of blocking
 * indefinitely specifically so this can fire.
 */
#include "tezos_gfx.h"
#include "tezos_core.h"

#define TEZOS_T9_MAX_LEN 64

/* Real phones commonly land somewhere in the 500-1000ms range; 800ms
   split the difference and is easy to tune here if it feels wrong once
   real hardware/timing is in hand. */
#define TEZOS_T9_COMMIT_DELAY_MS 800

typedef struct {
    char buffer[TEZOS_T9_MAX_LEN + 1]; /* committed text, null-terminated */
    int length;

    int pending_key;       /* TEZOS_INPUT_NUM value (0-9) currently being
                               cycled, or -1 if nothing pending */
    int pending_tap_count; /* how many times pending_key has been tapped
                               in a row */
    int elapsed_since_tap_ms; /* reset to 0 on every digit tap; accumulated
                               by tezos_t9_tick() to drive delay-based
                               auto-commit */
    bool uppercase;        /* toggled by '*' — simple persistent toggle,
                               not real phones' shift/caps-lock tri-state */
} tezos_t9_state_t;

void tezos_t9_init(tezos_t9_state_t *state);

/* Handles NUM/STAR/HASH/UP per the scheme above. Returns true if a
   redraw is needed (same true-means-redraw convention as the rest of
   tezos_widgets.h). Does NOT consume DOWN, SOFTKEY_L, or SOFTKEY_R —
   those pass through unclaimed for the screen to handle (e.g. SOFTKEY_L
   = confirm the whole field and move on). */
bool tezos_t9_handle_input(tezos_t9_state_t *state, tezos_input_event_t ev);

/* Advances the delay-based auto-commit timer by elapsed_ms. Call this on
   every main-loop wake-up (see src/shim-linux/linux_shim.c) regardless
   of whether real input arrived. No-op if nothing is currently pending.
   Returns true (redraw needed) exactly when the pending letter actually
   commits — most calls just accumulate time and return false. */
bool tezos_t9_tick(tezos_t9_state_t *state, int elapsed_ms);

/* Commits any in-progress multi-tap cycle into the buffer. Safe to call
   any time; tezos_t9_get_text() calls this internally, so reading the
   text is always safe/complete even mid-composition — but a screen
   should still call this explicitly before leaving the field if it
   wants the render right after to reflect the committed state instead
   of the tentative one. */
void tezos_t9_commit_pending(tezos_t9_state_t *state);

/* Commits any pending letter, then returns the buffer. Always safe to
   call, always returns a complete, committed string. */
const char *tezos_t9_get_text(tezos_t9_state_t *state);

/* Renders the buffer plus, if mid-composition, the tentative letter in
   a distinguishing color (TEZOS_CYAN) so it's visually clear it's not
   committed yet. Draws a simple block cursor after the text. */
void tezos_t9_render(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y,
                     const tezos_font_t *font, const tezos_t9_state_t *state);
