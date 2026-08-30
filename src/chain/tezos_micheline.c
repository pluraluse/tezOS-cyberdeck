#include "tezos_micheline.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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
