#include "import_recovery.h"
#include "../wallet/wallet.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../core/tezos_t9.h"
#include "../../chain/tezos_bip39.h"
#include "../../chain/tezos_bip39_wordlist.h"
#include "../../chain/tezos_signer.h"
#include <string.h>
#include <stdio.h>

#define WORD_COUNT 12 /* scope decision: 12-word phrases only for now.
    24-word import would just mean a bigger confirmed_words array and a
    longer loop -- not implemented yet because nothing in this project
    generates a 24-word phrase to test against, and 12 covers the
    overwhelming majority of real wallets including Kukai's default. */
#define MAX_CANDIDATES 8

typedef enum { PHASE_ENTERING, PHASE_REVIEW_FAIL, PHASE_IMPORTED } import_phase_t;

typedef struct {
    import_phase_t phase;
    char confirmed_words[WORD_COUNT][16];
    int confirmed_count;

    tezos_t9_state_t t9;
    tezos_list_state_t candidates;
    tezos_list_item_t candidate_items[MAX_CANDIDATES];
    int candidate_count;

    char assembled_phrase[256]; /* built once entry completes, for validate/import */
    char imported_address[128];
} import_state_t;

static import_state_t g_import_state;
static tezos_screen_t g_import_screen;

static void refilter_candidates(import_state_t *st) {
    /* Read the COMMITTED buffer directly -- NOT via tezos_t9_get_text(),
       which has a destructive side effect (force-commits any
       mid-multi-tap letter). Calling get_text() here on every keystroke
       was the actual bug: it prematurely committed each intermediate
       tap of a multi-tap cycle as if it were a finished letter,
       corrupting the whole word. Candidates now only update once a
       letter actually commits (different-key press, Up, or the delay
       timeout) rather than live on every single tap -- a reasonable
       trade for correctness. */
    const char *prefix = st->t9.buffer;
    size_t prefix_len = strlen(prefix);
    st->candidate_count = 0;
    if (prefix_len == 0) {
        tezos_list_init(&st->candidates, st->candidate_items, 0);
        return;
    }
    for (int i = 0; i < 2048 && st->candidate_count < MAX_CANDIDATES; i++) {
        const char *word = TEZOS_BIP39_WORDLIST[i];
        if (strncmp(word, prefix, prefix_len) == 0) {
            st->candidate_items[st->candidate_count].label = word;
            st->candidate_items[st->candidate_count].secondary = NULL;
            st->candidate_count++;
        }
    }
    tezos_list_init(&st->candidates, st->candidate_items, st->candidate_count);
}

