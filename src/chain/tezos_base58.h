#pragma once
/*
 * Base58Check encode/decode for Tezos address/key formats.
 *
 * Prefix byte values and the encoding scheme (double-SHA256 checksum,
 * standard Bitcoin base58 alphabet) verified against the actual Taquito
 * reference implementation (@taquito/utils, encoding.ts/constants.ts) —
 * not reconstructed from memory. See docs/build-notes/wallet-app-build-notes.md
 * for the specific commit/lines this was checked against.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TEZOS_PREFIX_TZ1,        /* Ed25519PublicKeyHash    — [6,161,159], 20-byte payload */
    TEZOS_PREFIX_TZ2,        /* Secp256k1PublicKeyHash  — [6,161,161], 20-byte payload */
    TEZOS_PREFIX_TZ3,        /* P256PublicKeyHash       — [6,161,164], 20-byte payload */
    TEZOS_PREFIX_KT1,        /* ContractHash            — [2,90,121],  20-byte payload */
    TEZOS_PREFIX_EDPK,       /* Ed25519PublicKey        — [13,15,37,217], 32-byte payload */
    TEZOS_PREFIX_EDSK_SEED,  /* Ed25519Seed             — [13,15,58,7],   32-byte payload */
    TEZOS_PREFIX_EDSIG,      /* Ed25519Signature        — [9,245,205,134,18], 64-byte payload */
    TEZOS_PREFIX_SIG,        /* GenericSignature        — [4,130,43],         64-byte payload */
    TEZOS_PREFIX_BLOCK_HASH, /* BlockHash               — [1,52],             32-byte payload */
} tezos_b58_prefix_t;

/* Decodes a base58check string (e.g. "tz1gvF4cD2dDtqitL3ZTraggSR1Mju2BKFEM")
   into its raw payload bytes, verifying the checksum and that the prefix
   matches `expected`. Returns false on any failure (bad checksum, wrong
   prefix, malformed input) — never partially fills `out` on failure. */
bool tezos_b58_decode(const char *input, tezos_b58_prefix_t expected,
                      uint8_t *out, size_t *out_len, size_t out_cap);

/* Encodes raw payload bytes into a base58check string with the given
   prefix. `out` must be large enough (caller's responsibility — see
   TEZOS_B58_MAX_STRLEN below for a safe upper bound). */
bool tezos_b58_encode(const uint8_t *payload, size_t payload_len,
                      tezos_b58_prefix_t prefix, char *out, size_t out_cap);

#define TEZOS_B58_MAX_STRLEN 128
