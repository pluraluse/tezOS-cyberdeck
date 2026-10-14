#include "tezos_micheline.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <cjson/cJSON.h>

tezos_micheline_node_t tezos_micheline_int(const char *decimal) {
    tezos_micheline_node_t n = {0};
    n.kind = MICHELINE_INT;
    n.int_decimal = decimal;
    return n;
}
tezos_micheline_node_t tezos_micheline_string(const char *s) {
    tezos_micheline_node_t n = {0};
    n.kind = MICHELINE_STRING;
    n.string_value = s;
    return n;
}
tezos_micheline_node_t tezos_micheline_bytes(const uint8_t *data, size_t len) {
    tezos_micheline_node_t n = {0};
    n.kind = MICHELINE_BYTES;
    n.bytes_data = data;
    n.bytes_len = len;
    return n;
}
tezos_micheline_node_t tezos_micheline_prim0(const char *name) {
    tezos_micheline_node_t n = {0};
    n.kind = MICHELINE_PRIM;
    n.prim_name = name;
    n.arg_count = 0;
    return n;
}
tezos_micheline_node_t tezos_micheline_prim1(const char *name, const tezos_micheline_node_t *arg) {
    tezos_micheline_node_t n = {0};
    n.kind = MICHELINE_PRIM;
    n.prim_name = name;
    n.args[0] = arg;
    n.arg_count = 1;
    return n;
}
tezos_micheline_node_t tezos_micheline_prim2(const char *name, const tezos_micheline_node_t *a,
                                             const tezos_micheline_node_t *b) {
    tezos_micheline_node_t n = {0};
    n.kind = MICHELINE_PRIM;
    n.prim_name = name;
    n.args[0] = a;
    n.args[1] = b;
    n.arg_count = 2;
    return n;
}
tezos_micheline_node_t tezos_micheline_sequence(const tezos_micheline_node_t *const *items, int count) {
    tezos_micheline_node_t n = {0};
    n.kind = MICHELINE_SEQUENCE;
    n.seq_items = items;
    n.seq_count = count;
    return n;
}
tezos_micheline_node_t tezos_micheline_unit(void) { return tezos_micheline_prim0("Unit"); }
tezos_micheline_node_t tezos_micheline_pair(const tezos_micheline_node_t *a, const tezos_micheline_node_t *b) {
    return tezos_micheline_prim2("Pair", a, b);
}

/* Primitive code table -- the data-constructor subset this module
   supports (see header scope note). Verified against Taquito's
   opMapping (constants.ts). */
static bool prim_code(const char *name, uint8_t *code_out) {
    static const struct { const char *name; uint8_t code; } TABLE[] = {
        {"False", 0x03}, {"Elt", 0x04}, {"Left", 0x05}, {"None", 0x06},
        {"Pair", 0x07}, {"Right", 0x08}, {"Some", 0x09}, {"True", 0x0a},
        {"Unit", 0x0b},
    };
    for (size_t i = 0; i < sizeof(TABLE)/sizeof(TABLE[0]); i++) {
        if (strcmp(name, TABLE[i].name) == 0) { *code_out = TABLE[i].code; return true; }
    }
    return false;
}

static void append_be32(tezos_bytes_t *out, uint32_t len) {
    uint8_t b[4] = { (uint8_t)(len>>24), (uint8_t)(len>>16), (uint8_t)(len>>8), (uint8_t)len };
    tezos_bytes_append(out, b, 4);
}

/* Signed zarith, per Taquito's michelson/codec.ts intEncoder -- NOT the
   same as tezos_forge.h's unsigned zarith. The first byte carries 6
   data bits (bit 6 = sign, bit 7 = continuation); every subsequent byte
   carries 7 data bits (bit 7 = continuation only). Verified against an
   independent Python port of the exact JS algorithm for a spread of
   values including the coincide-with-unsigned small-value case (10) and
   the first value where they diverge (100) -- see
   docs/build-notes/wallet-app-build-notes.md. Scoped to fit a signed
   64-bit range; arbitrary-precision Micheline ints (Taquito supports
   these via BigNumber) aren't needed for anything this device forges
   today (token IDs, small counters, amounts) -- same practical scope
   cut already made for the unsigned zarith in tezos_forge.c. */
