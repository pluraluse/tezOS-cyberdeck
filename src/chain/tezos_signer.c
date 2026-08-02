#include "tezos_signer.h"
#include "tezos_base58.h"
#include <sodium.h>
#include <string.h>

bool tezos_mock_signer_init(tezos_mock_signer_t *signer, const char *edsk_seed_b58) {
    size_t out_len;
    if (!tezos_b58_decode(edsk_seed_b58, TEZOS_PREFIX_EDSK_SEED, signer->seed, &out_len, sizeof signer->seed)
        || out_len != 32) return false;
    crypto_sign_seed_keypair(signer->public_key, signer->secret_key, signer->seed);
    return true;
}

void tezos_mock_signer_generate(tezos_mock_signer_t *signer) {
    randombytes_buf(signer->seed, sizeof signer->seed);
    crypto_sign_seed_keypair(signer->public_key, signer->secret_key, signer->seed);
}

bool tezos_mock_signer_public_key_b58(const tezos_mock_signer_t *signer, char *out, size_t out_cap) {
    return tezos_b58_encode(signer->public_key, 32, TEZOS_PREFIX_EDPK, out, out_cap);
}

bool tezos_mock_signer_address_b58(const tezos_mock_signer_t *signer, char *out, size_t out_cap) {
    /* public key hash = blake2b(pubkey, 20-byte digest), tz1-encoded —
       confirmed against Taquito's EdPublicKey.hash() implementation */
    uint8_t hash[20];
    crypto_generichash(hash, sizeof hash, signer->public_key, 32, NULL, 0);
    return tezos_b58_encode(hash, sizeof hash, TEZOS_PREFIX_TZ1, out, out_cap);
}

bool tezos_mock_signer_sign(const tezos_mock_signer_t *signer, tezos_bytes_t *forged_bytes,
                            char *sig_b58_out, size_t sig_b58_cap) {
    /* watermark (0x03, generic operation) + forged bytes, then
       blake2b-256, then ed25519-sign the hash — confirmed against
       Taquito's provider.ts (watermark) and ed-key.ts (hash+sign) */
    uint8_t *watermarked = malloc(forged_bytes->len + 1);
    if (!watermarked) return false;
    watermarked[0] = 0x03;
    memcpy(watermarked + 1, forged_bytes->data, forged_bytes->len);

    uint8_t hash[32];
    crypto_generichash(hash, sizeof hash, watermarked, forged_bytes->len + 1, NULL, 0);
    free(watermarked);

    uint8_t signature[64];
    if (crypto_sign_detached(signature, NULL, hash, sizeof hash, signer->secret_key) != 0) return false;

    if (sig_b58_out && !tezos_b58_encode(signature, sizeof signature, TEZOS_PREFIX_SIG, sig_b58_out, sig_b58_cap))
        return false;

    tezos_bytes_append(forged_bytes, signature, sizeof signature);
    return true;
}

bool tezos_mock_signer_self_verify(const tezos_mock_signer_t *signer,
                                   const uint8_t *signed_bytes, size_t signed_len) {
    if (signed_len < 64) return false;
    size_t unsigned_len = signed_len - 64;
    const uint8_t *signature = signed_bytes + unsigned_len;

    uint8_t *watermarked = malloc(unsigned_len + 1);
    if (!watermarked) return false;
    watermarked[0] = 0x03;
    memcpy(watermarked + 1, signed_bytes, unsigned_len);

    uint8_t hash[32];
    crypto_generichash(hash, sizeof hash, watermarked, unsigned_len + 1, NULL, 0);
    free(watermarked);

    return crypto_sign_verify_detached(signature, hash, sizeof hash, signer->public_key) == 0;
}
