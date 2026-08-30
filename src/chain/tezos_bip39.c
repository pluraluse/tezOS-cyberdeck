#define _POSIX_C_SOURCE 200809L
#include "tezos_bip39.h"
#include "tezos_bip39_wordlist.h"
#include <sodium.h>
#include <string.h>
#include <stdlib.h>

/* --- bit-level helpers: BIP39 packs 11-bit word indices across byte
   boundaries, so this needs actual bit manipulation, not byte-aligned
   copies --- */

static void set_bits(uint8_t *buf, size_t bit_offset, uint32_t value, int nbits) {
    for (int i = 0; i < nbits; i++) {
        size_t bit = bit_offset + i;
        int val_bit = (value >> (nbits - 1 - i)) & 1;
        if (val_bit) buf[bit / 8] |= (uint8_t)(0x80 >> (bit % 8));
    }
}

static uint32_t get_bits(const uint8_t *buf, size_t bit_offset, int nbits) {
    uint32_t v = 0;
    for (int i = 0; i < nbits; i++) {
        size_t bit = bit_offset + i;
        int val_bit = (buf[bit / 8] >> (7 - (bit % 8))) & 1;
        v = (v << 1) | (uint32_t)val_bit;
    }
    return v;
}

static int wordlist_index_of(const char *word) {
    /* Linear scan — 2048 entries, called a handful of times per
       validate() call, not a hot path. A sorted/binary-search table
       would be a reasonable optimization later if this ever needs to
       run somewhere more constrained than it does today. */
    for (int i = 0; i < 2048; i++) {
        if (strcmp(TEZOS_BIP39_WORDLIST[i], word) == 0) return i;
    }
    return -1;
}

bool tezos_bip39_entropy_to_mnemonic(const uint8_t *entropy, size_t entropy_len,
                                     char *mnemonic_out, size_t out_cap) {
    if (entropy_len != TEZOS_BIP39_ENTROPY_BYTES_12WORD &&
        entropy_len != TEZOS_BIP39_ENTROPY_BYTES_24WORD) return false;

    int entropy_bits = (int)entropy_len * 8;
    int checksum_bits = entropy_bits / 32;
    int total_bits = entropy_bits + checksum_bits;
    int word_count = total_bits / 11;

    uint8_t checksum_hash[32];
    crypto_hash_sha256(checksum_hash, entropy, entropy_len);

    /* combined = entropy bytes followed by the checksum's leading bits,
       packed into a buffer big enough for entropy + 1 checksum byte */
    uint8_t combined[TEZOS_BIP39_ENTROPY_BYTES_24WORD + 1] = {0};
    memcpy(combined, entropy, entropy_len);
    combined[entropy_len] = checksum_hash[0]; /* only the top checksum_bits of this get used */

    size_t pos = 0;
    for (int w = 0; w < word_count; w++) {
        uint32_t idx = get_bits(combined, (size_t)w * 11, 11);
        const char *word = TEZOS_BIP39_WORDLIST[idx];
        size_t wlen = strlen(word);
        if (pos + wlen + 2 > out_cap) return false; /* +1 space, +1 nul */
        if (w > 0) mnemonic_out[pos++] = ' ';
        memcpy(mnemonic_out + pos, word, wlen);
        pos += wlen;
    }
    mnemonic_out[pos] = '\0';
    return true;
}

bool tezos_bip39_validate(const char *mnemonic, int word_count) {
    if (word_count != 12 && word_count != 24) return false;
    int total_bits = word_count * 11;
    int checksum_bits = (word_count == 12) ? 4 : 8;
    int entropy_bits = total_bits - checksum_bits;
    int entropy_bytes = entropy_bits / 8;

    uint8_t combined[TEZOS_BIP39_ENTROPY_BYTES_24WORD + 1] = {0};

    char buf[TEZOS_BIP39_MAX_MNEMONIC_LEN];
    strncpy(buf, mnemonic, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';

    int found_words = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(buf, " ", &saveptr);
    while (tok && found_words < word_count) {
        int idx = wordlist_index_of(tok);
        if (idx < 0) return false; /* word not in wordlist -- likely a typo */
        set_bits(combined, (size_t)found_words * 11, (uint32_t)idx, 11);
        found_words++;
        tok = strtok_r(NULL, " ", &saveptr);
    }
    if (found_words != word_count || tok != NULL) return false; /* wrong word count */

    uint8_t entropy[TEZOS_BIP39_ENTROPY_BYTES_24WORD];
    memcpy(entropy, combined, (size_t)entropy_bytes);

    uint8_t hash[32];
    crypto_hash_sha256(hash, entropy, (size_t)entropy_bytes);

    uint32_t expected_checksum = get_bits(combined, (size_t)entropy_bits, checksum_bits);
    uint32_t actual_checksum = get_bits(hash, 0, checksum_bits);
    return expected_checksum == actual_checksum;
}

/* PBKDF2-HMAC-SHA512, output exactly 64 bytes (one block) — that's all
   BIP39 needs, so no multi-block T1||T2||... logic required. */
bool tezos_bip39_to_seed(const char *mnemonic, const char *passphrase, uint8_t seed_out[64]) {
    char salt[128];
    int n = snprintf(salt, sizeof salt, "mnemonic%s", passphrase ? passphrase : "");
    if (n < 0 || (size_t)n >= sizeof salt) return false;

    size_t mnemonic_len = strlen(mnemonic);
    size_t salt_len = strlen(salt);

    /* U1 = HMAC(password=mnemonic, salt || INT_32_BE(1)) */
    uint8_t block_input[128 + 4];
    if (salt_len + 4 > sizeof block_input) return false;
    memcpy(block_input, salt, salt_len);
    block_input[salt_len + 0] = 0x00;
    block_input[salt_len + 1] = 0x00;
    block_input[salt_len + 2] = 0x00;
    block_input[salt_len + 3] = 0x01;

    uint8_t u[64], t[64];
    crypto_auth_hmacsha512_state st;
    crypto_auth_hmacsha512_init(&st, (const uint8_t *)mnemonic, mnemonic_len);
    crypto_auth_hmacsha512_update(&st, block_input, salt_len + 4);
    crypto_auth_hmacsha512_final(&st, u);
    memcpy(t, u, 64);

    for (int i = 1; i < 2048; i++) {
        crypto_auth_hmacsha512_init(&st, (const uint8_t *)mnemonic, mnemonic_len);
        crypto_auth_hmacsha512_update(&st, u, 64);
        crypto_auth_hmacsha512_final(&st, u);
        for (int b = 0; b < 64; b++) t[b] ^= u[b];
    }
    memcpy(seed_out, t, 64);
    return true;
}