static bool encode_signed_zarith(tezos_bytes_t *out, const char *decimal) {
    char *endptr;
    long long value = strtoll(decimal, &endptr, 10);
    if (endptr == decimal || *endptr != '\0') return false;

    bool negative = value < 0;
    unsigned long long magnitude = negative ? (unsigned long long)(-(value + 1)) + 1 : (unsigned long long)value;
    /* (the +1/-1 dance above avoids UB negating LLONG_MIN) */

    uint8_t first = (uint8_t)(magnitude & 0x3F); /* first byte: 6 data bits */
    magnitude >>= 6;
    if (negative) first |= 0x40; /* sign bit */
    if (magnitude != 0) first |= 0x80; /* continuation */
    tezos_bytes_append_byte(out, first);

    while (magnitude != 0) {
        uint8_t b = (uint8_t)(magnitude & 0x7F);
        magnitude >>= 7;
        if (magnitude != 0) b |= 0x80;
        tezos_bytes_append_byte(out, b);
    }
    return true;
}

bool tezos_micheline_encode(const tezos_micheline_node_t *node, tezos_bytes_t *out) {
    switch (node->kind) {
        case MICHELINE_INT:
            tezos_bytes_append_byte(out, 0x00);
            return encode_signed_zarith(out, node->int_decimal);

        case MICHELINE_STRING: {
            tezos_bytes_append_byte(out, 0x01);
            size_t len = strlen(node->string_value);
            append_be32(out, (uint32_t)len);
            tezos_bytes_append(out, (const uint8_t *)node->string_value, len);
            return true;
        }

        case MICHELINE_BYTES:
            tezos_bytes_append_byte(out, 0x0a);
            append_be32(out, (uint32_t)node->bytes_len);
            tezos_bytes_append(out, node->bytes_data, node->bytes_len);
            return true;

        case MICHELINE_SEQUENCE: {
            tezos_bytes_t inner; tezos_bytes_init(&inner);
            for (int i = 0; i < node->seq_count; i++) {
                if (!tezos_micheline_encode(node->seq_items[i], &inner)) { tezos_bytes_free(&inner); return false; }
            }
            tezos_bytes_append_byte(out, 0x02);
            append_be32(out, (uint32_t)inner.len);
            tezos_bytes_append(out, inner.data, inner.len);
            tezos_bytes_free(&inner);
            return true;
        }

        case MICHELINE_PRIM: {
            if (node->arg_count < 0 || node->arg_count > 2) return false; /* see header scope note */
            uint8_t code;
            if (!prim_code(node->prim_name, &code)) return false;

            bool has_annot = node->annot != NULL;
            uint8_t preamble = (uint8_t)(2 * node->arg_count + (has_annot ? 1 : 0) + 0x03);
            tezos_bytes_append_byte(out, preamble);
            tezos_bytes_append_byte(out, code);

            for (int i = 0; i < node->arg_count; i++) {
                if (!tezos_micheline_encode(node->args[i], out)) return false;
            }
            if (has_annot) {
                size_t alen = strlen(node->annot);
                append_be32(out, (uint32_t)alen);
                tezos_bytes_append(out, (const uint8_t *)node->annot, alen);
            }
            return true;
        }
    }
    return false;
}

/* Known short entrypoint names get a single-byte tag; anything else
   gets 0xff + 1-byte length + UTF8 name (verified against Taquito's
   entrypointEncoder -- the "ff" + sliced valueEncoder(...).slice(8)
   trick reduces, for names under 256 chars, to exactly this). */
static bool entrypoint_short_code(const char *name, uint8_t *code_out) {
    static const struct { const char *name; uint8_t code; } TABLE[] = {
        {"default", 0x00}, {"root", 0x01}, {"do", 0x02}, {"set_delegate", 0x03},
        {"remove_delegate", 0x04}, {"deposit", 0x05}, {"stake", 0x06},
        {"unstake", 0x07}, {"finalize_unstake", 0x08}, {"set_delegate_parameters", 0x09},
    };
    for (size_t i = 0; i < sizeof(TABLE)/sizeof(TABLE[0]); i++) {
        if (strcmp(name, TABLE[i].name) == 0) { *code_out = TABLE[i].code; return true; }
    }
    return false;
}

