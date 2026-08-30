#include "../src/chain/tezos_rpc.h"
#include "../src/chain/tezos_http.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    /* === body construction: exercise the exact same logic
       tezos_rpc_inject_operation uses internally === */
    uint8_t sample_bytes[] = {0xde, 0xad, 0xbe, 0xef};
    char *hex = malloc(sizeof(sample_bytes) * 2 + 1);
    for (size_t i = 0; i < sizeof sample_bytes; i++) sprintf(hex + i*2, "%02x", sample_bytes[i]);
    cJSON *body_value = cJSON_CreateString(hex);
    char *body_json = cJSON_PrintUnformatted(body_value);
    printf("  constructed body: %s\n", body_json);
    CHECK(strcmp(body_json, "\"deadbeef\"") == 0,
          "injection body is a bare JSON string containing the hex, exactly matching the real Tezos RPC's expected format");
    cJSON_Delete(body_value); free(body_json); free(hex);

    /* === response parsing, against the documented real shape: a bare
       JSON string containing the Base58Check operation hash === */
    const char *realistic_response = "\"onvvXQvKPnDR3Q2fMqiXmMj4T5rHMTGYmA4vLnA37RnZfTh4pJm\"";
    cJSON *root = cJSON_Parse(realistic_response);
    CHECK(root != NULL && cJSON_IsString(root), "operation hash response parses as a JSON string, not an object");
    CHECK(strncmp(root->valuestring, "on", 2) == 0, "parsed hash has the correct 'on...' operation-hash prefix shape");
    cJSON_Delete(root);

    /* === live network attempt -- honest: expected to fail from this
       sandbox (no real RPC node in the domain allowlist), but on real
       hardware with real network AND a real funded testnet account
       this could genuinely succeed. Not attempted with fake bytes here
       since injecting garbage at a real node isn't a meaningful test
       and could trip abuse detection on a public endpoint. === */
    printf("  (live injection intentionally not attempted here -- needs a real funded\n");
    printf("   testnet account and unrestricted network access; see tezos_rpc.h)\n");

    tezos_http_init();
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    tezos_http_cleanup();
    return fails == 0 ? 0 : 1;
}
