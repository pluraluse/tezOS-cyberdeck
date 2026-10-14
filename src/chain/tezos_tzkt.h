#pragma once
/*
 * TZKT indexer client — the read side of the network layer (account
 * balance/counter for Wallet, branch hash for forging). Broadcasting a
 * signed operation is a SEPARATE concern (goes through a Tezos RPC
 * node's /injection/operation, not TZKT, which is a read-only indexer)
 * and isn't implemented here yet.
 *
 * Endpoint field names (balance, counter, hash) verified against real,
 * live TZKT API responses fetched directly — not guessed from secondary
 * documentation. See docs/build-notes/wallet-app-build-notes.md for the
 * captured fixtures this was checked against.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "tezos_micheline.h"

typedef enum {
    TEZOS_NETWORK_MAINNET,
    TEZOS_NETWORK_GHOSTNET,
    /* Teia's own testnet -- real, confirmed URL pattern
       (api.shadownet.tzkt.io) found via the official Tezos docs
       (docs.tezos.com/developing/information/indexers), which
       explicitly documents this exact hostname-substitution pattern
       for querying any TZKT-supported network. Relevant here
       specifically because this project's own earlier research
       (docs/build-notes/messenger-app-build-notes.md) already listed
       real Shadownet addresses for Teia contracts (Poll Comments), and
       Messenger's contract is a Teia contract -- Shadownet is a real,
       relevant place to test against, not an arbitrary addition. */
    TEZOS_NETWORK_SHADOWNET,
} tezos_network_t;

const char *tezos_tzkt_base_url(tezos_network_t network);

/* GET /v1/head, extracts the "hash" field -- this is what
   tezos_forge_operation_group's branch parameter needs. */
bool tezos_tzkt_get_head_hash(tezos_network_t network, char *hash_out, size_t hash_cap);

/* GET /v1/accounts/{address}, extracts "balance" and "counter" as
   decimal strings (matching the rest of the chain layer's convention of
   passing amounts/counters as decimal strings, not fixed-width ints --
   see tezos_forge.h). A brand new, never-used account has no counter
   field in the response at all (TZKT omits it) -- this is handled by
   returning "0" in that case, which is what a fresh account's real
   counter actually should be treated as for forging purposes. */
bool tezos_tzkt_get_account(tezos_network_t network, const char *address,
                            char *balance_out, size_t balance_cap,
                            char *counter_out, size_t counter_cap);

/* GET /v1/contracts/{address}/entrypoints, extracts just the entrypoint
   NAMES (e.g. "transfer", "mint", "collect") into a caller-provided
   array. Real, confirmed-current endpoint (multiple independent
   sources: the official TZKT SDK, a generated PHP client, TZKT's own
   v1.4 release notes) -- but UNLIKE tezos_tzkt_get_head_hash/
   tezos_tzkt_get_account above, this one's exact JSON response shape
   was NOT verified against a live captured response. Available
   documentation confirmed the endpoint, its query parameters, and that
   each entrypoint has a "name" field, but a literal fetched example
   response wasn't obtainable this session. Scoped deliberately
   narrow because of that gap: this extracts entrypoint names only,
   not full parameter-type schemas, which is the part the available
   documentation was most consistently specific about across sources.
   Parsing is defensive -- an entry missing a "name" field is skipped,
   not treated as a parse failure, and a genuinely malformed/unexpected
   response shape fails cleanly (returns false) rather than crashing or
   returning garbage. See docs/build-notes/scanner-app-build-notes.md
   for the honest verification-gap writeup, same discipline as the
   prior-art check that WiFi got right and Scanner's original QR schema
   and the signer wire protocol didn't. */
bool tezos_tzkt_get_contract_entrypoints(tezos_network_t network, const char *address,
                                         char names_out[][32], int max_names, int *count_out);

/* A single discovered entrypoint: its real name, plus its parameter
   type parsed into the bounded tree tezos_micheline.h's dynamic engine
   understands (see that header's "Dynamic entrypoint calling"
   section). `type_root` is -1 if the type couldn't be determined --
   either the real response genuinely omitted it, or (see the honest
   note below) this function's guess at where to find it in the
   response didn't match reality. -1 is a normal, expected outcome to
   handle, not a bug signal. */