bool tezos_forge_parameters(tezos_bytes_t *out, const char *entrypoint,
                            const tezos_micheline_node_t *value) {
    bool is_default_unit = (entrypoint == NULL || strcmp(entrypoint, "default") == 0)
                          && (value == NULL || (value->kind == MICHELINE_PRIM
                              && value->prim_name && strcmp(value->prim_name, "Unit") == 0));
    if (is_default_unit) {
        tezos_bytes_append_byte(out, 0x00);
        return true;
    }
    if (!entrypoint || !value) return false; /* non-default entrypoint needs a real value */

    tezos_bytes_append_byte(out, 0xff);
    uint8_t short_code;
    if (entrypoint_short_code(entrypoint, &short_code)) {
        tezos_bytes_append_byte(out, short_code);
    } else {
        size_t len = strlen(entrypoint);
        if (len > 31) return false; /* ENTRYPOINT_MAX_LENGTH, per Taquito's constant */
        tezos_bytes_append_byte(out, (uint8_t)len);
        tezos_bytes_append(out, (const uint8_t *)entrypoint, len);
    }

    tezos_bytes_t encoded_value; tezos_bytes_init(&encoded_value);
    if (!tezos_micheline_encode(value, &encoded_value)) { tezos_bytes_free(&encoded_value); return false; }
    append_be32(out, (uint32_t)encoded_value.len);
    tezos_bytes_append(out, encoded_value.data, encoded_value.len);
    tezos_bytes_free(&encoded_value);
    return true;
}

/* ============================================================
   Dynamic entrypoint calling -- see the header's design note on why
   this parses the STANDARD Micheline type grammar rather than TZKT's
   own less-documented human-readable schema convention.
   ============================================================ */

static bool parse_type_node(cJSON *json, tezos_micheline_type_pool_t *pool, int *out_index) {
    if (pool->node_count >= TEZOS_MTYPE_MAX_NODES) return false; /* bounded, fails cleanly */
    if (!cJSON_IsObject(json)) return false;

    cJSON *prim = cJSON_GetObjectItemCaseSensitive(json, "prim");
    if (!cJSON_IsString(prim) || !prim->valuestring) return false;

    tezos_micheline_type_kind_t kind;
    const char *p = prim->valuestring;
    if      (strcmp(p, "int") == 0)     kind = TEZOS_MTYPE_INT;
    else if (strcmp(p, "nat") == 0)     kind = TEZOS_MTYPE_NAT;
    else if (strcmp(p, "mutez") == 0)   kind = TEZOS_MTYPE_MUTEZ;
    else if (strcmp(p, "timestamp") == 0) kind = TEZOS_MTYPE_TIMESTAMP;
    else if (strcmp(p, "key_hash") == 0) kind = TEZOS_MTYPE_KEY_HASH;
    else if (strcmp(p, "string") == 0)  kind = TEZOS_MTYPE_STRING;
    else if (strcmp(p, "bytes") == 0)   kind = TEZOS_MTYPE_BYTES;
    else if (strcmp(p, "address") == 0) kind = TEZOS_MTYPE_ADDRESS;
    else if (strcmp(p, "bool") == 0)    kind = TEZOS_MTYPE_BOOL;
    else if (strcmp(p, "unit") == 0)    kind = TEZOS_MTYPE_UNIT;
    else if (strcmp(p, "pair") == 0)    kind = TEZOS_MTYPE_PAIR;
    else if (strcmp(p, "or") == 0)      kind = TEZOS_MTYPE_OR;
    else if (strcmp(p, "option") == 0)  kind = TEZOS_MTYPE_OPTION;
    else if (strcmp(p, "list") == 0)    kind = TEZOS_MTYPE_LIST;
    else return false; /* outside the supported scope -- fail cleanly, don't approximate */

    int my_index = pool->node_count++;
    tezos_micheline_type_node_t *node = &pool->nodes[my_index];
    memset(node, 0, sizeof *node);
    node->kind = kind;
    node->arg_count = 0;
    node->args[0] = node->args[1] = -1;

    /* field annotation, e.g. "%recipient" -- strip the leading '%' */
    cJSON *annots = cJSON_GetObjectItemCaseSensitive(json, "annots");
    if (cJSON_IsArray(annots) && cJSON_GetArraySize(annots) > 0) {
        cJSON *first = cJSON_GetArrayItem(annots, 0);
        if (cJSON_IsString(first) && first->valuestring) {
            const char *a = first->valuestring;
            if (*a == '%' || *a == ':' || *a == '@') a++;
            strncpy(node->annot, a, sizeof node->annot - 1);
        }
    }

    cJSON *args = cJSON_GetObjectItemCaseSensitive(json, "args");
    if (kind == TEZOS_MTYPE_PAIR || kind == TEZOS_MTYPE_OR) {
        /* both require exactly 2 sub-types, matching this module's
           value-side arg_count<=2 scope */
        if (!cJSON_IsArray(args) || cJSON_GetArraySize(args) != 2) return false;
        int a0, a1;
        if (!parse_type_node(cJSON_GetArrayItem(args, 0), pool, &a0)) return false;
        if (!parse_type_node(cJSON_GetArrayItem(args, 1), pool, &a1)) return false;
        node = &pool->nodes[my_index]; /* re-fetch: recursive calls may have grown the pool array in place, but not reallocated -- this is just defensive, pool->nodes is a fixed array */
        node->arg_count = 2;
        node->args[0] = a0;
        node->args[1] = a1;
    } else if (kind == TEZOS_MTYPE_OPTION || kind == TEZOS_MTYPE_LIST) {
        if (!cJSON_IsArray(args) || cJSON_GetArraySize(args) != 1) return false;
        int a0;
        if (!parse_type_node(cJSON_GetArrayItem(args, 0), pool, &a0)) return false;
        node = &pool->nodes[my_index];
        node->arg_count = 1;
        node->args[0] = a0;
    }
    /* int/nat/mutez/timestamp/key_hash/string/bytes/address/bool/unit: no sub-types, arg_count stays 0 */

    *out_index = my_index;
    return true;
}

