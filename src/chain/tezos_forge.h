#pragma once
/*
 * Tezos operation forging — building the raw binary bytes for an
 * operation (as sent to /injection/operation after signing).
 *
 * Field ordering, tag bytes, and zarith encoding verified directly
 * against Taquito's reference implementation
 * (@taquito/local-forging: schema/operation.ts, constants.ts, codec.ts)
 * — not reconstructed from memory. See
 * docs/build-notes/wallet-app-build-notes.md for specifics.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* A growable byte buffer — forging appends fields one at a time, and
   the final length isn't known until everything's been written. Not on
   the render hot path (this runs once per user-initiated operation, not
   per-frame), so a heap-based growable buffer is fine here — a
   different memory-model concern than tezos_core's screen/input loop. */
typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} tezos_bytes_t;

void tezos_bytes_init(tezos_bytes_t *b);
void tezos_bytes_free(tezos_bytes_t *b);
void tezos_bytes_append(tezos_bytes_t *b, const uint8_t *data, size_t len);
void tezos_bytes_append_byte(tezos_bytes_t *b, uint8_t byte);

/* Confirmed tag values (Taquito constants.ts kindMapping) */
#define TEZOS_TAG_REVEAL      0x6b
#define TEZOS_TAG_TRANSACTION 0x6c
#define TEZOS_TAG_ORIGINATION 0x6d
#define TEZOS_TAG_DELEGATION  0x6e

typedef struct {
    char source[40];      /* tz1/tz2/tz3 address string */
    char fee[32];          /* decimal string, e.g. "374" (mutez) */
    char counter[32];      /* decimal string */
    char gas_limit[32];    /* decimal string */
    char storage_limit[32];/* decimal string */
    char amount[32];       /* decimal string (mutez) */
    char destination[40];  /* tz1/tz2/tz3/KT1 address string */
    /* parameters (entrypoint calls) not yet supported — see
       docs/build-notes/wallet-app-build-notes.md open items.
       This covers a plain tez transfer only, which is what Wallet's
       Send flow needs first. */
} tezos_transaction_params_t;

typedef struct {
    char source[40];
    char fee[32];
    char counter[32];
    char gas_limit[32];
    char storage_limit[32];
    char public_key[64];  /* edpk... string */
} tezos_reveal_params_t;

typedef struct {
    char source[40];
    char fee[32];
    char counter[32];
    char gas_limit[32];
    char storage_limit[32];
    char delegate[40];    /* tz1/tz2/tz3 address, or empty string = remove delegation */
} tezos_delegation_params_t;

/* Appends a zarith-encoded (variable-length base-128) unsigned integer,
   given as a decimal string (amounts/counters can exceed 64-bit range
   in principle, though not in practice for tez amounts — decimal string
   input avoids needing bignum arithmetic for the common case while
   staying correct for the full range). */
bool tezos_forge_zarith(tezos_bytes_t *out, const char *decimal_str);

/* Appends a 22-byte ADDRESS encoding (implicit-or-originated tag +
   curve-tag-or-hash+pad) — used for the `destination` field. */
bool tezos_forge_address(tezos_bytes_t *out, const char *address_str);

/* Appends a 21-byte PKH encoding (curve tag + 20-byte hash, no leading
   address-tag byte) — used for `source` and `delegate` fields. */
bool tezos_forge_pkh(tezos_bytes_t *out, const char *address_str);

bool tezos_forge_reveal(tezos_bytes_t *out, const tezos_reveal_params_t *p);
bool tezos_forge_transaction(tezos_bytes_t *out, const tezos_transaction_params_t *p);
bool tezos_forge_delegation(tezos_bytes_t *out, const tezos_delegation_params_t *p);

/* Forges a full operation group: branch (32-byte block hash, decoded
   from its base58 string) + one or more already-forged contents
   (concatenated, e.g. from tezos_forge_transaction above). Caller
   assembles contents in order and passes the concatenated bytes. */
bool tezos_forge_operation_group(tezos_bytes_t *out, const char *branch_b58,
                                  const uint8_t *contents, size_t contents_len);

/* Renders a tezos_bytes_t as a lowercase hex string — useful for
   logging/display and for handing forged bytes to the signer over the
   serial link as ASCII hex rather than raw binary. */
bool tezos_bytes_to_hex(const tezos_bytes_t *b, char *out, size_t out_cap);
