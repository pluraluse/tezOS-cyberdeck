/* Operation forging -- verified byte-for-byte against Taquito's exact
   reference forging output for a real transaction, plus a Michelson
   entrypoint-call variant using the real Micheline encoder. */
#include "../src/chain/tezos_forge.h"
#include "../src/chain/tezos_micheline.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    tezos_transaction_params_t p = {0};
    strcpy(p.source, "tz1gvF4cD2dDtqitL3ZTraggSR1Mju2BKFEM");
    strcpy(p.fee, "374"); strcpy(p.counter, "1"); strcpy(p.gas_limit, "1000");
    strcpy(p.storage_limit, "0"); strcpy(p.amount, "1000000");
    strcpy(p.destination, "tz1gvF4cD2dDtqitL3ZTraggSR1Mju2BKFEM");

    tezos_bytes_t content; tezos_bytes_init(&content);
    CHECK(tezos_forge_transaction(&content, &p), "forge_transaction returns true");
    char hex[512]; tezos_bytes_to_hex(&content, hex, sizeof hex);
    const char *expected = "6c00e96b9f8b19af9c7ffa0c0480e1977b295850961ff60201e80700c0843d0000e96b9f8b19af9c7ffa0c0480e1977b295850961f00";
    CHECK(strcmp(hex, expected) == 0, "plain transfer matches Taquito's exact golden vector byte-for-byte");
    tezos_bytes_free(&content);

    tezos_bytes_t full; tezos_bytes_init(&full);
    CHECK(tezos_forge_operation_group(&full, "BLzyjjHKEKMULtvkpSHxuZxx6ei6fpntH2BTkYZiLgs8zLVstvX", content.data, 0), "op group (empty content) returns true");
    tezos_bytes_free(&full);

    /* re-forge to get content back (freed above) for the group test */
    tezos_bytes_init(&content);
    tezos_forge_transaction(&content, &p);
    tezos_bytes_init(&full);
    CHECK(tezos_forge_operation_group(&full, "BLzyjjHKEKMULtvkpSHxuZxx6ei6fpntH2BTkYZiLgs8zLVstvX", content.data, content.len),
          "full operation group forges successfully");
    char full_hex[1024]; tezos_bytes_to_hex(&full, full_hex, sizeof full_hex);
    const char *expected_full = "a99b946c97ada0f42c1bdeae0383db7893351232a832d00d0cd716eb6f66e5616c00e96b9f8b19af9c7ffa0c0480e1977b295850961ff60201e80700c0843d0000e96b9f8b19af9c7ffa0c0480e1977b295850961f00";
    CHECK(strcmp(full_hex, expected_full) == 0, "full forged operation matches Taquito's exact golden vector");
    tezos_bytes_free(&content); tezos_bytes_free(&full);

    /* entrypoint call, using the real Micheline encoder */
    tezos_micheline_node_t token_id = tezos_micheline_int("4021");
    p.entrypoint = "collect";
    p.parameter_value = &token_id;
    tezos_bytes_init(&content);
    CHECK(tezos_forge_transaction(&content, &p), "entrypoint-call transaction forges successfully");
    tezos_bytes_to_hex(&content, hex, sizeof hex);
    CHECK(strstr(hex, "ff07636f6c6c656374") != NULL, "forged transaction contains the real entrypoint framing, not the plain-transfer shortcut");
    tezos_bytes_free(&content);

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