int tezos_micheline_parse_type(const char *json_type, tezos_micheline_type_pool_t *pool) {
    pool->node_count = 0;
    cJSON *root = cJSON_Parse(json_type);
    if (!root) return -1;
    int index = -1;
    bool ok = parse_type_node(root, pool, &index);
    cJSON_Delete(root);
    return ok ? index : -1;
}

int tezos_micheline_type_leaf_count(const tezos_micheline_type_pool_t *pool, int root_index) {
    if (root_index < 0 || root_index >= pool->node_count) return -1;
    const tezos_micheline_type_node_t *node = &pool->nodes[root_index];
    switch (node->kind) {
        case TEZOS_MTYPE_UNIT:
            return 0;
        case TEZOS_MTYPE_PAIR:
        case TEZOS_MTYPE_OR: {
            int a = tezos_micheline_type_leaf_count(pool, node->args[0]);
            int b = tezos_micheline_type_leaf_count(pool, node->args[1]);
            if (a < 0 || b < 0) return -1;
            return a + b;
        }
        case TEZOS_MTYPE_OPTION:
            return tezos_micheline_type_leaf_count(pool, node->args[0]);
        case TEZOS_MTYPE_LIST:
            /* per-ELEMENT leaf count, not a total -- see the header's
               note on why lists need their own value-building
               function rather than fitting this fixed-count model */
            return tezos_micheline_type_leaf_count(pool, node->args[0]);
        default: /* int/nat/mutez/timestamp/key_hash/string/bytes/address/bool: exactly one leaf */
            return 1;
    }
}

/* Decodes a hex string ("48656c6c6f") into raw bytes. Returns the
   decoded length, or -1 on malformed input (odd length, non-hex
   characters) -- fails cleanly rather than silently truncating or
   encoding garbage. */
static int decode_hex(const char *hex, uint8_t *out, int out_cap) {
    int len = (int)strlen(hex);
    if (len % 2 != 0) return -1;
    int byte_count = len / 2;
    if (byte_count > out_cap) return -1;
    for (int i = 0; i < byte_count; i++) {
        char hi = hex[i*2], lo = hex[i*2+1];
        if (!isxdigit((unsigned char)hi) || !isxdigit((unsigned char)lo)) return -1;
        int hi_val = isdigit((unsigned char)hi) ? hi - '0' : tolower(hi) - 'a' + 10;
        int lo_val = isdigit((unsigned char)lo) ? lo - '0' : tolower(lo) - 'a' + 10;
        out[i] = (uint8_t)((hi_val << 4) | lo_val);
    }
    return byte_count;
}

