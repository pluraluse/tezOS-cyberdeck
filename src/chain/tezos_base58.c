#include "tezos_base58.h"
#include <sodium.h>
#include <string.h>
#include <stdlib.h>

static const char B58_ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

typedef struct { const uint8_t *bytes; size_t len; } prefix_bytes_t;

static prefix_bytes_t prefix_bytes(tezos_b58_prefix_t p, size_t *payload_len) {
    static const uint8_t TZ1[]  = {6, 161, 159};
    static const uint8_t TZ2[]  = {6, 161, 161};
    static const uint8_t TZ3[]  = {6, 161, 164};
    static const uint8_t KT1[]  = {2, 90, 121};
    static const uint8_t EDPK[] = {13, 15, 37, 217};
    static const uint8_t EDSK[] = {13, 15, 58, 7};
    static const uint8_t EDSIG[]= {9, 245, 205, 134, 18};
    static const uint8_t SIG[]  = {4, 130, 43};
    static const uint8_t BLOCK[]= {1, 52};

    switch (p) {
        case TEZOS_PREFIX_TZ1:        *payload_len = 20; return (prefix_bytes_t){TZ1, sizeof TZ1};
        case TEZOS_PREFIX_TZ2:        *payload_len = 20; return (prefix_bytes_t){TZ2, sizeof TZ2};
        case TEZOS_PREFIX_TZ3:        *payload_len = 20; return (prefix_bytes_t){TZ3, sizeof TZ3};
        case TEZOS_PREFIX_KT1:        *payload_len = 20; return (prefix_bytes_t){KT1, sizeof KT1};
        case TEZOS_PREFIX_EDPK:       *payload_len = 32; return (prefix_bytes_t){EDPK, sizeof EDPK};
        case TEZOS_PREFIX_EDSK_SEED:  *payload_len = 32; return (prefix_bytes_t){EDSK, sizeof EDSK};
        case TEZOS_PREFIX_EDSIG:      *payload_len = 64; return (prefix_bytes_t){EDSIG, sizeof EDSIG};
        case TEZOS_PREFIX_SIG:        *payload_len = 64; return (prefix_bytes_t){SIG, sizeof SIG};
        case TEZOS_PREFIX_BLOCK_HASH: *payload_len = 32; return (prefix_bytes_t){BLOCK, sizeof BLOCK};
    }
    *payload_len = 0;
    return (prefix_bytes_t){NULL, 0};
}

/* Big-endian byte array <-> base58, arbitrary precision via repeated
   division. Straightforward, not performance-critical (called per
   address, not per-frame). */
static bool b58_encode_raw(const uint8_t *data, size_t len, char *out, size_t out_cap) {
    /* count leading zero bytes -> each becomes a leading '1' */
    size_t zeros = 0;
    while (zeros < len && data[zeros] == 0) zeros++;

    /* big-endian base256 -> base58 via repeated division, using a
       temporary big-endian digit buffer sized generously */
    size_t buf_size = len * 138 / 100 + 1; /* log(256)/log(58), rounded up */
    uint8_t *buf = calloc(buf_size, 1);
    if (!buf) return false;

    size_t buf_end = buf_size;
    for (size_t i = zeros; i < len; i++) {
        int carry = data[i];
        size_t j = buf_size;
        while (j > 0 && (carry != 0 || j > buf_end)) {
            j--;
            carry += 256 * buf[j];
            buf[j] = carry % 58;
            carry /= 58;
            if (j < buf_end) buf_end = j;
        }
    }
    /* skip leading zero digits in the base58 buffer itself */
    size_t digit_start = buf_end;
    while (digit_start < buf_size && buf[digit_start] == 0) digit_start++;

    size_t total_len = zeros + (buf_size - digit_start);
    if (total_len + 1 > out_cap) { free(buf); return false; }

    size_t pos = 0;
    for (size_t i = 0; i < zeros; i++) out[pos++] = '1';
    for (size_t i = digit_start; i < buf_size; i++) out[pos++] = B58_ALPHABET[buf[i]];
    out[pos] = '\0';
    free(buf);
    return true;
}