typedef struct {
    char name[32];
    tezos_micheline_type_pool_t type_pool;
    int type_root;
} tezos_tzkt_entrypoint_t;

/* GET /v1/contracts/{address}/entrypoints?micheline=true -- same real,
   confirmed-current endpoint as tezos_tzkt_get_contract_entrypoints
   above, now also requesting each entrypoint's parameter type in raw
   Micheline format (not TZKT's own human-readable JSON schema
   convention -- see tezos_micheline.h's design note on why).

   HONEST GAP, larger than the one on the name-only function above:
   confirmed the query parameter itself is real (`micheline` is one of
   `contractsGetEntrypoints`'s real documented parameters, alongside
   `json` and `michelson`), but the exact JSON FIELD NAME the response
   uses to carry that Micheline type data was never confirmed against a
   live captured response -- this function tries a small set of
   plausible field names ("michelineParameters", then "parameters") in
   that order, and if neither matches, still succeeds at fetching names
   (falls back to the name-only behavior) with type_root left at -1 for
   every entry, rather than failing the whole call. This is a real,
   larger guess than the name-only field ("name" was confirmed across
   multiple independent sources); treat any type_root >= 0 result as
   worth double-checking against a real device with network access
   before trusting it for anything beyond development. */
bool tezos_tzkt_get_entrypoints_with_types(tezos_network_t network, const char *address,
                                           tezos_tzkt_entrypoint_t *entrypoints_out,
                                           int max_entrypoints, int *count_out);

/* GET /v1/tokens/balances?account={address}&token.contract={contract} --
   real, confirmed-current endpoint (multiple independent sources: the
   official Tezos docs, TzKT's own README, a Baking Bad migration
   guide), and this session ALSO captured a real, live example
   response for a real address holding many real tokens across many
   real contracts -- the exact field names below
   (account.address/token.tokenId/token.contract.address/balance) are
   confirmed against that live capture, not guessed from documentation
   alone, matching the account/head-hash endpoints' verification level
   rather than the lower-confidence entrypoint-type-fetching function
   above.

   Extracts only token IDs with a CURRENTLY NON-ZERO balance (the
   real, live example showed many zero-balance historical entries --
   e.g. tokens once held and since transferred away -- which a naive
   "just list every entry" approach would wrongly include). Used by
   Messenger's Inbox: each currently-held token ID on the confirmed
   messenger contract is believed to correspond to a received message
   ID (see docs/build-notes/messenger-app-build-notes.md) -- a real,
   load-bearing assumption from reading the storage shape, not yet
   independently confirmed by finding an actual message this way. */
bool tezos_tzkt_get_token_balances(tezos_network_t network, const char *account,
                                   const char *token_contract,
                                   char token_ids_out[][32], int max_ids, int *count_out);

/* A single message, as read back from the confirmed real
   `message_infos` big_map. Scoped to the preview fields most useful
   for an inbox list (sender/subject/content/timestamp) -- NOT the
   full 12-field record (cc/bcc/attachments/parent_message_id/
   tgc_fa2_contract/tgc_token_id are not extracted here; a full
   message-detail view is real follow-up work, not attempted yet). */
typedef struct {
    char sender[64];
    char subject[64];
    char content[256];
    char timestamp[32]; /* ISO-8601 string, TZKT's own normalized form */
} tezos_tzkt_message_t;

/* GET /v1/contracts/{address}/bigmaps/{name}/keys/{key} -- real,
   confirmed-current endpoint (multiple independent sources: TzKT's own
   README, the official SDK, a generated PHP client), and this session
   ALSO captured a real, live structural example (a different
   contract's `ledger` big_map at a historical level) confirming the
   real response shape: a `key`/`value` pair per entry, `value` being
   TZKT's human-readable JSON matching the big_map's actual value type
   -- NOT captured specifically for `message_infos` on the real
   Messenger contract, which is the honest gap here: the *shape*
   (key/value pair, value as a plain object) is confirmed, but the
   *exact* field names inside `message_infos`'s value were taken from
   the real storage SCHEMA captured earlier
   (sender/subject/content/timestamp, among others), not from a live
   VALUE example. Treat any successful result as worth double-checking
   against a real device before trusting it fully.

   `message_id` is passed as a decimal string (the big_map key). Fails
   cleanly (returns false) if the key doesn't exist (no such message)
   or the response doesn't match the expected shape -- not a crash,
   not fabricated content. */
