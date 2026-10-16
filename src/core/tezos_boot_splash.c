#include "tezos_boot_splash.h"
#include "tezos_gfx.h"
#include "tezos_fonts.h"
#include <string.h>

#define SPLASH_TEXT "tezOS"
#define SPLASH_SWEEP_MS 1400   /* time for the gleam to cross the whole wordmark */
#define SPLASH_HOLD_MS 500     /* brief hold on the fully-lit word before auto-advancing */
#define SPLASH_TOTAL_MS (SPLASH_SWEEP_MS + SPLASH_HOLD_MS)
#define SPLASH_GLEAM_HALFWIDTH 26 /* how many pixels either side of the sweep's
    center still count as "under the gleam" and get drawn bright */

typedef struct {
    int elapsed_ms;
    bool skip_requested;
} splash_state_t;
static splash_state_t g_splash_state;
static tezos_screen_t g_splash_screen;

static void splash_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    splash_state_t *st = (splash_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);

    int total_w = tezos_text_width(SPLASH_TEXT, &TEZOS_FONT_SPLASH_LOGO);
    int start_x = (fb->width - total_w) / 2;
    int y = (fb->height - TEZOS_FONT_SPLASH_LOGO.line_height) / 2;

    /* Sweep travels from just left of the word to just right of it, so
       the gleam has a clean entry and exit rather than snapping on/off
       at the wordmark's own edges. Clamped to the sweep phase only --
       once SPLASH_SWEEP_MS has elapsed the whole word just stays lit
       for the hold, no further movement. */
    int sweep_span = total_w + 2 * SPLASH_GLEAM_HALFWIDTH;
    int sweep_progress_ms = st->elapsed_ms < SPLASH_SWEEP_MS ? st->elapsed_ms : SPLASH_SWEEP_MS;
    int sweep_x = (start_x - SPLASH_GLEAM_HALFWIDTH) + (sweep_span * sweep_progress_ms) / SPLASH_SWEEP_MS;
    bool sweep_active = st->elapsed_ms < SPLASH_SWEEP_MS;

    /* Draw the wordmark one character at a time so each letter can be
       independently brightened as the gleam passes it -- this is what
       actually makes it look like light sweeping THROUGH the letters
       rather than a bar floating in front of static-colored text, and
       it only needs solid-color draws (no alpha/clipping support
       exists in this renderer, see tezos_gfx.h). */
    int x = start_x;
    for (const char *p = SPLASH_TEXT; *p; p++) {
        char ch[2] = { *p, '\0' };
        int w = tezos_text_width(ch, &TEZOS_FONT_SPLASH_LOGO);
        int char_center = x + w / 2;
        int dist = char_center - sweep_x;
        if (dist < 0) dist = -dist;
        bool lit = !sweep_active || dist < SPLASH_GLEAM_HALFWIDTH;
        tezos_color_t color = lit ? TEZOS_CYAN : TEZOS_PRIMARY;
        tezos_draw_text(fb, d, x, y, ch, &TEZOS_FONT_SPLASH_LOGO, color);
        x += w;
    }

    /* A bright vertical beam riding along with the gleam, reinforcing
       the "a beam of light" read specifically asked for -- drawn
       behind-in-spirit by being a separate accent rather than part of
       the letters themselves, so it reads as light passing across the
       sign rather than a static glow. */
    if (sweep_active) {
        int beam_x = sweep_x - 2;
        int beam_h = TEZOS_FONT_SPLASH_LOGO.line_height + 28;
        int beam_y = y - 14;
        if (beam_x >= 0 && beam_x + 4 <= fb->width) {
            tezos_draw_rect(fb, d, beam_x, beam_y, 4, beam_h, TEZOS_CYAN);
        }
    }
}

static void splash_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    splash_state_t *st = (splash_state_t *)self->app_state;
    if (st->elapsed_ms < SPLASH_TOTAL_MS) {
        st->elapsed_ms += elapsed_ms;
        *needs_redraw = true;
    }
}

static tezos_action_t splash_input(tezos_screen_t *self, tezos_input_event_t ev,
                                   tezos_screen_t **next, bool *needs_redraw) {
    (void)ev; (void)next; (void)needs_redraw;
    /* Any real input skips straight past the splash -- a captive
       animation on every single boot would be a real annoyance for
       someone who just wants to use the device, not a delight.
       Deliberately does NOT return TEZOS_ACTION_POP here: the splash is
       pushed as the very first (and, at this point, only) screen on
       the stack, and tezos_core_pop() correctly refuses to ever pop
       the last screen -- that would make this a silent no-op, not a
       real skip. Nor does it return TEZOS_ACTION_REPLACE with some
       hardcoded "next" screen: this core module has no business
       knowing about the platform shim's specific idle/home screen.
       Instead this just records that a skip was requested; the actual
       transition (to whatever the real home screen is) is the calling
       shim's job, driven by tezos_boot_splash_is_finished() the same
       way the timer-based auto-advance already is -- one real
       mechanism for both exit paths, not two different ones. */
    splash_state_t *st = (splash_state_t *)self->app_state;
    st->skip_requested = true;
    return TEZOS_ACTION_NONE;
}

tezos_screen_t *tezos_boot_splash_screen_get(void) {
    memset(&g_splash_state, 0, sizeof g_splash_state);
    g_splash_screen = (tezos_screen_t){
        .render = splash_render, .handle_input = splash_input,
        .on_resume = NULL, .on_tick = splash_on_tick,
        .app_state = &g_splash_state, .debug_name = "boot_splash",
    };
    return &g_splash_screen;
}

bool tezos_boot_splash_is_finished(tezos_screen_t *splash_screen) {
    if (!splash_screen || splash_screen->app_state != &g_splash_state) return false; /* real
        safety check -- a stale/wrong pointer should report "not finished" (safe default:
        keeps showing the splash) rather than silently misreport true */
    return g_splash_state.elapsed_ms >= SPLASH_TOTAL_MS || g_splash_state.skip_requested;
}
