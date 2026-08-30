/* Base58Check encode/decode -- verified against a real Taquito golden
   vector (@taquito/utils encoding.ts docstring example), both
   directions, plus KT1 round-trip. */
#include "../src/chain/tezos_base58.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    const char *addr = "tz1gvF4cD2dDtqitL3ZTraggSR1Mju2BKFEM";
    const char *expected_hash = "e96b9f8b19af9c7ffa0c0480e1977b295850961f";

    uint8_t out[64]; size_t out_len;
    CHECK(tezos_b58_decode(addr, TEZOS_PREFIX_TZ1, out, &out_len, sizeof out), "decode returns true");
    char hex[128] = {0};
    for (size_t i = 0; i < out_len; i++) sprintf(hex + i*2, "%02x", out[i]);
    CHECK(out_len == 20 && strcmp(hex, expected_hash) == 0, "decode matches Taquito golden vector exactly");

    char reencoded[TEZOS_B58_MAX_STRLEN];
    CHECK(tezos_b58_encode(out, out_len, TEZOS_PREFIX_TZ1, reencoded, sizeof reencoded), "encode returns true");
    CHECK(strcmp(reencoded, addr) == 0, "round-trip encode matches original address exactly");

    const char *kt1 = "KT1BEqzn5Wx8uJrZNvuS9DVHmLvG9td3fDLi";
    CHECK(tezos_b58_decode(kt1, TEZOS_PREFIX_KT1, out, &out_len, sizeof out), "KT1 decode returns true");
    char kt1_reenc[TEZOS_B58_MAX_STRLEN];
    tezos_b58_encode(out, out_len, TEZOS_PREFIX_KT1, kt1_reenc, sizeof kt1_reenc);
    CHECK(strcmp(kt1_reenc, kt1) == 0, "KT1 round-trip OK");

    /* generic prefix-less bs58check, used by Beacon pairing payloads */
    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03};
    char generic_enc[128];
    CHECK(tezos_b58check_encode_generic(payload, sizeof payload, generic_enc, sizeof generic_enc),
          "generic bs58check encode succeeds");
    uint8_t generic_dec[64]; size_t generic_dec_len;
    CHECK(tezos_b58check_decode_generic(generic_enc, generic_dec, &generic_dec_len, sizeof generic_dec)
          && generic_dec_len == sizeof payload && memcmp(generic_dec, payload, sizeof payload) == 0,
          "generic bs58check round-trip matches exactly");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
