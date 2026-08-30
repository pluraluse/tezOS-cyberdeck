#pragma once
/*
 * Micheline binary encoding — the wire format for smart contract
 * parameters (and storage/code, though only parameters are wired into
 * forging here). This is what Scanner, HEN-style minting, and
 * Messenger's post_message all actually need beyond plain tez transfers.
 *
 * Verified against Taquito's exact reference implementation
 * (taquito-local-forging/src/michelson/codec.ts) — primitive code
 * table, the signed-zarith int encoding (genuinely different from the
 * UNSIGNED zarith in tezos_forge.h — the sign bit steals a bit of
 * capacity from the first byte only, so small values coincide with
 * unsigned zarith but diverge starting around 64), and checked against
 * two of Taquito's own exact test vectors (a bytes value and a Pair of
 * two bytes values), plus an independent Python port of the signed-int
 * algorithm for cross-checking. See
 * docs/build-notes/wallet-app-build-notes.md for specifics.
 *
 * SCOPE CUT, deliberate: primitives with 0, 1, or 2 direct arguments
 * (covers Unit, Left/Right/Some/None/True/False, and Pair — the large
 * majority of real entrypoint parameters) plus a single optional
 * annotation. The generic 3+-arg primitive tag (0x09 in the prim
 * preamble, distinct from the 0x09 = 'Some' data-constructor code —
 * context disambiguates) is NOT implemented; nested 2-arg Pairs cover
 * most real N-ary cases anyway (e.g. Pair(a, Pair(b, c)) instead of a
 * flat 3-ary Pair).
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "tezos_forge.h" /* for tezos_bytes_t */

typedef enum {
    MICHELINE_INT,
    MICHELINE_STRING,
    MICHELINE_BYTES,
    MICHELINE_PRIM,
    MICHELINE_SEQUENCE,
} tezos_micheline_kind_t;

typedef struct tezos_micheline_node {
    tezos_micheline_kind_t kind;

    const char *int_decimal;   /* MICHELINE_INT: decimal string, may be negative */
    const char *string_value;  /* MICHELINE_STRING */
    const uint8_t *bytes_data; /* MICHELINE_BYTES */
    size_t bytes_len;

    const char *prim_name;     /* MICHELINE_PRIM: e.g. "Pair", "Left", "Unit" */
    const struct tezos_micheline_node *args[2];
    int arg_count;              /* 0, 1, or 2 -- see scope note above */
    const char *annot;          /* single optional annotation (e.g. "%transfer"), or NULL */

    const struct tezos_micheline_node *const *seq_items; /* MICHELINE_SEQUENCE */
    int seq_count;
} tezos_micheline_node_t;

/* Constructors -- nodes are plain caller-owned structs (stack or static),
   linked by pointer. No allocation happens inside this module, matching
   the rest of the chain layer's ownership style. */
tezos_micheline_node_t tezos_micheline_int(const char *decimal);
tezos_micheline_node_t tezos_micheline_string(const char *s);
tezos_micheline_node_t tezos_micheline_bytes(const uint8_t *data, size_t len);
tezos_micheline_node_t tezos_micheline_prim0(const char *name);
tezos_micheline_node_t tezos_micheline_prim1(const char *name, const tezos_micheline_node_t *arg);
tezos_micheline_node_t tezos_micheline_prim2(const char *name, const tezos_micheline_node_t *a,
                                             const tezos_micheline_node_t *b);
tezos_micheline_node_t tezos_micheline_sequence(const tezos_micheline_node_t *const *items, int count);

/* Convenience shortcuts for the most common data constructors. */
tezos_micheline_node_t tezos_micheline_unit(void);
tezos_micheline_node_t tezos_micheline_pair(const tezos_micheline_node_t *a, const tezos_micheline_node_t *b);

/* Recursively encodes a Micheline value tree into its binary wire
   format, appending to `out`. */
bool tezos_micheline_encode(const tezos_micheline_node_t *node, tezos_bytes_t *out);

/* Encodes the full operation `parameters` field (entrypoint + value),
   per Taquito's parametersEncoder: '00' shortcut if entrypoint is
   "default" (or NULL) and value is Unit; otherwise 'ff' + entrypoint
   encoding + 4-byte length + Micheline-encoded value. Pass NULL
   entrypoint and NULL value for a plain transfer (no parameters) --
   this is what tezos_forge_transaction already does when
   tezos_transaction_params_t.entrypoint is unset, so existing callers
   are unaffected. */
bool tezos_forge_parameters(tezos_bytes_t *out, const char *entrypoint,
                            const tezos_micheline_node_t *value);