static bool b58_decode_raw(const char *s, uint8_t *out, size_t *out_len, size_t out_cap) {
    size_t s_len = strlen(s);
    size_t zeros = 0;
    while (zeros < s_len && s[zeros] == '1') zeros++;

    size_t buf_size = s_len * 733 / 1000 + 1; /* log(58)/log(256), rounded up */
    uint8_t *buf = calloc(buf_size, 1);
    if (!buf) return false;
    size_t buf_end = buf_size;

    for (size_t i = zeros; i < s_len; i++) {
        const char *p = strchr(B58_ALPHABET, s[i]);
        if (!p) { free(buf); return false; }
        int carry = (int)(p - B58_ALPHABET);
        size_t j = buf_size;
        while (j > 0 && (carry != 0 || j > buf_end)) {
            j--;
            carry += 58 * buf[j];
            buf[j] = carry % 256;
            carry /= 256;
            if (j < buf_end) buf_end = j;
        }
    }
    size_t byte_start = buf_end;
    while (byte_start < buf_size && buf[byte_start] == 0) byte_start++;

    size_t total_len = zeros + (buf_size - byte_start);
    if (total_len > out_cap) { free(buf); return false; }

    size_t pos = 0;
    for (size_t i = 0; i < zeros; i++) out[pos++] = 0;
    for (size_t i = byte_start; i < buf_size; i++) out[pos++] = buf[i];
    *out_len = total_len;
    free(buf);
    return true;
}

bool tezos_b58_decode(const char *input, tezos_b58_prefix_t expected,
                      uint8_t *out, size_t *out_len, size_t out_cap) {
    uint8_t decoded[256];
    size_t decoded_len = 0;
    if (!b58_decode_raw(input, decoded, &decoded_len, sizeof decoded)) return false;
    if (decoded_len < 4) return false; /* need at least a 4-byte checksum */

    size_t payload_and_prefix_len = decoded_len - 4;
    const uint8_t *checksum = decoded + payload_and_prefix_len;

    uint8_t hash1[32], hash2[32];
    crypto_hash_sha256(hash1, decoded, payload_and_prefix_len);
    crypto_hash_sha256(hash2, hash1, 32);
    if (memcmp(hash2, checksum, 4) != 0) return false; /* bad checksum */

    size_t expected_payload_len;
    prefix_bytes_t pfx = prefix_bytes(expected, &expected_payload_len);
    if (pfx.len == 0 || payload_and_prefix_len != pfx.len + expected_payload_len) return false;
    if (memcmp(decoded, pfx.bytes, pfx.len) != 0) return false; /* wrong prefix */

    if (expected_payload_len > out_cap) return false;
    memcpy(out, decoded + pfx.len, expected_payload_len);
    *out_len = expected_payload_len;
    return true;
}

bool tezos_b58_encode(const uint8_t *payload, size_t payload_len,
                      tezos_b58_prefix_t prefix, char *out, size_t out_cap) {
    size_t expected_payload_len;
    prefix_bytes_t pfx = prefix_bytes(prefix, &expected_payload_len);
    if (pfx.len == 0 || payload_len != expected_payload_len) return false;

    uint8_t buf[256];
    if (pfx.len + payload_len + 4 > sizeof buf) return false;
    memcpy(buf, pfx.bytes, pfx.len);
    memcpy(buf + pfx.len, payload, payload_len);

    uint8_t hash1[32], hash2[32];
    crypto_hash_sha256(hash1, buf, pfx.len + payload_len);
    crypto_hash_sha256(hash2, hash1, 32);
    memcpy(buf + pfx.len + payload_len, hash2, 4);

    return b58_encode_raw(buf, pfx.len + payload_len + 4, out, out_cap);
}
