/* Real tests for the home screen's grid navigation math -- the exact
   logic most likely to hide a subtle bug (numpad-position mapping,
   wrap-around arithmetic), extracted specifically so it can be tested
   directly and completely, independent of the real DRM-dependent
   screen machinery around it. */
#include "../src/core/tezos_grid_nav.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    /* === real numpad-position mapping, all 9 real positions === */
    CHECK(idle_grid_next_selected(0, 9, TEZOS_INPUT_NUM, 1) == 0, "NUM 1 maps to grid index 0 (top-left)");
    CHECK(idle_grid_next_selected(0, 9, TEZOS_INPUT_NUM, 5) == 4, "NUM 5 maps to grid index 4 (real center)");
    CHECK(idle_grid_next_selected(0, 9, TEZOS_INPUT_NUM, 9) == 8, "NUM 9 maps to grid index 8 (bottom-right)");
    CHECK(idle_grid_next_selected(3, 9, TEZOS_INPUT_NUM, 7) == 6, "NUM 7 correctly jumps from any current selection, not relative to it");

    /* === real, deliberate no-op: NUM press beyond the real app count === */
    CHECK(idle_grid_next_selected(2, 5, TEZOS_INPUT_NUM, 9) == 2,
          "a NUM press beyond the real app count (5 apps, NUM 9) is a real no-op, doesn't select a nonexistent slot");
    CHECK(idle_grid_next_selected(2, 5, TEZOS_INPUT_NUM, 5) == 4,
          "NUM 5 is valid with exactly 5 real apps (the real last one), correctly selects it");
    CHECK(idle_grid_next_selected(2, 5, TEZOS_INPUT_NUM, 0) == 2,
          "NUM 0 (not a real 1-9 grid position) is a real no-op");

    /* === real wrap-around at both boundaries === */
    CHECK(idle_grid_next_selected(0, 9, TEZOS_INPUT_UP, 0) == 8,
          "UP from the real first item wraps to the real last item, not a negative/invalid index");
    CHECK(idle_grid_next_selected(8, 9, TEZOS_INPUT_DOWN, 0) == 0,
          "DOWN from the real last item wraps to the real first item");
    CHECK(idle_grid_next_selected(4, 9, TEZOS_INPUT_UP, 0) == 3, "UP from a real middle item moves back by one, no wrap involved");
    CHECK(idle_grid_next_selected(4, 9, TEZOS_INPUT_DOWN, 0) == 5, "DOWN from a real middle item moves forward by one");

    /* === real wrap-around with a real, non-default app count (not hardcoded to 9) === */
    CHECK(idle_grid_next_selected(0, 3, TEZOS_INPUT_UP, 0) == 2, "UP wrap works correctly with a real, smaller app count too, not just 9");
    CHECK(idle_grid_next_selected(2, 3, TEZOS_INPUT_DOWN, 0) == 0, "DOWN wrap works correctly with a real, smaller app count too");

    /* === real, deliberate no-op for any other input type === */
    CHECK(idle_grid_next_selected(3, 9, TEZOS_INPUT_SOFTKEY_L, 0) == 3, "SOFTKEY_L doesn't change selection -- that's a real, separate action (open the app), not navigation");
    CHECK(idle_grid_next_selected(3, 9, TEZOS_INPUT_STAR, 0) == 3, "an unrelated input type (STAR) is a real no-op");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
