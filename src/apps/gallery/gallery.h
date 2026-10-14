#pragma once
/*
 * Gallery — real owned-token viewer, built on
 * tezos_tzkt_get_owned_tokens() (tezos_tzkt.h), which is itself the
 * same real, live-verified /v1/tokens/balances endpoint already used
 * for Messenger's Inbox, generalized to "every contract this address
 * holds anything on" rather than one specific contract.
 *
 * SCOPE, deliberate: a real list (contract alias + token ID + name)
 * and a real detail view (adds contract address, balance). GLB 3D
 * model viewing (mentioned in this app's original STATUS.md as "an
 * explicit afterthought, not a core feature") is NOT attempted here —
 * this device's display pipeline has no 3D rendering capability at
 * all, and building one would be a large, separate project. Image
 * thumbnails are also not fetched/decoded here — the real metadata
 * often includes a thumbnailUri (usually IPFS), but decoding arbitrary
 * remote images into the framebuffer is real, separate work (format
 * detection, decode, dither-to-panel-depth) not attempted in this
 * pass. The name/contract/token-ID list this app shows is real data,
 * not a placeholder for the eventual richer version.
 */
#include "../../core/tezos_core.h"
#include <stdint.h>

tezos_screen_t *gallery_entry_screen(void);

/* Exposed specifically for direct testing (see tests/test_gallery_ui.c)
   -- same reasoning as messenger.h's messenger_test_inject_inbox: lets
   list -> detail -> back navigation be verified with actual token data
   present, since this sandbox can't reach live network data to
   populate it naturally. NOT for production use. */
tezos_screen_t *gallery_test_inject_tokens(const char *const *aliases, const char *const *names,
                                           const char *const *token_ids, const char *const *balances,
                                           int count);

/* Exposed specifically for direct testing (see tests/test_gallery_viewer.c)
   -- injects a real, synthetic RGB565 image directly into the pannable
   1:1 viewer's state, bypassing the real network fetch/decode entirely
   (which can't be exercised live in this sandbox) so the actual pan/
   clamp mechanics can be verified directly. NOT for production use. */
tezos_screen_t *gallery_test_inject_viewer(const uint16_t *pixels, int w, int h);
