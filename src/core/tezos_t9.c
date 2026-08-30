#include "tezos_t9.h"
#include <string.h>

/* Standard phone T9 letter sets. Index = digit. Each string's last
   character is the digit itself, reachable by cycling past the letters
   — matches real phone behavior (you can still type "2", not just
   letters). 0 = space then digit; 1 = punctuation, no letters. */
static const char *KEY_CHARS[10] = {
    " 0",              /* 0 */
    ".,'?!\"-()@/:_1",  /* 1 */
    "abc2",            /* 2 */
    "def3",            /* 3 */
    "ghi4",            /* 4 */
    "jkl5",            /* 5 */
    "mno6",            /* 6 */
    "pqrs7",           /* 7 */
    "tuv8",            /* 8 */
    "wxyz9",           /* 9 */
};

void tezos_t9_init(tezos_t9_state_t *state) {
    state->buffer[0] = '\0';
    state->length = 0;
    state->pending_key = -1;
    state->pending_tap_count = 0;
    state->elapsed_since_tap_ms = 0;
    state->uppercase = false;
}

static char current_pending_char(const tezos_t9_state_t *state) {
    const char *chars = KEY_CHARS[state->pending_key];
    int n = (int)strlen(chars);
    char c = chars[state->pending_tap_count % n];
    if (state->uppercase && c >= 'a' && c <= 'z') c -= ('a' - 'A');
    return c;
}

void tezos_t9_commit_pending(tezos_t9_state_t *state) {
    if (state->pending_key < 0) return;
    if (state->length < TEZOS_T9_MAX_LEN) {
        state->buffer[state->length++] = current_pending_char(state);
        state->buffer[state->length] = '\0';
    }
    state->pending_key = -1;
    state->pending_tap_count = 0;
    state->elapsed_since_tap_ms = 0;
}

bool tezos_t9_handle_input(tezos_t9_state_t *state, tezos_input_event_t ev) {
    switch (ev.type) {
        case TEZOS_INPUT_NUM: {
            int digit = ev.value; /* 0-9 */
            state->elapsed_since_tap_ms = 0; /* timer measures time SINCE LAST tap,
                not since the key was first pressed -- restart on every tap */
            if (state->pending_key == digit) {
                state->pending_tap_count++; /* cycle to next letter on this key */
            } else {
                tezos_t9_commit_pending(state); /* different key: commit previous first */
                state->pending_key = digit;
                state->pending_tap_count = 0;
            }
            return true;
        }
        case TEZOS_INPUT_STAR:
            state->uppercase = !state->uppercase;
            return true; /* re-cases the live tentative letter too, via current_pending_char */
        case TEZOS_INPUT_HASH:
            if (state->pending_key >= 0) {
                /* cancel the in-progress letter, don't touch committed buffer */
                state->pending_key = -1;
                state->pending_tap_count = 0;
            } else if (state->length > 0) {
                state->buffer[--state->length] = '\0';
            }
            return true;
        case TEZOS_INPUT_UP:
            if (state->pending_key >= 0) {
                tezos_t9_commit_pending(state); /* explicit confirm-and-advance,
                    see header comment: this is what enables typing two
                    letters from the same key back to back */
                return true;
            }
            return false; /* nothing pending: not this widget's concern, let it pass through */
        default:
            return false; /* DOWN, softkeys, etc. -- unclaimed, screen's job */
    }
}

const char *tezos_t9_get_text(tezos_t9_state_t *state) {
    tezos_t9_commit_pending(state);
    return state->buffer;
}

bool tezos_t9_tick(tezos_t9_state_t *state, int elapsed_ms) {
    if (state->pending_key < 0) return false; /* nothing cycling, nothing to time out */
    state->elapsed_since_tap_ms += elapsed_ms;
    if (state->elapsed_since_tap_ms >= TEZOS_T9_COMMIT_DELAY_MS) {
        tezos_t9_commit_pending(state);
        return true; /* the tentative cyan letter just became committed text -- redraw */
    }
    return false;
}

void tezos_t9_render(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y,
                     const tezos_font_t *font, const tezos_t9_state_t *state) {
    int cursor_x = x + tezos_draw_text(fb, d, x, y, state->buffer, font, TEZOS_TEXT);
    if (state->pending_key >= 0) {
        char tentative[2] = { current_pending_char(state), '\0' };
        cursor_x += tezos_draw_text(fb, d, cursor_x, y, tentative, font, TEZOS_CYAN);
    }
    /* simple block cursor */
    tezos_draw_rect(fb, d, cursor_x + 2, y, 2, font->line_height, TEZOS_MUTED);
}
