#pragma once
/*
 * BIP39 mnemonic (recovery phrase) generation, validation, and
 * mnemonic-to-seed conversion.
 *
 * Verified against the official trezor/python-mnemonic reference
 * implementation's published test vectors (entropy -> mnemonic -> seed,
 * with the standard "TREZOR" test passphrase) — see
 * docs/build-notes/wallet-app-build-notes.md.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 12-word (128-bit entropy) is what tezOS generates by default — the
   same strength Kukai/Temple/most wallets default to. 24-word (256-bit)
   support is included since some users may want to import an existing
   24-word phrase from elsewhere. */
#define TEZOS_BIP39_ENTROPY_BYTES_12WORD 16
#define TEZOS_BIP39_ENTROPY_BYTES_24WORD 32
#define TEZOS_BIP39_MAX_MNEMONIC_LEN 256 /* 24 words * ~9 chars + spaces, generous */

/* Generates a mnemonic from raw entropy (caller supplies real randomness
   — this function does not generate entropy itself, see
   tezos_bip39_random_entropy for that). entropy_len must be 16 or 32. */
bool tezos_bip39_entropy_to_mnemonic(const uint8_t *entropy, size_t entropy_len,
                                     char *mnemonic_out, size_t out_cap);

/* Validates a mnemonic's checksum (the last word encodes a checksum over
   the preceding entropy bits — this catches most typos/transcription
   errors when a user is entering an existing phrase). word_count must
   be 12 or 24. */
bool tezos_bip39_validate(const char *mnemonic, int word_count);

/* PBKDF2-HMAC-SHA512(mnemonic, "mnemonic"+passphrase, 2048 iterations)
   -> 64-byte seed, per the BIP39 spec. Empty passphrase is standard/
   default — most wallets don't prompt for one unless the user opts in
   to the extra "25th word" security layer. */
bool tezos_bip39_to_seed(const char *mnemonic, const char *passphrase,
                         uint8_t seed_out[64]);
