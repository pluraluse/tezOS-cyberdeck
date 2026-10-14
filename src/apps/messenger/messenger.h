#pragma once
/*
 * Messenger — real send, built on the real Teia Channels-family
 * contract's actual write_message entrypoint. See
 * docs/build-notes/messenger-app-build-notes.md for the full history:
 * two earlier research passes (reverse-engineered frontend source, an
 * unmerged draft PR) both described a DIFFERENT design than what the
 * live contract data actually shows -- treat that file's "CORRECTION"
 * section as authoritative over the sections below it.
 *
 * SCOPE, deliberate, v1: single receiver, no cc/bcc/attachments/
 * threading/token-gating -- write_message's real type supports all of
 * those (see the build notes), this just doesn't expose them in the UI
 * yet. token_metadata (a `map`, out of scope in the general dynamic
 * engine) is always sent as a real, correctly-encoded EMPTY map, not
 * approximated. Every optional field this v1 doesn't expose is a real
 * Micheline "None", not a placeholder value.
 *
 * NOT BUILT: reading/displaying received messages. That needs a real
 * TZKT capability this project doesn't have yet -- querying an
 * address's FA2 token balances on this contract (each held token ID is
 * believed to correspond to a received message ID, based on the real
 * storage schema showing a `message_infos` big_map alongside a
 * standard FA2 ledger -- a real, load-bearing assumption, not yet
 * independently confirmed). The Inbox row currently pushes an honest
 * "not built yet" screen, not fake data.
 *
 * The contract address is a real Settings value
 * (settings_get_messenger_contract()), not hardcoded -- defaults to
 * KT19VtkiKbg69tQW2iVkpRLc9h3mkS8y9MMw, confirmed by the user directly
 * against TZKT's own storage schema as the real source of the
 * write_message/message_infos data this app is built against
 * (resolving an earlier two-candidate-address uncertainty in this
 * project's history) -- still user-editable in Settings rather than a
 * bare constant, since that confirms the data's origin, not
 * necessarily that it's what Teia's production app currently calls.
 */
#include "../../core/tezos_core.h"
#include "../../chain/tezos_micheline.h"

tezos_screen_t *messenger_entry_screen(void);

/* Exposed specifically for direct testing (see tests/test_inbox_ui.c)
   -- injects synthetic messages directly into the Inbox list state
   and pushes the real messenger screen already on MSG_INBOX_LIST, so
   the list -> detail -> back navigation can be verified even though
   this sandbox can't reach real network data to populate it
   naturally. NOT for production use -- real inbox data always comes
   from messenger_entry_screen()'s real fetch. */
tezos_screen_t *messenger_test_inject_inbox(const char *const *senders, const char *const *subjects,
                                            const char *const *contents, const char *const *timestamps,
                                            int count);

/* Exposed specifically for direct testing (see
   tests/test_messenger.c) -- builds write_message's real Micheline
   parameter value. Not part of the "public app API" in the same sense
   as messenger_entry_screen; every other app in this project tests
   this kind of thing indirectly through the real UI flow instead, but
   this function's correctness (an exact match to a real, hand-verified
   contract entrypoint's parameter shape) is important enough to check
   directly, not just trust via an end-to-end flow that could mask a
   structural mistake behind a network failure either way. */
bool messenger_build_write_message_value(const char *receiver, const char *subject,
                                         const char *content, tezos_micheline_node_t *out);
