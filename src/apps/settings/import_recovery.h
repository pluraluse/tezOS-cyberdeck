#pragma once
/*
 * Recovery phrase import — the T9-driven screen for entering a BIP39
 * mnemonic (e.g. a Kukai seed phrase) and adopting it as this device's
 * active signer. All the crypto (BIP39 validation, SLIP-0010
 * derivation, signer construction) was already built and verified
 * separately (src/chain/tezos_bip39.c, tezos_slip10.c, tezos_signer.c)
 * -- this file is the missing piece that actually exposes it to a user.
 *
 * IMPORTANT, surfaced directly in this screen's copy, not just a code
 * comment: this device uses the standard 44'/1729'/account'/0'
 * derivation path (same as Ledger/Temple). This is now confirmed to
 * match Kukai's own convention too -- their official docs (docs.kukai.app)
 * and their own open-source kukai-crypto-swift library both explicitly
 * state path 44'/1729'/0'/0' via SLIP-0010, the exact same convention
 * and algorithm used here. Not a live-tested cross-check against a
 * running Kukai instance, but a direct confirmation from Kukai's own
 * source, not an assumption -- see docs/build-notes/wallet-app-build-notes.md.
 */
#include "../../core/tezos_core.h"

tezos_screen_t *import_recovery_entry_screen(void);
