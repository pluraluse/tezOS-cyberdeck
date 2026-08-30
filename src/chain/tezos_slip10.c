#include "tezos_slip10.h"
#include <sodium.h>
#include <string.h>

void tezos_slip10_master(const uint8_t seed[64], tezos_slip10_node_t *out) {
    uint8_t h[64];
    crypto_auth_hmacsha512_state st;
    crypto_auth_hmacsha512_init(&st, (const uint8_t *)"ed25519 seed", 12);
    crypto_auth_hmacsha512_update(&st, seed, 64);
    crypto_auth_hmacsha512_final(&st, h);
    memcpy(out->key, h, 32);
    memcpy(out->chain_code, h + 32, 32);
}

void tezos_slip10_derive_hardened(const tezos_slip10_node_t *parent, uint32_t index,
                                  tezos_slip10_node_t *out) {
    uint32_t hardened_index = index | 0x80000000u;

    /* data = 0x00 || parent_key(32) || ser32(hardened_index), matching
       the "always hardened" formula for ed25519 -- no branch on
       normal-vs-hardened since ed25519 only ever does this one. */
    uint8_t data[1 + 32 + 4];
    data[0] = 0x00;
    memcpy(data + 1, parent->key, 32);
    data[33] = (uint8_t)(hardened_index >> 24);
    data[34] = (uint8_t)(hardened_index >> 16);
    data[35] = (uint8_t)(hardened_index >> 8);
    data[36] = (uint8_t)(hardened_index);

    uint8_t h[64];
    crypto_auth_hmacsha512_state st;
    crypto_auth_hmacsha512_init(&st, parent->chain_code, 32);
    crypto_auth_hmacsha512_update(&st, data, sizeof data);
    crypto_auth_hmacsha512_final(&st, h);
    memcpy(out->key, h, 32);
    memcpy(out->chain_code, h + 32, 32);
}

void tezos_slip10_derive_tezos_path(const uint8_t seed[64], uint32_t account,
                                    tezos_slip10_node_t *out) {
    tezos_slip10_node_t node;
    tezos_slip10_master(seed, &node);
    tezos_slip10_derive_hardened(&node, 44, &node);
    tezos_slip10_derive_hardened(&node, 1729, &node);
    tezos_slip10_derive_hardened(&node, account, &node);
    tezos_slip10_derive_hardened(&node, 0, &node);
    *out = node;
}