bool tezos_tzkt_get_message(tezos_network_t network, const char *contract_address,
                            const char *message_id, tezos_tzkt_message_t *message_out);

/* A single owned token, for Gallery -- richer than the plain token ID
   tezos_tzkt_get_token_balances extracts for Messenger's narrower
   need (that one is scoped to "does this ID exist," filtered to one
   known contract; this one is for genuinely browsing what an address
   owns across every contract). */
typedef struct {
    char contract_address[40];
    char contract_alias[32];  /* may be empty -- not every contract has one */
    char token_id[32];
    char name[64];            /* from token.metadata.name -- may be empty */
    char balance[32];
    char image_uri[200];      /* from token.metadata.displayUri, falling back to
        thumbnailUri then artifactUri -- real, common TZIP-12/16 metadata fields
        for a token's real image reference (usually an ipfs:// URI, sometimes a
        direct http(s) URL). May be empty -- not every real token populates any
        of these. See tezos_resolve_ipfs_uri() (tezos_image_decode.h) for turning
        this into a real, fetchable URL. */
} tezos_tzkt_owned_token_t;

/* GET /v1/tokens/balances?account={address} -- same real, confirmed-
   current endpoint as tezos_tzkt_get_token_balances above (this
   session's live capture already confirmed contract.alias/
   contract.address/tokenId/balance field names; metadata.name uses
   the same live capture, which showed real, populated name fields for
   several real tokens, e.g. "hic et nunc NFTs"). Deliberately NOT
   filtered to a single contract -- Gallery's whole point is showing
   everything an address holds, unlike Messenger's narrower need.
   Same zero-balance-exclusion behavior as the Messenger-specific
   function, same real reasoning (historical entries for tokens once
   held and since transferred away). */
bool tezos_tzkt_get_owned_tokens(tezos_network_t network, const char *account,
                                 tezos_tzkt_owned_token_t *tokens_out, int max_tokens, int *count_out);

/* Real activity/staleness signals for Explorer's "is this contract
   actually dead" judgment -- NOT a vulnerability scanner, just the
   same public info any block explorer shows (balance, transaction
   count, alias, last activity timestamp). Deliberately does not
   attempt any bug/exploit detection -- see
   docs/SECURITY-FINDINGS.md's ground rules for why that's kept
   entirely out of this codebase. */
typedef struct {
    char alias[64];         /* may be empty -- not every contract has one */
    char balance[32];       /* raw mutez, as a decimal string */
    int num_transactions;
    char first_activity_time[32];  /* ISO-8601, empty if never seen */
    char last_activity_time[32];   /* ISO-8601, empty if never seen --
        the real staleness signal: a contract whose last activity is
        long past is a real candidate for "abandoned," though this
        function only reports the fact, it doesn't judge it */
} tezos_tzkt_contract_info_t;

/* GET /v1/contracts/{address} -- real, confirmed-current endpoint,
   and this session ALREADY captured a real, live example of exactly
   this response shape (fetched directly for the real Liquidity Baking
   contract while researching Scanner's entrypoint-discovery feature
   earlier this session) -- these exact field names
   (alias/balance/numTransactions/firstActivityTime/lastActivityTime)
   are confirmed from that live capture, not guessed from
   documentation, matching the higher verification tier this project
   reserves for endpoints with an actual captured example. Fails
   cleanly (returns false) if the address doesn't exist or isn't a
   contract at all -- not a crash, not fabricated info. */
bool tezos_tzkt_get_contract_info(tezos_network_t network, const char *address,
                                  tezos_tzkt_contract_info_t *info_out);