static void entering_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    import_state_t *st = (import_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "IMPORT PHRASE", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    char progress[32];
    snprintf(progress, sizeof progress, "Word %d of %d", st->confirmed_count + 1, WORD_COUNT);
    tezos_draw_text(fb, d, 8, 38, progress, &TEZOS_FONT_SMALL, TEZOS_MUTED);

    /* show the last couple confirmed words for context */
    int y = 60;
    int start = st->confirmed_count > 2 ? st->confirmed_count - 2 : 0;
    for (int i = start; i < st->confirmed_count; i++) {
        char line[32];
        snprintf(line, sizeof line, "%d. %s", i + 1, st->confirmed_words[i]);
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_MUTED);
        y += 18;
    }

    y += 8;
    tezos_draw_rect(fb, d, 8, y, fb->width - 16, 2, TEZOS_MUTED);
    y += 10;
    tezos_t9_render(fb, d, 8, y, &TEZOS_FONT_BODY, &st->t9);
    y += 32;

    if (st->candidate_count > 0) {
        tezos_list_render(fb, d, 8, y, fb->width - 16, fb->height - y - 40, &TEZOS_FONT_BODY, &st->candidates);
    } else if (strlen(st->t9.buffer) == 0) {
        tezos_draw_text(fb, d, 8, y, "Type letters to see matches", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        tezos_draw_text(fb, d, 8, y, "No matching word", &TEZOS_FONT_SMALL, TEZOS_PINK);
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void review_fail_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    (void)self;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "IMPORT PHRASE", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "Checksum invalid.", &TEZOS_FONT_BODY, TEZOS_PINK);
    tezos_draw_text(fb, d, 8, 80, "One or more words may", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 98, "be wrong, or out of order.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("RETRY", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "RETRY", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void imported_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    import_state_t *st = (import_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "IMPORTED", &TEZOS_FONT_HEADER, TEZOS_CYAN);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    tezos_draw_text(fb, d, 8, 44, "New active address:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 62, st->imported_address, &TEZOS_FONT_SMALL, TEZOS_TEXT);

    tezos_draw_rect(fb, d, 0, 92, fb->width, 1, TEZOS_MUTED);
    /* Confirmed against Kukai's own official docs (docs.kukai.app) and
       their own open-source kukai-crypto-swift library, both stating
       path 44'/1729'/0'/0' via SLIP-0010 -- the exact same convention
       and algorithm this device uses. Not a live-tested cross-check
       against a running Kukai instance, but a direct, credible
       confirmation from Kukai's own source, not an assumption. */
    const char *lines[] = {
        "Uses path 44'/1729'/0'/0'",
        "-- confirmed to match Kukai's",
        "own docs + open-source",
        "library (same convention as",
        "Ledger/Temple too).",
    };
    int y = 100;
    for (size_t i = 0; i < sizeof(lines)/sizeof(lines[0]); i++) {
        tezos_draw_text(fb, d, 8, y, lines[i], &TEZOS_FONT_SMALL, TEZOS_CYAN);
        y += 18;
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("DONE", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "DONE", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void try_finish_import(import_state_t *st) {
    /* assemble the full phrase */
    st->assembled_phrase[0] = '\0';
    for (int i = 0; i < WORD_COUNT; i++) {
        strcat(st->assembled_phrase, st->confirmed_words[i]);
        if (i < WORD_COUNT - 1) strcat(st->assembled_phrase, " ");
    }

    if (!tezos_bip39_validate(st->assembled_phrase, WORD_COUNT)) {
        st->phase = PHASE_REVIEW_FAIL;
        return;
    }

    tezos_mock_signer_t signer;
    if (!tezos_mock_signer_from_mnemonic(&signer, st->assembled_phrase, WORD_COUNT, "", 0)) {
        st->phase = PHASE_REVIEW_FAIL; /* shouldn't happen if validate() passed, but don't assume */
        return;
    }

    wallet_set_active_signer(&signer);
    tezos_mock_signer_address_b58(&signer, st->imported_address, sizeof st->imported_address);
    st->phase = PHASE_IMPORTED;
}

static tezos_action_t entering_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                            tezos_screen_t **next, bool *needs_redraw) {
    import_state_t *st = (import_state_t *)self->app_state;
    (void)next;

    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP; /* CANCEL */

    if (ev.type == TEZOS_INPUT_SOFTKEY_L) { /* SELECT the highlighted candidate */
        tezos_t9_commit_pending(&st->t9); /* in case a letter is still mid-cycle */
        refilter_candidates(st);
        if (st->candidate_count == 0) return TEZOS_ACTION_NONE;
        const char *word = st->candidate_items[st->candidates.selected].label;
        strncpy(st->confirmed_words[st->confirmed_count], word, 15);
        st->confirmed_count++;
        tezos_t9_init(&st->t9);
        refilter_candidates(st);
        *needs_redraw = true;
        if (st->confirmed_count == WORD_COUNT) try_finish_import(st);
        return TEZOS_ACTION_NONE;
    }

    if (ev.type == TEZOS_INPUT_DOWN) {
        if (tezos_list_handle_input(&st->candidates, ev)) *needs_redraw = true;
        return TEZOS_ACTION_NONE;
    }

    /* everything else (NUM/STAR/HASH/UP) goes to T9 */
    if (tezos_t9_handle_input(&st->t9, ev)) {
        refilter_candidates(st);
        *needs_redraw = true;
    }
    return TEZOS_ACTION_NONE;
}

static void entering_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    import_state_t *st = (import_state_t *)self->app_state;
    if (tezos_t9_tick(&st->t9, elapsed_ms)) {
        refilter_candidates(st);
        *needs_redraw = true;
    }
}

static tezos_action_t review_fail_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                               tezos_screen_t **next, bool *needs_redraw) {
    import_state_t *st = (import_state_t *)self->app_state;
    (void)next; (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) { /* RETRY -- reset entirely */
        st->confirmed_count = 0;
        tezos_t9_init(&st->t9);
        refilter_candidates(st);
        st->phase = PHASE_ENTERING;
        return TEZOS_ACTION_NONE;
    }
    return TEZOS_ACTION_NONE;
}

static tezos_action_t imported_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                            tezos_screen_t **next, bool *needs_redraw) {
    (void)self; (void)next; (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    return TEZOS_ACTION_NONE;
}

/* dispatches render/input to whichever phase is active */
static void screen_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    import_state_t *st = (import_state_t *)self->app_state;
    switch (st->phase) {
        case PHASE_ENTERING:    entering_render(self, fb, d); break;
        case PHASE_REVIEW_FAIL: review_fail_render(self, fb, d); break;
        case PHASE_IMPORTED:    imported_render(self, fb, d); break;
    }
}
static tezos_action_t screen_input(tezos_screen_t *self, tezos_input_event_t ev,
                                   tezos_screen_t **next, bool *needs_redraw) {
    import_state_t *st = (import_state_t *)self->app_state;
    switch (st->phase) {
        case PHASE_ENTERING:    return entering_handle_input(self, ev, next, needs_redraw);
        case PHASE_REVIEW_FAIL: return review_fail_handle_input(self, ev, next, needs_redraw);
        case PHASE_IMPORTED:    return imported_handle_input(self, ev, next, needs_redraw);
    }
    return TEZOS_ACTION_NONE;
}
static void screen_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    import_state_t *st = (import_state_t *)self->app_state;
    if (st->phase == PHASE_ENTERING) entering_on_tick(self, elapsed_ms, needs_redraw);
}

tezos_screen_t *import_recovery_entry_screen(void) {
    memset(&g_import_state, 0, sizeof g_import_state);
    g_import_state.phase = PHASE_ENTERING;
    tezos_t9_init(&g_import_state.t9);
    refilter_candidates(&g_import_state);

    g_import_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = NULL, .on_tick = screen_tick,
        .app_state = &g_import_state, .debug_name = "import_recovery",
    };
    return &g_import_screen;
}
