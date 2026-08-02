#include "tezos_forge.h"
#include "tezos_base58.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

void tezos_bytes_init(tezos_bytes_t *b) { b->data = NULL; b->len = 0; b->cap = 0; }
void tezos_bytes_free(tezos_bytes_t *b) { free(b->data); b->data = NULL; b->len = b->cap = 0; }

static void bytes_ensure_cap(tezos_bytes_t *b, size_t additional) {
    if (b->len + additional <= b->cap) return;
    size_t new_cap = b->cap == 0 ? 64 : b->cap * 2;
    while (new_cap < b->len + additional) new_cap *= 2;
    b->data = realloc(b->data, new_cap);
    b->cap = new_cap;
}

void tezos_bytes_append(tezos_bytes_t *b, const uint8_t *data, size_t len) {
    bytes_ensure_cap(b, len);
    memcpy(b->data + b->len, data, len);
    b->len += len;
}
void tezos_bytes_append_byte(tezos_bytes_t *b, uint8_t byte) { tezos_bytes_append(b, &byte, 1); }

bool tezos_forge_zarith(tezos_bytes_t *out, const char *decimal_str) {
    errno = 0;
    char *endptr;
    unsigned long long value = strtoull(decimal_str, &endptr, 10);
    if (errno != 0 || endptr == decimal_str || *endptr != '\0') return false;
    do {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        tezos_bytes_append_byte(out, byte);
    } while (value != 0);
    return true;
}

/* Dispatches on the string's own prefix characters ("tz1"/"tz2"/"tz3"/"KT1")
   rather than trying every base58 prefix in turn — cheaper and clearer. */
static bool decode_implicit_or_originated(const char *address_str, uint8_t *hash20,
                                          int *curve_tag /* -1 if originated (KT1) */) {
    size_t out_len;
    if (strncmp(address_str, "tz1", 3) == 0) {
        *curve_tag = 0;
        return tezos_b58_decode(address_str, TEZOS_PREFIX_TZ1, hash20, &out_len, 20) && out_len == 20;
    } else if (strncmp(address_str, "tz2", 3) == 0) {
        *curve_tag = 1;
        return tezos_b58_decode(address_str, TEZOS_PREFIX_TZ2, hash20, &out_len, 20) && out_len == 20;
    } else if (strncmp(address_str, "tz3", 3) == 0) {
        *curve_tag = 2;
        return tezos_b58_decode(address_str, TEZOS_PREFIX_TZ3, hash20, &out_len, 20) && out_len == 20;
    } else if (strncmp(address_str, "KT1", 3) == 0) {
        *curve_tag = -1;
        return tezos_b58_decode(address_str, TEZOS_PREFIX_KT1, hash20, &out_len, 20) && out_len == 20;
    }
    return false;
}

bool tezos_forge_address(tezos_bytes_t *out, const char *address_str) {
    uint8_t hash[20];
    int curve_tag;
    if (!decode_implicit_or_originated(address_str, hash, &curve_tag)) return false;

    if (curve_tag == -1) { /* originated (KT1): 0x01 + 20-byte hash + 0x00 pad */
        tezos_bytes_append_byte(out, 0x01);
        tezos_bytes_append(out, hash, 20);
        tezos_bytes_append_byte(out, 0x00);
    } else { /* implicit: 0x00 + curve tag + 20-byte hash */
        tezos_bytes_append_byte(out, 0x00);
        tezos_bytes_append_byte(out, (uint8_t)curve_tag);
        tezos_bytes_append(out, hash, 20);
    }
    return true;
}

bool tezos_forge_pkh(tezos_bytes_t *out, const char *address_str) {
    uint8_t hash[20];
    int curve_tag;
    if (!decode_implicit_or_originated(address_str, hash, &curve_tag)) return false;
    if (curve_tag == -1) return false; /* source/delegate can't be a KT1 */
    tezos_bytes_append_byte(out, (uint8_t)curve_tag);
    tezos_bytes_append(out, hash, 20);
    return true;
}

static bool forge_manager_fields(tezos_bytes_t *out, const char *source, const char *fee,
                                 const char *counter, const char *gas_limit, const char *storage_limit) {
    return tezos_forge_pkh(out, source)
        && tezos_forge_zarith(out, fee)
        && tezos_forge_zarith(out, counter)
        && tezos_forge_zarith(out, gas_limit)
        && tezos_forge_zarith(out, storage_limit);
}

bool tezos_forge_reveal(tezos_bytes_t *out, const tezos_reveal_params_t *p) {
    tezos_bytes_append_byte(out, TEZOS_TAG_REVEAL);
    if (!forge_manager_fields(out, p->source, p->fee, p->counter, p->gas_limit, p->storage_limit)) return false;

    /* public_key: 1-byte curve tag (0 = ed25519) + 32-byte raw key,
       decoded from the edpk... string */
    uint8_t pubkey[32];
    size_t out_len;
    if (!tezos_b58_decode(p->public_key, TEZOS_PREFIX_EDPK, pubkey, &out_len, sizeof pubkey) || out_len != 32)
        return false;
    tezos_bytes_append_byte(out, 0x00); /* ed25519 tag */
    tezos_bytes_append(out, pubkey, 32);
    return true;
}

bool tezos_forge_transaction(tezos_bytes_t *out, const tezos_transaction_params_t *p) {
    tezos_bytes_append_byte(out, TEZOS_TAG_TRANSACTION);
    if (!forge_manager_fields(out, p->source, p->fee, p->counter, p->gas_limit, p->storage_limit)) return false;
    if (!tezos_forge_zarith(out, p->amount)) return false;
    if (!tezos_forge_address(out, p->destination)) return false;
    tezos_bytes_append_byte(out, 0x00); /* no parameters — plain tez transfer only, see header */
    return true;
}

bool tezos_forge_delegation(tezos_bytes_t *out, const tezos_delegation_params_t *p) {
    tezos_bytes_append_byte(out, TEZOS_TAG_DELEGATION);
    if (!forge_manager_fields(out, p->source, p->fee, p->counter, p->gas_limit, p->storage_limit)) return false;
    if (p->delegate[0] != '\0') {
        tezos_bytes_append_byte(out, 0xff); /* boolEncoder(true) — confirmed 0xff, not 0x01 */
        if (!tezos_forge_pkh(out, p->delegate)) return false;
    } else {
        tezos_bytes_append_byte(out, 0x00); /* boolEncoder(false) — remove delegation */
    }
    return true;
}

bool tezos_forge_operation_group(tezos_bytes_t *out, const char *branch_b58,
                                  const uint8_t *contents, size_t contents_len) {
    uint8_t branch_hash[32];
    size_t out_len;
    if (!tezos_b58_decode(branch_b58, TEZOS_PREFIX_BLOCK_HASH, branch_hash, &out_len, sizeof branch_hash)
        || out_len != 32) return false;
    tezos_bytes_append(out, branch_hash, 32);
    tezos_bytes_append(out, contents, contents_len);
    return true;
}

bool tezos_bytes_to_hex(const tezos_bytes_t *b, char *out, size_t out_cap) {
    if (b->len * 2 + 1 > out_cap) return false;
    for (size_t i = 0; i < b->len; i++) sprintf(out + i * 2, "%02x", b->data[i]);
    out[b->len * 2] = '\0';
    return true;
}
