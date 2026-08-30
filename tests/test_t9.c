/* T9 multi-tap widget -- logic checks (spelling, case toggle, backspace
   semantics, digit-cycle wraparound) plus the delay-based auto-commit
   mechanism (typing purely via the 800ms timeout, no Up presses). */
#include "../src/core/tezos_t9.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void tap(tezos_t9_state_t *s, tezos_input_type_t type, int value) {
    tezos_input_event_t ev = { type, value };
    tezos_t9_handle_input(s, ev);
}

int main(void) {
    tezos_t9_state_t s;
    tezos_t9_init(&s);

    tap(&s, TEZOS_INPUT_NUM, 4); tap(&s, TEZOS_INPUT_NUM, 4);
    tap(&s, TEZOS_INPUT_NUM, 3); tap(&s, TEZOS_INPUT_NUM, 3);
    tap(&s, TEZOS_INPUT_NUM, 5); tap(&s, TEZOS_INPUT_NUM, 5); tap(&s, TEZOS_INPUT_NUM, 5);
    tap(&s, TEZOS_INPUT_UP, 0);
    tap(&s, TEZOS_INPUT_NUM, 5); tap(&s, TEZOS_INPUT_NUM, 5); tap(&s, TEZOS_INPUT_NUM, 5);
    tap(&s, TEZOS_INPUT_NUM, 6); tap(&s, TEZOS_INPUT_NUM, 6); tap(&s, TEZOS_INPUT_NUM, 6);
    const char *text = tezos_t9_get_text(&s);
    CHECK(strcmp(text, "hello") == 0, "multi-tap spelling of \"hello\" (incl. same-key double-L via Up) is correct");

    tezos_t9_init(&s);
    tap(&s, TEZOS_INPUT_STAR, 0);
    tap(&s, TEZOS_INPUT_NUM, 4); tap(&s, TEZOS_INPUT_NUM, 4);
    tap(&s, TEZOS_INPUT_UP, 0);
    CHECK(strcmp(tezos_t9_get_text(&s), "H") == 0, "case toggle (*) produces uppercase");

    tezos_t9_init(&s);
    tap(&s, TEZOS_INPUT_NUM, 4); tap(&s, TEZOS_INPUT_NUM, 4); tap(&s, TEZOS_INPUT_UP, 0);
    tap(&s, TEZOS_INPUT_NUM, 2);
    tap(&s, TEZOS_INPUT_HASH, 0);
    CHECK(strcmp(tezos_t9_get_text(&s), "h") == 0, "HASH cancels a pending letter without touching committed text");
    tap(&s, TEZOS_INPUT_HASH, 0);
    CHECK(strcmp(tezos_t9_get_text(&s), "") == 0, "HASH with nothing pending backspaces the last committed char");

    tezos_t9_init(&s);
    tap(&s, TEZOS_INPUT_NUM, 2); tap(&s, TEZOS_INPUT_NUM, 2); tap(&s, TEZOS_INPUT_NUM, 2); tap(&s, TEZOS_INPUT_NUM, 2);
    tap(&s, TEZOS_INPUT_UP, 0);
    CHECK(strcmp(tezos_t9_get_text(&s), "2") == 0, "cycling past all letters on a key reaches the digit itself");

    tezos_t9_init(&s);
    tezos_input_event_t down_ev = {TEZOS_INPUT_DOWN, 0};
    CHECK(tezos_t9_handle_input(&s, down_ev) == false, "DOWN is left unclaimed by the widget");

    /* delay-based auto-commit */
    tezos_t9_init(&s);
    tap(&s, TEZOS_INPUT_NUM, 4); tap(&s, TEZOS_INPUT_NUM, 4);
    CHECK(tezos_t9_tick(&s, 400) == false, "tick before the delay elapses does not fire a commit");
    CHECK(tezos_t9_tick(&s, 400) == true, "tick crossing the 800ms threshold commits (returns true)");
    CHECK(strcmp(s.buffer, "h") == 0, "committed letter is correct");

    tezos_t9_init(&s);
    tap(&s, TEZOS_INPUT_NUM, 4); tap(&s, TEZOS_INPUT_NUM, 4); tezos_t9_tick(&s, 800);
    tap(&s, TEZOS_INPUT_NUM, 3); tap(&s, TEZOS_INPUT_NUM, 3); tezos_t9_tick(&s, 800);
    tap(&s, TEZOS_INPUT_NUM, 5); tap(&s, TEZOS_INPUT_NUM, 5); tap(&s, TEZOS_INPUT_NUM, 5); tezos_t9_tick(&s, 800);
    tap(&s, TEZOS_INPUT_NUM, 5); tap(&s, TEZOS_INPUT_NUM, 5); tap(&s, TEZOS_INPUT_NUM, 5); tezos_t9_tick(&s, 800);
    tap(&s, TEZOS_INPUT_NUM, 6); tap(&s, TEZOS_INPUT_NUM, 6); tap(&s, TEZOS_INPUT_NUM, 6); tezos_t9_tick(&s, 800);
    CHECK(strcmp(s.buffer, "hello") == 0, "\"hello\" typed using ONLY delay-based auto-commit, zero Up presses");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
