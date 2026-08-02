#pragma once
/*
 * tezOS signer — key derivation and signing logic.
 *
 * This file implements the SOFTWARE MOCK signer only: a local ed25519
 * keypair derived from a seed, used to prove the forge->sign->verify
 * pipeline works end to end before the real STM32/Pico signer hardware
 * exists. See docs/build-notes/wallet-app-build-notes.md.
 *
 * *** NEVER use tezos_mock_signer_* with a seed protecting real funds. ***
 * The whole point of the real signer hardware is that the seed never
 * touches the main board's Linux environment — this mock intentionally
 * violates that for dev/test purposes only.
 *
 * Signing logic (watermark, hashing, curve) verified against Taquito's
 * reference implementation and its own test vectors — see
 * docs/build-notes/wallet-app-build-notes.md for specifics.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "tezos_forge.h"

typedef struct {
    uint8_t seed[32];       /* Ed25519 seed — this is the actual secret */
    uint8_t public_key[32]; /* derived, cached */
    uint8_t secret_key[64]; /* derived (libsodium's expanded form), cached */
} tezos_mock_signer_t;

/* Decodes an edsk... seed string (TEZOS_PREFIX_EDSK_SEED) and derives
   the keypair. Returns false on a malformed seed string. */
bool tezos_mock_signer_init(tezos_mock_signer_t *signer, const char *edsk_seed_b58);

/* Generates a fresh random seed and derives its keypair — for spinning
   up a throwaway test identity rather than using a known/fixed one. */
void tezos_mock_signer_generate(tezos_mock_signer_t *signer);

/* Encodes this signer's public key as an "edpk..." string. */
bool tezos_mock_signer_public_key_b58(const tezos_mock_signer_t *signer, char *out, size_t out_cap);

/* Encodes this signer's public key hash as a "tz1..." string (the
   address this signer controls). */
bool tezos_mock_signer_address_b58(const tezos_mock_signer_t *signer, char *out, size_t out_cap);

/* Signs already-forged operation bytes: prepends the generic-operation
   watermark (0x03), blake2b-256 hashes the result, ed25519-signs that
   hash. Appends the raw 64-byte signature to `forged_bytes` in place
   (producing the final "signed operation" bytes ready to inject) AND
   separately returns the signature encoded as a "sig..." string. */
bool tezos_mock_signer_sign(const tezos_mock_signer_t *signer, tezos_bytes_t *forged_bytes,
                            char *sig_b58_out, size_t sig_b58_cap);

/* Self-check: verifies a signature against this signer's own public
   key. Not something a real device needs at runtime (the chain verifies
   signatures, not the wallet) — this exists purely so the dev/test
   pipeline can assert "the signature I just produced is actually valid"
   without needing network access to a real node. */
bool tezos_mock_signer_self_verify(const tezos_mock_signer_t *signer,
                                   const uint8_t *signed_bytes, size_t signed_len);
