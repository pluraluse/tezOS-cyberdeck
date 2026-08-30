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

typedef enum {
    TEZOS_NETWORK_MAINNET,
    TEZOS_NETWORK_GHOSTNET,
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
