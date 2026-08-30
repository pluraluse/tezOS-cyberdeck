/* Micheline encoding -- verified against two exact Taquito test
   vectors, plus 12 signed-zarith cross-checks against an independent
   Python port. */
#include "../src/chain/tezos_micheline.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void check_hex(tezos_bytes_t *b, const char *expected, const char *msg) {
    char hex[512]; tezos_bytes_to_hex(b, hex, sizeof hex);
    CHECK(strcmp(hex, expected) == 0, msg);
}

int main(void) {
    uint8_t raw[] = {0x02, 0x02};
    tezos_micheline_node_t b = tezos_micheline_bytes(raw, sizeof raw);
    tezos_bytes_t out; tezos_bytes_init(&out);
    tezos_micheline_encode(&b, &out);
    check_hex(&out, "0a000000020202", "bytes value matches Taquito's exact golden vector");
    tezos_bytes_free(&out);

    uint8_t raw2[] = {0x04, 0x05};
    tezos_micheline_node_t b1 = tezos_micheline_bytes(raw, sizeof raw);
    tezos_micheline_node_t b2 = tezos_micheline_bytes(raw2, sizeof raw2);
    tezos_micheline_node_t pair = tezos_micheline_pair(&b1, &b2);
    tezos_bytes_init(&out);
    tezos_micheline_encode(&pair, &out);
    check_hex(&out, "07070a0000000202020a000000020405", "Pair(bytes,bytes) matches Taquito's exact golden vector");
    tezos_bytes_free(&out);

    struct { const char *dec; const char *hex; } int_vectors[] = {
        {"0", "0000"}, {"10", "000a"}, {"100", "00a401"}, {"123456", "0080890f"},
        {"-123654", "00c68c0f"}, {"127", "00bf01"}, {"128", "008002"},
        {"-1", "0041"}, {"-64", "00c001"}, {"-65", "00c101"},
        {"4096", "008040"}, {"-4096", "00c040"},
    };
    for (size_t i = 0; i < sizeof(int_vectors)/sizeof(int_vectors[0]); i++) {
        tezos_micheline_node_t n = tezos_micheline_int(int_vectors[i].dec);
        tezos_bytes_init(&out);
        tezos_micheline_encode(&n, &out);
        char hex[64]; tezos_bytes_to_hex(&out, hex, sizeof hex);
        char msg[128]; snprintf(msg, sizeof msg, "signed zarith int(%s) matches Python cross-check", int_vectors[i].dec);
        CHECK(strcmp(hex, int_vectors[i].hex) == 0, msg);
        tezos_bytes_free(&out);
    }

    tezos_bytes_init(&out);
    CHECK(tezos_forge_parameters(&out, NULL, NULL), "default/NULL params returns true");
    check_hex(&out, "00", "default+Unit correctly uses the single-byte 0x00 shortcut");
    tezos_bytes_free(&out);

    tezos_micheline_node_t token_id = tezos_micheline_int("4021");
    tezos_bytes_init(&out);
    CHECK(tezos_forge_parameters(&out, "collect", &token_id), "forge_parameters for a real custom entrypoint returns true");
    char hex[64]; tezos_bytes_to_hex(&out, hex, sizeof hex);
    CHECK(strncmp(hex, "ff07636f6c6c656374", 18) == 0, "entrypoint framing correct: 0xff + len(7) + \"collect\" in hex");
    tezos_bytes_free(&out);

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