/* recursive helper -- consumes leaves via *leaf_pos (advances it),
   writes new value nodes into value_pool starting at *value_pos
   (advances it). scratch_bytes/scratch_addrs are per-leaf-position
   dedicated storage (indexed by leaf position, NOT shared/reused
   across leaves) -- a type with multiple bytes- or address-typed
   leaves (e.g. pair(address, address)) would otherwise have each
   later leaf silently corrupt the earlier one's already-built value if
   they aliased a single shared buffer. Caught before this ever shipped,
   not after. Returns the built node's index, or -1 on failure. */
static int build_value_node(const tezos_micheline_type_pool_t *type_pool, int type_index,
                            const char *const *leaf_values, int leaf_count, int *leaf_pos,
                            const int *list_item_counts, int list_item_counts_len, int *list_pos,
                            tezos_micheline_node_t *value_pool, int value_pool_cap, int *value_pos,
                            uint8_t scratch_bytes[][256], tezos_bytes_t *scratch_addrs,
                            const tezos_micheline_node_t *list_item_ptrs[][TEZOS_MICHELINE_MAX_LIST_ITEMS]) {
    if (type_index < 0 || type_index >= type_pool->node_count) return -1;
    const tezos_micheline_type_node_t *tnode = &type_pool->nodes[type_index];

    if (tnode->kind == TEZOS_MTYPE_UNIT) {
        if (*value_pos >= value_pool_cap) return -1;
        int idx = (*value_pos)++;
        value_pool[idx] = tezos_micheline_unit();
        return idx;
    }
    if (tnode->kind == TEZOS_MTYPE_PAIR) {
        int a = build_value_node(type_pool, tnode->args[0], leaf_values, leaf_count, leaf_pos,
                                 list_item_counts, list_item_counts_len, list_pos,
                                 value_pool, value_pool_cap, value_pos, scratch_bytes, scratch_addrs, list_item_ptrs);
        if (a < 0) return -1;
        int b = build_value_node(type_pool, tnode->args[1], leaf_values, leaf_count, leaf_pos,
                                 list_item_counts, list_item_counts_len, list_pos,
                                 value_pool, value_pool_cap, value_pos, scratch_bytes, scratch_addrs, list_item_ptrs);
        if (b < 0) return -1;
        if (*value_pos >= value_pool_cap) return -1;
        int idx = (*value_pos)++;
        value_pool[idx] = tezos_micheline_pair(&value_pool[a], &value_pool[b]);
        return idx;
    }
    if (tnode->kind == TEZOS_MTYPE_OPTION) {
        /* Scoped simply: always builds "Some" -- a real "skip this
           optional field entirely" UI flow is future work, not
           attempted here. */
        int inner = build_value_node(type_pool, tnode->args[0], leaf_values, leaf_count, leaf_pos,
                                     list_item_counts, list_item_counts_len, list_pos,
                                     value_pool, value_pool_cap, value_pos, scratch_bytes, scratch_addrs, list_item_ptrs);
        if (inner < 0) return -1;
        if (*value_pos >= value_pool_cap) return -1;
        int idx = (*value_pos)++;
        value_pool[idx] = tezos_micheline_prim1("Some", &value_pool[inner]);
        return idx;
    }
    if (tnode->kind == TEZOS_MTYPE_OR) {
        /* Scoped simply: always builds "Left" -- see PAIR/OPTION's
           reasoning above, same real UI decision deferred. */
        int inner = build_value_node(type_pool, tnode->args[0], leaf_values, leaf_count, leaf_pos,
                                     list_item_counts, list_item_counts_len, list_pos,
                                     value_pool, value_pool_cap, value_pos, scratch_bytes, scratch_addrs, list_item_ptrs);
        if (inner < 0) return -1;
        if (*value_pos >= value_pool_cap) return -1;
        int idx = (*value_pos)++;
        value_pool[idx] = tezos_micheline_prim1("Left", &value_pool[inner]);
        return idx;
    }
    if (tnode->kind == TEZOS_MTYPE_LIST) {
        /* Unified list handling -- works ANYWHERE in the tree now, not
           just at the root. Confirmed genuinely necessary (not just a
           nice-to-have) against the real Teia Channels contract's
           write_message entrypoint, whose cc/bcc/attachments fields
           are all option(list(...)) -- a shape the original
           list-only-at-the-root design couldn't build at all, since
           this function used to just fail outright on any LIST node.
           Reads this list's item count from the list_item_counts
           side-channel (one entry per LIST node encountered, in this
           same depth-first order), then builds that many items of the
           element type, collecting them into a per-list-instance
           scratch array (list_item_ptrs, indexed by *list_pos BEFORE
           incrementing it -- same "dedicated storage per instance, not
           shared" discipline as the address/bytes leaf scratch
           buffers, since a tree with multiple lists must not have a
           later list's items overwrite an earlier list's already-built
           ones). */
        if (*list_pos >= list_item_counts_len) return -1; /* caller didn't provide a count for this list */
        int list_instance = (*list_pos)++;
        if (list_instance >= 8) return -1; /* bounded -- see list_item_ptrs' sizing below */
        int item_count = list_item_counts[list_instance];
        if (item_count < 0 || item_count > TEZOS_MICHELINE_MAX_LIST_ITEMS) return -1;

        for (int i = 0; i < item_count; i++) {
            int item_root = build_value_node(type_pool, tnode->args[0], leaf_values, leaf_count, leaf_pos,
                                             list_item_counts, list_item_counts_len, list_pos,
                                             value_pool, value_pool_cap, value_pos, scratch_bytes, scratch_addrs, list_item_ptrs);
            if (item_root < 0) return -1; /* this item's leaves failed -- fail the whole list, not a partial one */
            list_item_ptrs[list_instance][i] = &value_pool[item_root];
        }
        if (*value_pos >= value_pool_cap) return -1;
        int seq_idx = (*value_pos)++;
        value_pool[seq_idx] = tezos_micheline_sequence(list_item_ptrs[list_instance], item_count);
        return seq_idx;
    }

    /* leaf types: int/nat/mutez/timestamp/key_hash/string/bytes/address/bool */
    if (*leaf_pos >= leaf_count) return -1;
    int this_leaf_pos = *leaf_pos;
    const char *leaf = leaf_values[(*leaf_pos)++];
    if (*value_pos >= value_pool_cap) return -1;
    int idx = *value_pos;

    switch (tnode->kind) {
        case TEZOS_MTYPE_INT:
        case TEZOS_MTYPE_NAT:
        case TEZOS_MTYPE_MUTEZ:
        case TEZOS_MTYPE_TIMESTAMP: {
            /* validate it's actually numeric (optionally signed for
               int) -- fail cleanly rather than encode garbage.
               timestamp uses the same signed-int wire encoding as int
               (a Unix epoch second count), per the Michelson spec's
               optimized/wire form -- confirmed relevant against the
               real Liquidity Baking contract, whose deadline fields
               are all timestamp-typed. */
            const char *p = leaf;
            if (*p == '-' && (tnode->kind == TEZOS_MTYPE_INT || tnode->kind == TEZOS_MTYPE_TIMESTAMP)) p++;
            if (*p == '\0') return -1;
            for (; *p; p++) if (!isdigit((unsigned char)*p)) return -1;
            value_pool[idx] = tezos_micheline_int(leaf);
            break;
        }
        case TEZOS_MTYPE_STRING:
            value_pool[idx] = tezos_micheline_string(leaf);
            break;
        case TEZOS_MTYPE_BYTES: {
            /* dedicated per-leaf-position buffer -- not shared/reused
               across leaves, see the function-level comment above */
            int n = decode_hex(leaf, scratch_bytes[this_leaf_pos], 256);
            if (n < 0) return -1;
            value_pool[idx] = tezos_micheline_bytes(scratch_bytes[this_leaf_pos], (size_t)n);
            break;
        }
        case TEZOS_MTYPE_ADDRESS: {
            /* dedicated per-leaf-position tezos_bytes_t -- same reasoning */
            tezos_bytes_t *addr_buf = &scratch_addrs[this_leaf_pos];
            tezos_bytes_init(addr_buf);
            if (!tezos_forge_address(addr_buf, leaf)) return -1;
            value_pool[idx] = tezos_micheline_bytes(addr_buf->data, addr_buf->len);
            /* addr_buf is intentionally not freed here -- value_pool[idx]
               points into it for the caller's lifetime, matching this
               module's "no allocation happens inside this module"
               contract as closely as address encoding's real need for a
               byte buffer allows; caller owns scratch_addrs' lifetime */
            break;
        }
        case TEZOS_MTYPE_KEY_HASH: {
            /* reuses tezos_forge_pkh directly -- already verified for
               the operation-level source/delegate fields, and produces
               EXACTLY the 21-byte curve-tag+hash encoding key_hash
               values need (no leading address-tag byte, unlike
               ADDRESS above). Confirmed relevant against the real
               QuipuSwap contract, whose delegate/baker fields
               (`ban`'s baker, `vote`'s candidate, etc.) are all
               key_hash-typed. */
            tezos_bytes_t *kh_buf = &scratch_addrs[this_leaf_pos];
            tezos_bytes_init(kh_buf);
            if (!tezos_forge_pkh(kh_buf, leaf)) return -1;
            value_pool[idx] = tezos_micheline_bytes(kh_buf->data, kh_buf->len);
            break;
        }
        case TEZOS_MTYPE_BOOL:
            if (strcmp(leaf, "true") == 0) value_pool[idx] = tezos_micheline_prim0("True");
            else if (strcmp(leaf, "false") == 0) value_pool[idx] = tezos_micheline_prim0("False");
            else return -1;
            break;
        default:
            return -1; /* unreachable given the type-parse scope, but don't assume */
    }
    (*value_pos)++;
    return idx;
}

