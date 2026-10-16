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

/* Exposed specifically for direct testing (see
   tests/test_wallet_tez_domain.c) -- pushes the real recipient-entry
   screen with pre-set T9 text, bypassing the need to simulate exact
   multi-tap key sequences to produce specific text. NOT for
   production use. */
tezos_screen_t *wallet_test_inject_recipient_text(const char *text);

/* Replaces the wallet's active signer -- what the recovery-phrase import
   screen calls after a successful import, so Wallet actually signs with
   the imported identity from then on, not the hardcoded dev test seed. */
void wallet_set_active_signer(const tezos_mock_signer_t *signer);

/* Reads the currently-active signer's address. Not just a test hook --
   other apps (Messenger's profile display, Beacon pairing) will
   eventually want to show the active address too. Returns false if no
   signer is active yet at all (shouldn't happen in practice since
   ensure_signer()'s dev fallback always runs unless the locked gate is
   showing, but a real API shouldn't just assume). */
bool wallet_get_active_address(char *out, size_t cap);

/* The real public key (edpk...), not the address (tz1...) -- see
   wallet.c for why these were being conflated and why that was wrong.
   Same "not active yet" convention as wallet_get_active_address. */
bool wallet_get_active_public_key(char *out, size_t cap);

/* Signs `forged_bytes` (appends the signature in place, same convention
   as tezos_mock_signer_sign) using whichever signer is currently
   active -- the dev test seed, or a real imported/unlocked one. This is
   what lets OTHER apps (Scanner) sign an operation without needing
   their own signer instance or exposing wallet.c's private signer
   struct directly -- only "sign this" and "what's my address" are
   public, not the raw key material. Returns false if no signer is
   active yet at all (matches wallet_get_active_address's convention). */
bool wallet_sign_operation(tezos_bytes_t *forged_bytes, char *sig_out, size_t sig_cap);

/* Sets the active signer AND remembers the mnemonic/passphrase for the
   rest of this session -- required for wallet_switch_account() below
   to be able to re-derive a different account index without asking
   for the PIN again mid-session. Callers with real seed material
   (import_recovery.c, wallet.c's own unlock flow) should call this
   instead of wallet_set_active_signer() directly. */
bool wallet_set_active_signer_from_mnemonic(const char *mnemonic, int word_count,
                                            const char *passphrase, uint32_t account);

/* Re-derives the active signer at a different account index, using the
   mnemonic remembered by wallet_set_active_signer_from_mnemonic()
   above. Returns false if no mnemonic is remembered this session (the
   dev test seed case, or before any real import/unlock happened) --
   callers should treat that as "account switching isn't available
   right now", not attempt it silently. */
bool wallet_switch_account(uint32_t account);

/* Whether real seed material (not just the dev test seed) is currently
   remembered this session -- used by Settings' Profiles screen to
   decide whether to offer account switching at all. */
bool wallet_has_session_mnemonic(void);

/* Derives the address for `account` WITHOUT changing the active
   signer -- purely for display (Settings' Profiles list showing an
   address preview per account). Returns false if no session mnemonic
   is remembered (matches wallet_switch_account's convention). */
bool wallet_derive_account_address(uint32_t account, char *out, size_t cap);

/* Exposed specifically for direct testing (see tests/test_wallet_send.c)
   -- real money-conversion logic (tez decimal string -> mutez integer
   string) deserves direct unit verification, not just trust via a UI
   flow. Converts e.g. "1.5" -> "1500000". Returns false (leaving
   mutez_out unchanged) for anything that isn't a clean, at-most-6-
   decimal-place tez amount -- see the implementation's own comment for
   why silent rounding/truncation isn't acceptable here. */
bool wallet_tez_to_mutez(const char *tez_str, char *mutez_out, size_t mutez_cap);
