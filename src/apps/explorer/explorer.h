#pragma once
/*
 * Explorer — real address/contract lookup, activity signals, and
 * holdings, for the actual stated use case: recovering your own
 * assets from a project whose website has gone dark. You don't need a
 * QR code from a site that no longer exists — you need the contract
 * address, which you already have, and a way to see what's really
 * there and call a real entrypoint on it directly.
 *
 * Consolidates what used to be a separate "Scanner" app concept —
 * QR-based input is a real, useful secondary path (kept, not
 * discarded) but address entry is the PRIMARY flow here, matching the
 * actual use case rather than assuming a live QR source always exists.
 *
 * Built on three real, tested TZKT functions: tezos_tzkt_get_contract_info
 * (activity/staleness signals), tezos_tzkt_get_entrypoints_with_types
 * (the shared dynamic engine's data source), and
 * tezos_tzkt_get_owned_tokens (what a contract itself holds).
 *
 * IMPORTANT, deliberate scope note (see docs/SECURITY-FINDINGS.md and
 * docs/build-notes/explorer-app-build-notes.md): this app shows real,
 * public information and lets you call entrypoints a contract already
 * exposes -- the same category of thing any block explorer or wallet
 * does. It does NOT attempt any vulnerability/bug detection, and never
 * will -- "is this contract inactive" (a real, disclosed activity
 * signal) is a completely different question from "is this contract
 * exploitable," and this app only ever answers the former.
 *
 * NOT YET BUILT: calling an entrypoint with real, dynamically-prompted
 * parameters (walking an arbitrary type tree via T9, the way Scanner's
 * single-int-only flow doesn't). This is real, substantial follow-up
 * work — see the build notes for the concrete plan. This version shows
 * real entrypoint names/types and real holdings, but doesn't yet let
 * you actually call one with a real value beyond a placeholder.
 */
#include "../../core/tezos_core.h"

tezos_screen_t *explorer_entry_screen(void);

/* Exposed specifically for direct testing (see tests/test_explorer_call.c)
   -- injects a single real entrypoint (name + a real parsed Micheline
   type) directly into the Entrypoints list state and pushes the real
   Explorer screen already there, so the full call flow (walking the
   type via T9, confirming, forging/signing/broadcasting) can be
   exercised without needing a live TZKT fetch to discover a real
   entrypoint first. NOT for production use. */
tezos_screen_t *explorer_test_inject_entrypoint(const char *address, const char *entrypoint_name,
                                                const char *type_json);