int tezos_micheline_build_value(const tezos_micheline_type_pool_t *type_pool, int type_root,
                                const char *const *leaf_values, int leaf_count,
                                const int *list_item_counts, int list_item_counts_len,
                                tezos_micheline_node_t *value_pool, int value_pool_cap) {
    /* dedicated per-leaf-position scratch storage, sized to the max
       possible leaf count (bounded by TEZOS_MTYPE_MAX_NODES, since
       there can never be more leaves than type-tree nodes) -- see the
       aliasing-bug comment on build_value_node above for why this
       can't be one shared buffer. */
    static uint8_t scratch_bytes[TEZOS_MTYPE_MAX_NODES][256];
    static tezos_bytes_t scratch_addrs[TEZOS_MTYPE_MAX_NODES];
    /* per-list-instance item-pointer storage -- up to 8 distinct list
       nodes per tree (comfortably more than any real contract seen so
       far; write_message has 4), each up to TEZOS_MICHELINE_MAX_LIST_ITEMS
       items. Dedicated per instance, not shared, same aliasing
       discipline as the leaf scratch buffers above. */
    static const tezos_micheline_node_t *list_item_ptrs[8][TEZOS_MICHELINE_MAX_LIST_ITEMS];

    int leaf_pos = 0, value_pos = 0, list_pos = 0;
    int result = build_value_node(type_pool, type_root, leaf_values, leaf_count, &leaf_pos,
                                  list_item_counts, list_item_counts_len, &list_pos,
                                  value_pool, value_pool_cap, &value_pos,
                                  scratch_bytes, scratch_addrs, list_item_ptrs);
    if (result < 0) return -1;
    if (leaf_pos != leaf_count) return -1; /* real leaf_count mismatch is a caller bug -- surface it, don't ignore */
    if (list_pos != list_item_counts_len) return -1; /* real list_item_counts_len mismatch, same reasoning */
    return result;
}

