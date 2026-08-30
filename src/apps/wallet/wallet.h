#pragma once
/*
 * Wallet app — the first real app screen (beyond Idle) built on the
 * core engine, and the first to exercise the real forge/sign pipeline
 * end to end. See docs/build-notes/wallet-app-build-notes.md for what's
 * real here versus what's still a marked placeholder.
 */
#include "../../core/tezos_core.h"
#include "../../chain/tezos_signer.h"

/* Entry point for the app registry — matches the tezos_app_t.entry
   function pointer signature used throughout src/shim-linux and
   src/core/demo_idle_screen.c. */
tezos_screen_t *wallet_entry_screen(void);

/* Replaces the wallet's active signer -- what the recovery-phrase import
   screen calls after a successful import, so Wallet actually signs with
   the imported identity from then on, not the hardcoded dev test seed. */
void wallet_set_active_signer(const tezos_mock_signer_t *signer);
