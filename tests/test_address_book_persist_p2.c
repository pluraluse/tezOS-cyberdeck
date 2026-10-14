/* Part 2 -- a genuinely separate process, sharing only the on-disk
   file p1 wrote. Confirms real persistence, not just in-memory state
   surviving within one process. */
#include "../src/core/tezos_address_book.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    /* deliberately NOT calling test_reset or remove() here -- the whole
       point is loading whatever p1's separate process already wrote */
    CHECK(tezos_address_book_count() == 2, "count is still 2 in this genuinely separate process");

    tezos_address_book_entry_t entry;
    CHECK(tezos_address_book_get(0, &entry) && strcmp(entry.name, "Alice") == 0
          && strcmp(entry.address, "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL") == 0,
          "Alice's real data survived a genuine cross-process restart");
    CHECK(tezos_address_book_get(1, &entry) && strcmp(entry.name, "Some Contract") == 0,
          "Some Contract's real data survived too, in the correct order");

    tezos_address_book_test_reset(); /* clean up after this test pair */
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
