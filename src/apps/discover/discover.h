#pragma once
/*
 * Discover — a real, generic Tezos address browser, in the spirit of
 * an old-web browser: visit any address (like typing a URL), see
 * real basic info about what's there, browse what it holds with real
 * images if it has any, bookmark it for next time. Deliberately does
 * NOT hardcode a curated list of "known" marketplace contracts —
 * real browsers don't ship with someone else's fixed bookmark list
 * baked in, and this project has its own real, recent history of a
 * contract address being mislabeled with the wrong platform name (see
 * the Forge-report scrutiny in this project's own conversation
 * history) — shipping a small, hand-picked list of addresses this
 * project can't independently, continuously verify stay correct felt
 * like the wrong kind of confidence to bake into the product. Instead:
 * the real, already-tested address book (tezos_address_book.h) *is*
 * the bookmark list, exactly like a real browser's — user-curated,
 * not vendor-curated.
 *
 * Deliberately reuses existing, already-tested capability rather than
 * rebuilding it: tezos_tzkt_get_contract_info()/
 * get_entrypoints_with_types() (already real, already used by
 * Explorer) for "what is this address," tezos_tzkt_get_owned_tokens()
 * (already real, already used by Gallery, and already generic — takes
 * an explicit address, not implicitly "the active wallet") for
 * "what does it hold," and the real image decode/thumbnail pipeline
 * (tezos_image_decode.h + Camera's downsample/RGB565-convert
 * functions, already proven by Gallery's own real viewer) for
 * actually showing what's there, not just naming it. Nothing about
 * Discover required new chain-layer code — the generic capability
 * this project already had *was* the actual point of building it
 * generically in the first place.
 *
 * Real, deliberate scope for this pass: browsing an address's basic
 * info and its real token holdings, with real thumbnails. Calling an
 * entrypoint (the deeper, more involved interaction) is deliberately
 * left to Explorer, which already does this well — Discover's own
 * "CALL" action hands off to Explorer's real entry screen for that
 * address rather than rebuilding the whole dynamic-engine walker UI a
 * second time here, matching this project's own established
 * generic-first, don't-fragment-the-same-capability philosophy.
 */
#include "../../core/tezos_core.h"

tezos_screen_t *discover_entry_screen(void);

/* Exposed specifically for direct testing (see
   tests/test_discover.c) -- injects a real "already visited" state
   directly, with real, in-memory holdings/info data (including up to
   one real holding with an image URI, so the browsing/detail/viewer
   flow can be exercised end to end), so the browsing UI can be
   verified without a live network fetch. NOT for production use. */
tezos_screen_t *discover_test_inject_site(const char *address, const char *alias,
                                          const char *balance, const char *tx_count,
                                          int entrypoint_count,
                                          const char *holding_name, const char *holding_image_uri);
