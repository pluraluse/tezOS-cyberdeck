#pragma once
/*
 * SLIP-0010 hierarchical deterministic key derivation, ed25519 only.
 *
 * ed25519 can't do the plain BIP32 non-hardened derivation trick (no
 * public-key-only child derivation exists for this curve) — every
 * derivation step here uses the "hardened" formula unconditionally,
 * per the SLIP-0010 spec. This is why the project's derivation path
 * decision (44'/1729'/account'/0', ALL segments hardened) is what it is.
 *
 * Verified against the official satoshilabs/slips reference algorithm
 * (slip-0010/testvectors.py) — both the master-key derivation and child
 * derivation steps produce byte-identical output to the reference
 * implementation for its own published test vector, cross-confirmed
 * against an independent third-party implementation's documented
 * output too. See docs/build-notes/wallet-app-build-notes.md.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint8_t key[32];        /* the derived key at this node -- IS the
                                ed25519 seed once you're at a leaf,
                                feed directly into tezos_mock_signer_from_seed */
    uint8_t chain_code[32];
} tezos_slip10_node_t;

/* Derives the master node from a BIP39 seed (or any 64-byte seed).
   HMAC-SHA512 with key "ed25519 seed" -- this is the ed25519-specific
   modifier, different from BIP32's "Bitcoin seed". */
void tezos_slip10_master(const uint8_t seed[64], tezos_slip10_node_t *out);

/* Derives one hardened child. `index` should NOT have the hardening bit
   pre-set -- this function sets it unconditionally, since ed25519 has
   no non-hardened path. Pass 44, 1729, 0, 0 in sequence for the
   project's standard 44'/1729'/account'/0' path, not raw 0x8000002c etc. */
void tezos_slip10_derive_hardened(const tezos_slip10_node_t *parent, uint32_t index,
                                  tezos_slip10_node_t *out);

/* Convenience: derives the full standard Tezos path
   44'/1729'/account'/0' from a BIP39 seed in one call. */
void tezos_slip10_derive_tezos_path(const uint8_t seed[64], uint32_t account,
                                    tezos_slip10_node_t *out);
