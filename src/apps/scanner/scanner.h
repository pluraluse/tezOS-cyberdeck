#pragma once
/*
 * Scanner — "generic entrypoint decode/fill/forge" (the one line this
 * app has had as its spec since the original app-suite decision). No
 * further detail was ever specified beyond that, so this is a real,
 * reasoned design rather than a guess filling in an existing blueprint.
 *
 * WHY QR-BASED INPUT: Tezos contract addresses are 36-character mixed-
 * case Base58 strings -- typing one via T9 would be genuinely painful,
 * and Scanner's whole reason to exist is calling entrypoints on
 * contracts a user didn't type in themselves. Real-world usage looks
 * like "a dApp's website shows a QR code, scan it to fill in the call"
 * -- so this reuses the same real zbar/libqrencode pipeline already
 * verified for Beacon pairing and Camera's Scan mode.
 *
 * DELIBERATELY NOT Beacon: Beacon is a paired, ongoing dApp<->wallet
 * session (see docs/build-notes/beacon-build-notes.md). Scanner is a
 * one-off, unpaired "scan this QR, review the call, sign it" flow --
 * genuinely a different use case, so it uses its own simpler JSON
 * schema for the scanned payload
 * (`{"destination":"KT1...","entrypoint":"...","value":{"int":"..."}}`),
 * not Beacon's full envelope. Scoped to single-int-parameter entrypoint
 * calls for now, matching what `tezos_micheline.h`'s primitive support
 * actually covers well (see that header's own scope note).
 *
 * HONEST LIMITATION, same shape as Camera's Scan mode: no real camera
 * driver exists here, so a "simulate scan" trigger generates a
 * realistic call-spec QR and decodes it through the real libraries,
 * standing in for a live camera feed.
 *
 * Uses `wallet_sign_operation()`/`wallet_get_active_address()` --
 * Scanner doesn't have its own signer, it signs with whatever identity
 * is currently active in Wallet (the dev seed, or a real
 * imported/unlocked one), same as the rest of this device's one-signer
 * model.
 */
#include "../../core/tezos_core.h"

tezos_screen_t *scanner_entry_screen(void);
