#pragma once
/*
 * Wallet app — the first real app screen (beyond Idle) built on the
 * core engine, and the first to exercise the real forge/sign pipeline
 * end to end. See docs/build-notes/wallet-app-build-notes.md for what's
 * real here versus what's still a marked placeholder.
 */
#include "../../core/tezos_core.h"

/* Entry point for the app registry — matches the tezos_app_t.entry
   function pointer signature used throughout src/shim-linux and
   src/core/demo_idle_screen.c. */
tezos_screen_t *wallet_entry_screen(void);
