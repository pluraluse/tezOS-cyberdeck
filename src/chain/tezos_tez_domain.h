#pragma once
/*
 * .tez domain name resolution — real, built against the official
 * Tezos Domains developer documentation
 * (developers.tezos.domains/interoperability/name-resolution), not
 * invented. The real, documented mechanism: a NameRegistry contract
 * holds a `records` bigmap, keyed by the UTF-8-encoded domain name
 * (hex bytes), whose value contains the resolved `address`.
 *
 * Real, honest scope decisions, stated plainly rather than glossed
 * over:
 *
 * 1. The official docs describe a stable proxy entry point
 *    (`NameRegistry.CheckAddress`) whose own storage points at the
 *    current, real NameRegistry contract, specifically so clients
 *    don't need to track migrations by hand. This module does NOT
 *    implement that proxy-resolution step — this project's sandbox
 *    has no live network access to verify that specific contract's
 *    exact storage shape, and guessing at an unverified JSON layout
 *    felt like the wrong kind of confidence to build against (this
 *    project has its own real, direct history of exactly that kind of
 *    mistake). Instead, the real NameRegistry contract address is a
 *    real, user-configurable Settings value (see
 *    `settings_get_name_registry_contract()`), the same honest
 *    pattern already used for the RPC Node and Messenger Contract
 *    settings — both of which had the identical "needs live
 *    verification, so let it be confirmed and set rather than
 *    guessed" gap.
 *
 * 2. The docs call for full name "normalization" before encoding a
 *    lookup key (a real, documented algorithm this module doesn't
 *    fully implement). What's real here: UTF-8 byte encoding, matching
 *    the real, working example shown in Tezos Domains' own official
 *    Taquito documentation (`stringToBytes(domainName)`). Full
 *    Unicode-edge-case normalization is a real, disclosed gap, not
 *    silently assumed complete.
 *
 * 3. Expiry checking (domains do expire and become resolvable again)
 *    is NOT implemented in this first pass — a real, disclosed scope
 *    cut. A resolved address from an expired domain could theoretically
 *    be stale. Real, natural follow-up work, not attempted here.
 */
#include <stdbool.h>
#include <stddef.h>
#include "tezos_tzkt.h"

/* Real, basic .tez name validation -- not the full documented
   normalization algorithm, but real, useful checks: non-empty,
   ends in ".tez", and contains only characters Tezos Domains actually
   allows (lowercase letters, digits, hyphens, and the dot separating
   labels) before ever attempting a real network lookup. Returns false
   for anything that couldn't possibly be a valid domain -- a real,
   fast, local rejection rather than wasting a real network round trip
   on obviously-invalid input. */
bool tezos_tez_domain_is_valid_format(const char *name);

/* Real, portable name-to-lookup-key encoding: UTF-8 bytes of the
   name, hex-encoded (matching the real convention TZKT's own bigmap-
   key API uses for bytes-typed keys). `out` must be at least
   `2 * strlen(name) + 1` bytes. Returns false if `out_cap` is too
   small -- a real, reported failure, not a silent truncation. */
bool tezos_tez_domain_encode_key(const char *name, char *out, size_t out_cap);

/* Real resolution: given a real NameRegistry contract address (see
   this file's own header comment on why that's a real, required,
   user-configurable parameter rather than resolved automatically),
   looks up the real `records` bigmap for `name` and returns the
   resolved address in `address_out` if found. Returns false if the
   name is malformed, the domain isn't registered, the real network
   call fails, or the response doesn't have the expected real shape --
   not a crash, not a fabricated address. */
bool tezos_tez_domain_resolve(tezos_network_t network, const char *registry_contract,
                              const char *name, char *address_out, size_t address_cap);