int tezos_micheline_build_list_value(const tezos_micheline_type_pool_t *type_pool, int list_type_root,
                                     const char *const *element_leaves, int total_leaf_count, int item_count,
                                     tezos_micheline_node_t *value_pool, int value_pool_cap) {
    if (list_type_root < 0 || list_type_root >= type_pool->node_count) return -1;
    if (type_pool->nodes[list_type_root].kind != TEZOS_MTYPE_LIST) return -1; /* real caller error -- not a list type at all */
    int counts[1] = { item_count };
    return tezos_micheline_build_value(type_pool, list_type_root, element_leaves, total_leaf_count,
                                       counts, 1, value_pool, value_pool_cap);
}

void tezos_micheline_walker_init(tezos_micheline_walker_t *w,
                                 const tezos_micheline_type_pool_t *pool, int type_root) {
    memset(w, 0, sizeof *w);
    w->pool = pool;
    w->stack_top = 0;
    if (type_root >= 0) w->stack[w->stack_top++] = type_root;
}

tezos_walk_action_t tezos_micheline_walker_next(tezos_micheline_walker_t *w,
                                                tezos_micheline_type_kind_t *out_leaf_kind) {
    while (w->stack_top > 0) {
        int top_index = w->stack[w->stack_top - 1];
        const tezos_micheline_type_node_t *node = &w->pool->nodes[top_index];

        switch (node->kind) {
            case TEZOS_MTYPE_UNIT:
                /* 0 leaves needed, matching tezos_micheline_build_value's own
                   contract for it -- skip silently, no UI step */
                w->stack_top--;
                continue;
            case TEZOS_MTYPE_PAIR: {
                w->stack_top--;
                /* push args[1] first, then args[0], so args[0] ends up on
                   top and is processed first -- matches
                   tezos_micheline_build_value's own args[0]-then-args[1] order */
                w->stack[w->stack_top++] = node->args[1];
                w->stack[w->stack_top++] = node->args[0];
                continue;
            }
            case TEZOS_MTYPE_OPTION:
            case TEZOS_MTYPE_OR: {
                /* both always take their first/only real branch (Some / Left),
                   matching this project's already-documented scope */
                w->stack_top--;
                w->stack[w->stack_top++] = node->args[0];
                continue;
            }
            case TEZOS_MTYPE_LIST: {
                w->pending_list_element_type = node->args[0];
                return TEZOS_WALK_NEED_LIST_COUNT; /* stack unchanged -- this
                    LIST node stays on top until submit_list_count pops it */
            }
            default: /* every leaf kind: int/nat/mutez/timestamp/key_hash/string/bytes/address/bool */
                *out_leaf_kind = node->kind;
                return TEZOS_WALK_NEED_LEAF; /* stack unchanged -- popped by submit_leaf */
        }
    }
    return TEZOS_WALK_DONE;
}

