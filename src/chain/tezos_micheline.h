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

/* ============================================================
   Dynamic entrypoint calling -- the "fully interact with a contract
   without going to the website" capability shared by Scanner and
   Messenger. Given a contract's real entrypoint parameter TYPE
   (fetched via tezos_tzkt_get_entrypoint_type() in tezos_tzkt.h, in
   the STANDARD Micheline type JSON grammar -- not TZKT's own
   human-readable schema convention -- since confirmed correct against
   a real captured live response for the actual Liquidity Baking
   contract, not just a documented endpoint description; the raw
   Micheline type grammar is a spec-level format this project already
   has real, tested code against on the value-construction side above),
   parse that type into a bounded tree, then build a matching VALUE
   tree from user-supplied leaf strings (T9-entered ints, strings,
   addresses).

   SCOPE, deliberate: covers int/nat/mutez/timestamp (decimal strings --
   timestamp uses the same signed-int wire encoding as int, per the
   Michelson spec's optimized/wire form, not the human-readable ISO-8601
   string form), key_hash (same base58 handling as address, minus the
   leading address-tag byte -- reuses tezos_forge_pkh, already verified
   for the `source`/`delegate` operation fields), string, bytes (hex
   string), address/string-typed leaves, bool, unit, pair/or/option as
   the composite wrappers matching the value-side arg_count<=2 scope
   (confirmed directly against the real Liquidity Baking contract,
   whose `addLiquidity` entrypoint is a genuine 4-field record via 3
   levels of nested pairs, exactly matching this design), and list (a
   single, uniform element type, variable length at runtime -- see
   tezos_micheline_build_list_value below, since a variable-length
   collection genuinely doesn't fit the fixed depth-first leaf model
   everything else here uses).

   Confirmed against a second real contract (QuipuSwap DEX 2.0 factory,
   32 real entrypoints, pasted verbatim from a live fetch): `list` and
   `key_hash` are both real and common enough there (list in 6+
   entrypoints, key_hash in 6) to be worth adding, which is why they're
   here. Also surfaced, and left deliberately OUT of scope: `contract`,
   `key`, `signature` (together making up the real TZIP-17
   gasless-transaction "permit" pattern), `map`, and `lambda`/
   `operation` (a real callback-function parameter on that contract's
   `swap` entrypoint). A type using something outside the supported
   list
   is NOT silently
   approximated -- parsing fails cleanly (returns false), same
   discipline as tezos_tzkt_get_contract_entrypoints's defensive
   parsing. */

typedef enum {
    TEZOS_MTYPE_INT, TEZOS_MTYPE_NAT, TEZOS_MTYPE_MUTEZ, TEZOS_MTYPE_TIMESTAMP,
    TEZOS_MTYPE_KEY_HASH,
    TEZOS_MTYPE_STRING, TEZOS_MTYPE_BYTES, TEZOS_MTYPE_ADDRESS,
    TEZOS_MTYPE_BOOL, TEZOS_MTYPE_UNIT,
    TEZOS_MTYPE_PAIR, TEZOS_MTYPE_OR, TEZOS_MTYPE_OPTION, TEZOS_MTYPE_LIST,
} tezos_micheline_type_kind_t;

/* 16 was sized against Liquidity Baking's largest real entrypoint
   (~9 nodes) -- a real, common QuipuSwap entrypoint (launch_exchange,
   a nested or/or/pair token-variant structure) needed exactly 16 nodes
   and failed right at the boundary, caught by testing against the
   real response rather than assumed sufficient. Raised with real
   headroom, not just bumped to the next failure point. */
#define TEZOS_MTYPE_MAX_NODES 48
#define TEZOS_MTYPE_MAX_ANNOT_LEN 32

typedef struct {
    tezos_micheline_type_kind_t kind;
    char annot[TEZOS_MTYPE_MAX_ANNOT_LEN]; /* field annotation e.g. "recipient", or empty */
    int arg_count;   /* 0, 1, or 2 -- matches the value-side scope */
    int args[2];     /* indices into the owning pool, or -1 */
} tezos_micheline_type_node_t;

typedef struct {
    tezos_micheline_type_node_t nodes[TEZOS_MTYPE_MAX_NODES];
    int node_count;
} tezos_micheline_type_pool_t;

