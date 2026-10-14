/* Address book persistence, PART 1 of 2 -- run
   test_address_book_persist_p2 immediately after this to prove real
   cross-restart persistence (two separate process invocations sharing
   only the file on disk), same rigor as Settings' own two-part
   persistence test. */
#include "../src/core/tezos_address_book.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    remove("/tmp/tezos_cyberdeck_address_book.txt"); /* start genuinely clean,
        not via test_reset (which is fine here too, but removing the real
        file directly is the most explicit way to guarantee a clean start
        for this specific cross-process test) */

    CHECK(tezos_address_book_add("Alice", "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL"), "add Alice");
    CHECK(tezos_address_book_add("Some Contract", "KT1TxqZ8QtKvLu3V3JH7Gx58n7Co8pgtpQU5"), "add Some Contract");
    CHECK(tezos_address_book_count() == 2, "count is 2 in this process");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
