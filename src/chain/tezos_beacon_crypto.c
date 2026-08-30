#include "tezos_beacon_crypto.h"
#include "tezos_base58.h"
#include <sodium.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void tezos_beacon_keypair_from_seed(const char *seed_str, tezos_beacon_keypair_t *out) {
    uint8_t hash[32];
    crypto_generichash(hash, 32, (const uint8_t *)seed_str, strlen(seed_str), NULL, 0); /* BLAKE2b-256 */
    crypto_sign_seed_keypair(out->public_key, out->secret_key, hash);
}

bool tezos_beacon_seal(const uint8_t *payload, size_t payload_len,
                       const uint8_t their_pk[32],
                       uint8_t *out, size_t out_cap, size_t *out_len) {
    uint8_t their_x25519_pk[32];
    if (crypto_sign_ed25519_pk_to_curve25519(their_x25519_pk, their_pk) != 0) return false;

    uint8_t eph_pk[crypto_box_PUBLICKEYBYTES], eph_sk[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair(eph_pk, eph_sk);

    /* deterministic nonce = BLAKE2b-24(ephemeral_pk || their_x25519_pk) --
       verified against the real beacon-sdk source, not a typical
       random-nonce NaCl usage */
    uint8_t nonce_input[64];
    memcpy(nonce_input, eph_pk, 32);
    memcpy(nonce_input + 32, their_x25519_pk, 32);
    uint8_t nonce[24];
    crypto_generichash(nonce, 24, nonce_input, 64, NULL, 0);

    size_t needed = 32 + crypto_box_MACBYTES + payload_len;
    if (needed > out_cap) return false;

    memcpy(out, eph_pk, 32);
    if (crypto_box_easy(out + 32, payload, payload_len, nonce, their_x25519_pk, eph_sk) != 0) return false;

    *out_len = needed;
    return true;
}

bool tezos_beacon_open(const uint8_t *sealed, size_t sealed_len,
                       const tezos_beacon_keypair_t *my_keypair,
                       uint8_t *out, size_t out_cap, size_t *out_len) {
    if (sealed_len < 32 + crypto_box_MACBYTES) return false;

    uint8_t my_x25519_sk[32], my_x25519_pk[32];
    if (crypto_sign_ed25519_sk_to_curve25519(my_x25519_sk, my_keypair->secret_key) != 0) return false;
    if (crypto_sign_ed25519_pk_to_curve25519(my_x25519_pk, my_keypair->public_key) != 0) return false;

    const uint8_t *epk = sealed;
    const uint8_t *ciphertext = sealed + 32;
    size_t ciphertext_len = sealed_len - 32;

    uint8_t nonce_input[64];
    memcpy(nonce_input, epk, 32);
    memcpy(nonce_input + 32, my_x25519_pk, 32);
    uint8_t nonce[24];
    crypto_generichash(nonce, 24, nonce_input, 64, NULL, 0);

    size_t plaintext_len = ciphertext_len - crypto_box_MACBYTES;
    if (plaintext_len > out_cap) return false;

    if (crypto_box_open_easy(out, ciphertext, ciphertext_len, nonce, epk, my_x25519_sk) != 0) return false;
    *out_len = plaintext_len;
    return true;
}

bool tezos_beacon_session_keys_client(const tezos_beacon_keypair_t *my_keypair,
                                      const uint8_t their_pk[32],
                                      tezos_beacon_session_keys_t *out) {
    uint8_t my_x25519_pk[32], my_x25519_sk[32], their_x25519_pk[32];
    if (crypto_sign_ed25519_pk_to_curve25519(my_x25519_pk, my_keypair->public_key) != 0) return false;
    if (crypto_sign_ed25519_sk_to_curve25519(my_x25519_sk, my_keypair->secret_key) != 0) return false;
    if (crypto_sign_ed25519_pk_to_curve25519(their_x25519_pk, their_pk) != 0) return false;

    return crypto_kx_client_session_keys(out->rx, out->tx, my_x25519_pk, my_x25519_sk, their_x25519_pk) == 0;
}

bool tezos_beacon_session_keys_server(const tezos_beacon_keypair_t *my_keypair,
                                      const uint8_t their_pk[32],
                                      tezos_beacon_session_keys_t *out) {
    uint8_t my_x25519_pk[32], my_x25519_sk[32], their_x25519_pk[32];
    if (crypto_sign_ed25519_pk_to_curve25519(my_x25519_pk, my_keypair->public_key) != 0) return false;
    if (crypto_sign_ed25519_sk_to_curve25519(my_x25519_sk, my_keypair->secret_key) != 0) return false;
    if (crypto_sign_ed25519_pk_to_curve25519(their_x25519_pk, their_pk) != 0) return false;

    return crypto_kx_server_session_keys(out->rx, out->tx, my_x25519_pk, my_x25519_sk, their_x25519_pk) == 0;
}

bool tezos_beacon_sender_id(const uint8_t pk[32], char *out, size_t out_cap) {
    uint8_t digest[5];
    crypto_generichash(digest, sizeof digest, pk, 32, NULL, 0); /* BLAKE2b, 5-byte output */
    return tezos_b58check_encode_generic(digest, sizeof digest, out, out_cap);
}

bool tezos_beacon_hex_hash(const uint8_t pk[32], char *out, size_t out_cap) {
    uint8_t digest[32];
    crypto_generichash(digest, sizeof digest, pk, 32, NULL, 0); /* BLAKE2b, 32-byte output --
        DIFFERENT digest length than sender_id above, per the real source */
    if (out_cap < sizeof(digest) * 2 + 1) return false;
    for (size_t i = 0; i < sizeof digest; i++) sprintf(out + i * 2, "%02x", digest[i]);
    return true;
}

bool tezos_beacon_secretbox_encrypt(const char *message, const uint8_t tx_key[32],
                                    char *hex_out, size_t hex_out_cap) {
    size_t msg_len = strlen(message);
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof nonce);

    size_t ciphertext_len = crypto_secretbox_MACBYTES + msg_len;
    uint8_t *ciphertext = malloc(ciphertext_len);
    if (!ciphertext) return false;
    if (crypto_secretbox_easy(ciphertext, (const uint8_t *)message, msg_len, nonce, tx_key) != 0) {
        free(ciphertext);
        return false;
    }

    size_t combined_len = sizeof(nonce) + ciphertext_len;
    if (hex_out_cap < combined_len * 2 + 1) { free(ciphertext); return false; }
    for (size_t i = 0; i < sizeof nonce; i++) sprintf(hex_out + i * 2, "%02x", nonce[i]);
    for (size_t i = 0; i < ciphertext_len; i++) sprintf(hex_out + (sizeof(nonce) + i) * 2, "%02x", ciphertext[i]);
    free(ciphertext);
    return true;
}

bool tezos_beacon_secretbox_decrypt(const char *hex_payload, const uint8_t rx_key[32],
                                    char *message_out, size_t message_out_cap) {
    size_t hex_len = strlen(hex_payload);
    if (hex_len % 2 != 0) return false;
    size_t raw_len = hex_len / 2;
    if (raw_len < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) return false;

    uint8_t *raw = malloc(raw_len);
    if (!raw) return false;
    if (sodium_hex2bin(raw, raw_len, hex_payload, hex_len, NULL, NULL, NULL) != 0) { free(raw); return false; }

    const uint8_t *nonce = raw;
    const uint8_t *ciphertext = raw + crypto_secretbox_NONCEBYTES;
    size_t ciphertext_len = raw_len - crypto_secretbox_NONCEBYTES;
    size_t plaintext_len = ciphertext_len - crypto_secretbox_MACBYTES;

    if (plaintext_len + 1 > message_out_cap) { free(raw); return false; }
    if (crypto_secretbox_open_easy((uint8_t *)message_out, ciphertext, ciphertext_len, nonce, rx_key) != 0) {
        free(raw);
        return false;
    }
    message_out[plaintext_len] = '\0';
    free(raw);
    return true;
}