/* Parses a raw Micheline type JSON expression (the standard protocol-
   level grammar: {"prim":"int"}, {"prim":"pair","args":[...]}, etc.)
   into a bounded tree. Returns the root node's index into
   pool->nodes, or -1 on failure -- malformed JSON, a type outside the
   supported list above, or exceeding TEZOS_MTYPE_MAX_NODES. */
int tezos_micheline_parse_type(const char *json_type, tezos_micheline_type_pool_t *pool);

/* Counts how many leaf values (int/nat/mutez/string/bytes/address/
   bool) a parsed type actually needs -- e.g. a Pair of two ints needs
   2, a bare int needs 1, Unit needs 0. Callers use this to know how
   many T9 prompts to show before attempting to build the value.
   For a LIST type specifically, this returns the PER-ELEMENT leaf
   count (how many leaves ONE item needs) for a list appearing at the
   ROOT of the tree being asked about -- for lists nested deeper inside
   (e.g. inside an option or pair), the leaf count of the WHOLE
   containing type still isn't a fixed number, which is exactly why
   list item counts are threaded separately below rather than folded
   into this count. */
int tezos_micheline_type_leaf_count(const tezos_micheline_type_pool_t *pool, int root_index);

/* Builds a VALUE tree matching the given TYPE tree, consuming
   `leaf_values` in the same depth-first order tezos_micheline_type_leaf_count
   walked them, and writes the resulting nodes into `value_pool`
   (caller-provided, at least as many slots as the type tree had
   nodes). Returns the root value node's index into value_pool, or -1
   on failure (a leaf string that doesn't parse for its expected kind
   -- e.g. non-numeric text for an int -- fails cleanly here, not with
   garbage encoded on-chain).

   LISTS ANYWHERE IN THE TREE: `list_item_counts` is a flat array
   giving each LIST type node's item count, in the same depth-first
   order those nodes are encountered during the walk (pass NULL and 0
   for a type with no lists at all -- the common case, and every
   existing caller of this function before this parameter existed).
   This is what makes a real, genuinely necessary shape like
   `option(list(address))` buildable -- confirmed necessary against
   the real Teia Channels contract's `write_message` entrypoint, whose
   cc/bcc/attachments fields are ALL option-wrapping-a-list, a shape
   the original list-only-at-the-root design (see
   tezos_micheline_build_list_value below) genuinely couldn't build.
   `leaf_values` still holds every scalar leaf across the WHOLE tree,
   including every list item's leaves, in depth-first order -- a list
   with N items just means N * (that list's element type's leaf count)
   worth of consecutive entries at that point in the array. */
int tezos_micheline_build_value(const tezos_micheline_type_pool_t *type_pool, int type_root,
                                const char *const *leaf_values, int leaf_count,
                                const int *list_item_counts, int list_item_counts_len,
                                tezos_micheline_node_t *value_pool, int value_pool_cap);

/* Convenience wrapper for the simple case: the type IS a list at its
   very root (not nested inside anything). Equivalent to calling
   tezos_micheline_build_value with a single-entry list_item_counts
   array containing `item_count`. `element_leaves` holds all items'
   leaves concatenated (item 0's, then item 1's, ...), each consuming
   exactly tezos_micheline_type_leaf_count(type_pool, the list's
   element type) leaves. `item_count` is how many items -- 0 (an empty
   list) is valid. Returns the built MICHELINE_SEQUENCE node's index
   into value_pool, or -1 on failure. */
#define TEZOS_MICHELINE_MAX_LIST_ITEMS 16
int tezos_micheline_build_list_value(const tezos_micheline_type_pool_t *type_pool, int list_type_root,
                                     const char *const *element_leaves, int total_leaf_count, int item_count,
                                     tezos_micheline_node_t *value_pool, int value_pool_cap);

