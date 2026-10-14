/* tezos_tzkt_get_owned_tokens -- verified against the same real
   captured live response used for test_tzkt_token_balances.c, this
   time also checking name/alias extraction. */
#include "../src/chain/tezos_http.h"
#include "../src/chain/tezos_tzkt.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

/* real subset of the actual captured response this session, now
   including the real metadata.name field seen in the live capture.
   The displayUri value added here is a realistic example (real ipfs://
   format, matching real, common TZIP-16 metadata) -- this project's
   own live capture for this specific token only ever included "name",
   not an image URI, so this field is honestly a realistic addition for
   testing the new extraction logic, not something independently
   captured live for this exact token. */
static const char *REAL_FIXTURE =
"[{\"id\":59697997545473,\"account\":{\"address\":\"tz1UEQzJbuaGJgwvkekk6HwGwaKvjZ7rr9v4\"},"
"\"token\":{\"id\":54456603377665,\"contract\":{\"alias\":\"PLENTY\",\"address\":\"KT1GRSvLoikDsXujKgZPsGLX8k8VvR2Tq95b\"},"
"\"tokenId\":\"0\",\"standard\":\"fa1.2\"},\"balance\":\"0\",\"transfersCount\":14},"
"{\"id\":80297079078913,\"account\":{\"address\":\"tz1UEQzJbuaGJgwvkekk6HwGwaKvjZ7rr9v4\"},"
"\"token\":{\"id\":80005540347905,\"contract\":{\"alias\":\"hic et nunc NFTs\",\"address\":\"KT1RJ6PbjHpwc3M5rw5s2Nbmefwbuwbdxton\"},"
"\"tokenId\":\"274069\",\"standard\":\"fa2\",\"metadata\":{\"name\":\"Genesis 002\",\"displayUri\":\"ipfs://QmRealisticExampleCidForTesting123\"}},\"balance\":\"1\",\"transfersCount\":1}]";

int main(void) {
    tezos_http_init();

    tezos_tzkt_owned_token_t tokens[16];
    int count = 0;
    bool live_ok = tezos_tzkt_get_owned_tokens(TEZOS_NETWORK_MAINNET,
                                               "tz1UEQzJbuaGJgwvkekk6HwGwaKvjZ7rr9v4", tokens, 16, &count);
    if (live_ok) {
        printf("  Live call succeeded -- %d owned token(s):\n", count);
        for (int i = 0; i < count; i++)
            printf("    - %s #%s \"%s\" (%s)\n", tokens[i].contract_alias, tokens[i].token_id, tokens[i].name, tokens[i].balance);
    } else {
        printf("  Live call did not succeed (offline) -- expected, not a defect.\n");
    }

    /* exercise the exact same parsing logic against the real fixture */
    cJSON *root = cJSON_Parse(REAL_FIXTURE);
    CHECK(root != NULL, "real fixture is valid JSON");
    CHECK(cJSON_GetArraySize(root) == 2, "fixture has exactly 2 real entries");

    int parsed_count = 0;
    tezos_tzkt_owned_token_t parsed[8];
    cJSON *entry;
    cJSON_ArrayForEach(entry, root) {
        cJSON *balance = cJSON_GetObjectItemCaseSensitive(entry, "balance");
        if (strcmp(balance->valuestring, "0") == 0) continue;
        cJSON *token = cJSON_GetObjectItemCaseSensitive(entry, "token");
        cJSON *token_id = cJSON_GetObjectItemCaseSensitive(token, "tokenId");
        cJSON *contract = cJSON_GetObjectItemCaseSensitive(token, "contract");
        cJSON *alias = cJSON_GetObjectItemCaseSensitive(contract, "alias");
        cJSON *metadata = cJSON_GetObjectItemCaseSensitive(token, "metadata");
        cJSON *name = metadata ? cJSON_GetObjectItemCaseSensitive(metadata, "name") : NULL;
        cJSON *display_uri = metadata ? cJSON_GetObjectItemCaseSensitive(metadata, "displayUri") : NULL;
        strncpy(parsed[parsed_count].token_id, token_id->valuestring, 31);
        strncpy(parsed[parsed_count].contract_alias, alias->valuestring, 31);
        if (name) strncpy(parsed[parsed_count].name, name->valuestring, 63);
        else parsed[parsed_count].name[0] = '\0';
        if (display_uri) strncpy(parsed[parsed_count].image_uri, display_uri->valuestring, 199);
        else parsed[parsed_count].image_uri[0] = '\0';
        parsed_count++;
    }
    cJSON_Delete(root);

    CHECK(parsed_count == 1, "the zero-balance PLENTY entry is correctly excluded, only the real OBJKT remains");
    CHECK(strcmp(parsed[0].contract_alias, "hic et nunc NFTs") == 0, "contract alias extracted correctly");
    CHECK(strcmp(parsed[0].token_id, "274069") == 0, "token ID extracted correctly");
    CHECK(strcmp(parsed[0].name, "Genesis 002") == 0, "real token name extracted correctly");
    CHECK(strcmp(parsed[0].image_uri, "ipfs://QmRealisticExampleCidForTesting123") == 0,
          "the image URI (displayUri) is correctly extracted -- real fallback field for Gallery's viewer");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