bool tezos_micheline_walker_submit_list_count(tezos_micheline_walker_t *w, int count) {
    if (w->stack_top == 0) return false;
    if (w->pool->nodes[w->stack[w->stack_top - 1]].kind != TEZOS_MTYPE_LIST) return false; /* real caller bug */
    if (count < 0 || count > TEZOS_MICHELINE_MAX_LIST_ITEMS) return false;
    if (w->list_item_counts_len >= 8) return false; /* bounded, matches build_value's own list-instance cap */

    w->list_item_counts[w->list_item_counts_len++] = count;
    w->stack_top--; /* pop the LIST node -- now processed */

    /* push `count` copies of the element type, in REVERSE order, so the
       first one popped (processed first, presented to the user as
       "item 1") is conceptually the first item -- see the header's
       design note on why item ordering among identical-type copies is
       purely a UI-labeling concern, not a correctness one, but worth
       getting right for a sane user experience regardless. */
    if (w->stack_top + count > TEZOS_WALK_MAX_STACK) return false; /* real bound, don't overflow */
    for (int i = count - 1; i >= 0; i--) {
        w->stack[w->stack_top++] = w->pending_list_element_type;
    }
    return true;
}

bool tezos_micheline_walker_submit_leaf(tezos_micheline_walker_t *w, const char *value) {
    if (w->stack_top == 0) return false;
    int top_index = w->stack[w->stack_top - 1];
    tezos_micheline_type_kind_t kind = w->pool->nodes[top_index].kind;
    if (kind == TEZOS_MTYPE_PAIR || kind == TEZOS_MTYPE_OPTION || kind == TEZOS_MTYPE_OR
        || kind == TEZOS_MTYPE_LIST || kind == TEZOS_MTYPE_UNIT) {
        return false; /* real caller bug -- not actually awaiting a leaf */
    }
    if (strlen(value) >= TEZOS_WALK_MAX_LEAF_LEN) return false;
    if (w->leaf_count >= TEZOS_MTYPE_MAX_NODES) return false; /* bounded */

    strncpy(w->leaf_storage[w->leaf_count], value, TEZOS_WALK_MAX_LEAF_LEN - 1);
    w->leaf_storage[w->leaf_count][TEZOS_WALK_MAX_LEAF_LEN - 1] = '\0';
    w->leaf_ptrs[w->leaf_count] = w->leaf_storage[w->leaf_count];
    w->leaf_count++;
    w->stack_top--; /* pop the leaf node -- now processed */
    return true;
}