/* ============================================================
   Interactive type walker -- converts recursively walking an arbitrary
   type tree into an explicit, pausable state machine, so a caller can
   prompt for one leaf value (or one list's item count) at a time via
   real UI input, then resume, rather than needing every value up
   front. This is the piece that turns "the dynamic engine can build a
   value if you already have all the leaves" into "a UI can actually
   walk an unknown contract's entrypoint and ask the right questions in
   the right order" -- the single largest remaining UI-engineering gap
   this project had, closed here at the engine level so any app
   (Explorer first, potentially others later) can drive it.

   Processing order matches tezos_micheline_build_value's own
   depth-first walk exactly: PAIR visits args[0] then args[1]; OPTION
   and OR always take their "Some"/"Left" branch (matching this
   project's existing documented scope -- choosing the other branch,
   or omitting an optional field, is real UI-decision work for a future
   pass, not attempted here); LIST asks for an item count first, then
   walks that many copies of its element type; UNIT and every other
   leaf kind produce exactly one step each (UNIT needs no user input at
   all and is skipped internally, matching build_value's own 0-leaf
   contract for it). */

typedef enum {
    TEZOS_WALK_DONE,           /* nothing left to ask -- call
                                  tezos_micheline_build_value with the
                                  walker's own collected arrays */
    TEZOS_WALK_NEED_LIST_COUNT,/* prompt "how many items?", then call
                                  tezos_micheline_walker_submit_list_count */
    TEZOS_WALK_NEED_LEAF,      /* prompt for one value of the kind named
                                  in *out_leaf_kind, then call
                                  tezos_micheline_walker_submit_leaf */
} tezos_walk_action_t;

#define TEZOS_WALK_MAX_STACK 48   /* matches TEZOS_MTYPE_MAX_NODES-scale headroom */
#define TEZOS_WALK_MAX_LEAF_LEN 128

typedef struct {
    const tezos_micheline_type_pool_t *pool;
    int stack[TEZOS_WALK_MAX_STACK];
    int stack_top; /* number of type-node indices currently on the stack */

    int pending_list_element_type; /* valid only right after NEED_LIST_COUNT */

    char leaf_storage[TEZOS_MTYPE_MAX_NODES][TEZOS_WALK_MAX_LEAF_LEN];
    const char *leaf_ptrs[TEZOS_MTYPE_MAX_NODES]; /* points into leaf_storage;
        this is exactly the array tezos_micheline_build_value expects */
    int leaf_count;

    int list_item_counts[8]; /* same bound as tezos_micheline_build_value's
        internal list_item_ptrs sizing */
    int list_item_counts_len;
} tezos_micheline_walker_t;

/* Starts (or restarts) a walk at `type_root` within `pool`. */
void tezos_micheline_walker_init(tezos_micheline_walker_t *w,
                                 const tezos_micheline_type_pool_t *pool, int type_root);

/* Advances the walker to its next required action. If the result is
   TEZOS_WALK_NEED_LEAF, `*out_leaf_kind` is set to the kind of value
   needed (so the caller can show an appropriate prompt/hint) --
   ignored for the other two results. Safe to call repeatedly without
   side effects until the corresponding submit_* function is called. */
tezos_walk_action_t tezos_micheline_walker_next(tezos_micheline_walker_t *w,
                                                tezos_micheline_type_kind_t *out_leaf_kind);

/* Provide the count for a TEZOS_WALK_NEED_LIST_COUNT step. Returns
   false (and leaves the walker unchanged) if count is negative or
   exceeds TEZOS_MICHELINE_MAX_LIST_ITEMS, or if the walker isn't
   actually awaiting a list count right now -- a real caller bug,
   surfaced rather than silently ignored. */
bool tezos_micheline_walker_submit_list_count(tezos_micheline_walker_t *w, int count);

/* Provide the value for a TEZOS_WALK_NEED_LEAF step, as a plain string
   in the same format tezos_micheline_build_value's leaf_values expects
   (decimal for int/nat/mutez/timestamp, hex for bytes, tz/KT1 string
   for address/key_hash, "true"/"false" for bool). This function does
   NOT validate the value itself -- that happens later, for real, when
   tezos_micheline_build_value is finally called with the complete
   arrays; a bad value here just means that later call fails cleanly,
   the same as it always has. Returns false if the walker isn't
   actually awaiting a leaf right now, or the value is longer than
   TEZOS_WALK_MAX_LEAF_LEN-1. */
bool tezos_micheline_walker_submit_leaf(tezos_micheline_walker_t *w, const char *value);

