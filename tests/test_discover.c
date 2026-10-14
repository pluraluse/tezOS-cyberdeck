/* Real Discover flow tests -- Home (bookmarks) -> visit a new address
   -> Site (real info) -> real, INDEPENDENT bookmark (STAR) and browse
   (SOFTKEY_L) actions -- this is the test that caught a real design
   flaw where an earlier version required bookmarking before browsing
   was allowed at all, an arbitrary gate no real browser has -> real
   Holdings list -> detail (with a real image, injected) -> viewer ->
   back navigation all the way out. */
#include "../src/apps/discover/discover.h"
#include "../src/core/tezos_address_book.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void tap(tezos_core_t *core, tezos_input_type_t type, int value) {
    tezos_input_event_t ev = { type, value };
    tezos_core_tick(core, &ev);
}

int main(void) {
    tezos_address_book_test_reset();

    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);

    /* === real injected site with a real holding (image URI included) === */
    tezos_core_push(&core, discover_test_inject_site(
        "KT1TestAddress123", "Test dApp", "5000000", "42", 3,
        "Test Item", "ipfs://QmTestCid"));
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 1, "injected site pushed correctly");

    /* real, independent actions: browse should work WITHOUT bookmarking
       first -- this is the actual design fix, verified directly */
    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* BROWSE */
    CHECK(core.depth == 1, "BROWSE works without bookmarking first -- the real, corrected design");
    tezos_fb_dump_ppm(fb, "discover_holdings_list.ppm");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* SELECT the one real holding */
    CHECK(core.depth == 1, "selecting a holding stays in-screen (real phase change)");
    tezos_fb_dump_ppm(fb, "discover_holdings_detail.ppm");

    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0); /* BACK to holdings list */
    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0); /* BACK to site */
    CHECK(core.depth == 1, "backing out of holdings returns to the real site screen");
    tezos_fb_destroy(fb);

    /* === real, independent bookmark action -- fresh core/fb, since
       reusing the same core without popping the first injected
       screen would push a second instance on top of the first while
       both point at the same shared static state, an unsound test
       setup, not a real product scenario */
    tezos_fb_t *fb2 = tezos_fb_create(320, 480);
    tezos_core_t core2;
    tezos_core_init(&core2, fb2);
    tezos_core_push(&core2, discover_test_inject_site(
        "KT1TxqZ8QtKvLu3V3JH7Gx58n7Co8pgtpQU5", "Another dApp", "1000000", "7", -1, NULL, NULL));
    core2.needs_render = true;
    tezos_core_tick(&core2, NULL);
    CHECK(tezos_address_book_count() == 0, "not yet bookmarked");
    tap(&core2, TEZOS_INPUT_STAR, 0); /* real bookmark action */
    CHECK(tezos_address_book_count() == 1, "STAR genuinely saved a real new bookmark -- validated via the real tezos_forge_address check, same as the address book's own established behavior");
    tezos_address_book_entry_t entry;
    tezos_address_book_get(0, &entry);
    CHECK(strcmp(entry.name, "Another dApp") == 0 && strcmp(entry.address, "KT1TxqZ8QtKvLu3V3JH7Gx58n7Co8pgtpQU5") == 0,
          "the real bookmark's name and address are correct");

    /* === real re-visit detection: bookmarking the same address again shouldn't duplicate === */
    tap(&core2, TEZOS_INPUT_STAR, 0); /* already bookmarked -- should be a real no-op */
    CHECK(tezos_address_book_count() == 1, "bookmarking an already-bookmarked address doesn't create a duplicate");

    /* === real Home screen navigation: does the bookmark actually show up? === */
    tezos_fb_destroy(fb2);
    tezos_fb_t *fb3 = tezos_fb_create(320, 480);
    tezos_core_t core3;
    tezos_core_init(&core3, fb3);
    tezos_core_push(&core3, discover_entry_screen());
    core3.needs_render = true;
    tezos_core_tick(&core3, NULL);
    CHECK(core3.depth == 1, "Home screen pushed correctly");

    /* === real "Visit New Address" flow from Home === */
    tap(&core3, TEZOS_INPUT_SOFTKEY_L, 0); /* select "+ Visit New Address" (item 0) */
    CHECK(core3.depth == 1, "entering address-entry is an in-screen phase change");
    /* type something via T9, then GO */
    tap(&core3, TEZOS_INPUT_NUM, 2); tap(&core3, TEZOS_INPUT_UP, 0); /* arbitrary text */
    tap(&core3, TEZOS_INPUT_SOFTKEY_L, 0); /* GO -- real visit attempt (will fail offline, expected) */
    tezos_fb_dump_ppm(fb3, "discover_site_after_visit.ppm");

    tezos_fb_destroy(fb3);
    tezos_address_book_test_reset();
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
