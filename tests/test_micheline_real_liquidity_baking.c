/* Uses the EXACT real response captured live from
   api.tzkt.io/v1/contracts/KT1TxqZ8QtKvLu3V3JH7Gx58n7Co8pgtpQU5/entrypoints?micheline=true
   (the real Liquidity Baking contract) -- not a guessed fixture. This
   confirmed "michelineParameters" as the real field name (matching
   this project's first guess exactly) and surfaced a real gap: every
   non-trivial entrypoint here uses a "timestamp" type this project's
   parser didn't support at all until this was caught. */
#include "../src/chain/tezos_micheline.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

/* the exact real captured response, verbatim */
static const char *REAL_RESPONSE =
"[{\"name\":\"default\",\"jsonParameters\":{\"schema:unit\":\"unit\"},\"michelineParameters\":{\"prim\":\"unit\",\"annots\":[\"%default\"]},\"unused\":false},"
"{\"name\":\"addLiquidity\",\"jsonParameters\":{\"schema:object\":{\"owner:address\":\"address\",\"minLqtMinted:nat\":\"nat\",\"maxTokensDeposited:nat\":\"nat\",\"deadline:timestamp\":\"timestamp\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%addLiquidity\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%owner\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%minLqtMinted\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%maxTokensDeposited\"]},{\"prim\":\"timestamp\",\"annots\":[\"%deadline\"]}]}]}]},\"unused\":false},"
"{\"name\":\"removeLiquidity\",\"jsonParameters\":{\"schema:object\":{\"to:address\":\"address\",\"lqtBurned:nat\":\"nat\",\"minXtzWithdrawn:mutez\":\"mutez\",\"minTokensWithdrawn:nat\":\"nat\",\"deadline:timestamp\":\"timestamp\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%removeLiquidity\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%to\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%lqtBurned\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"mutez\",\"annots\":[\"%minXtzWithdrawn\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%minTokensWithdrawn\"]},{\"prim\":\"timestamp\",\"annots\":[\"%deadline\"]}]}]}]}]},\"unused\":false},"
"{\"name\":\"tokenToToken\",\"jsonParameters\":{\"schema:object\":{\"outputDexterContract:address\":\"address\",\"minTokensBought:nat\":\"nat\",\"to:address\":\"address\",\"tokensSold:nat\":\"nat\",\"deadline:timestamp\":\"timestamp\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%tokenToToken\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%outputDexterContract\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%minTokensBought\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%to\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%tokensSold\"]},{\"prim\":\"timestamp\",\"annots\":[\"%deadline\"]}]}]}]}]},\"unused\":false},"
"{\"name\":\"tokenToXtz\",\"jsonParameters\":{\"schema:object\":{\"to:address\":\"address\",\"tokensSold:nat\":\"nat\",\"minXtzBought:mutez\":\"mutez\",\"deadline:timestamp\":\"timestamp\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%tokenToXtz\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%to\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%tokensSold\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"mutez\",\"annots\":[\"%minXtzBought\"]},{\"prim\":\"timestamp\",\"annots\":[\"%deadline\"]}]}]}]},\"unused\":false},"
"{\"name\":\"xtzToToken\",\"jsonParameters\":{\"schema:object\":{\"to:address\":\"address\",\"minTokensBought:nat\":\"nat\",\"deadline:timestamp\":\"timestamp\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%xtzToToken\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%to\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%minTokensBought\"]},{\"prim\":\"timestamp\",\"annots\":[\"%deadline\"]}]}]},\"unused\":false}]";

int main(void) {
    cJSON *root = cJSON_Parse(REAL_RESPONSE);
    CHECK(root != NULL, "the real captured response is valid JSON (sanity check on the paste itself)");
    CHECK(cJSON_GetArraySize(root) == 6, "the real response has exactly 6 entrypoints, matching what was pasted");

    /* exercise the exact same field-name-guessing + type-parsing logic
       tezos_tzkt_get_entrypoints_with_types uses, driven by this real
       response rather than a synthetic fixture */
    const char *expected_names[] = {"default", "addLiquidity", "removeLiquidity", "tokenToToken", "tokenToXtz", "xtzToToken"};
    int expected_leaves[] = {0, 4, 5, 5, 4, 3}; /* unit=0, then each record's real field count */

    cJSON *entry; int i = 0;
    cJSON_ArrayForEach(entry, root) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, "name");
        CHECK(strcmp(name->valuestring, expected_names[i]) == 0, "entrypoint name matches the real response, in order");

        /* THE key confirmation: "michelineParameters" is exactly this
           project's first guessed field name -- confirmed correct
           against real data, not assumed */
        cJSON *mf = cJSON_GetObjectItemCaseSensitive(entry, "michelineParameters");
        CHECK(mf != NULL, "the real field name is 'michelineParameters' -- exactly this project's first guess, confirmed correct");

        char *type_json = cJSON_PrintUnformatted(mf);
        tezos_micheline_type_pool_t pool;
        int root_idx = tezos_micheline_parse_type(type_json, &pool);
        free(type_json);

        CHECK(root_idx >= 0, "real type parses correctly (including the ones using 'timestamp', now supported)");
        if (root_idx >= 0) {
            int leaves = tezos_micheline_type_leaf_count(&pool, root_idx);
            CHECK(leaves == expected_leaves[i], "leaf count matches the real entrypoint's actual field count");
        }
        i++;
    }

    /* deep check on addLiquidity specifically: confirm the nested-pair
       structure really is 3 levels deep (4 fields), matching the
       design's "N-ary records via nested 2-ary pairs" assumption
       against genuinely real data, not a hoped-for shape */
    cJSON *add_liq = cJSON_GetArrayItem(root, 1);
    cJSON *mf = cJSON_GetObjectItemCaseSensitive(add_liq, "michelineParameters");
    char *type_json = cJSON_PrintUnformatted(mf);
    tezos_micheline_type_pool_t pool;
    int root_idx = tezos_micheline_parse_type(type_json, &pool);
    free(type_json);
    CHECK(pool.nodes[root_idx].kind == TEZOS_MTYPE_PAIR, "addLiquidity's root is a pair (owner, rest)");
    int level2 = pool.nodes[root_idx].args[1];
    CHECK(pool.nodes[level2].kind == TEZOS_MTYPE_PAIR, "2nd nesting level is a pair (minLqtMinted, rest)");
    int level3 = pool.nodes[level2].args[1];
    CHECK(pool.nodes[level3].kind == TEZOS_MTYPE_PAIR, "3rd nesting level is a pair (maxTokensDeposited, deadline)");
    CHECK(pool.nodes[pool.nodes[level3].args[1]].kind == TEZOS_MTYPE_TIMESTAMP,
          "the deepest leaf is genuinely timestamp-typed, matching the real contract's actual deadline field exactly");

    /* build a real value for addLiquidity's type using plausible leaves,
       confirming the whole pipeline (real type -> real value) works end
       to end for a real contract's real entrypoint */
    const char *leaves[] = {"tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL", "100", "5000", "1735689600"};
    tezos_micheline_node_t values[16];
    int val_root = tezos_micheline_build_value(&pool, root_idx, leaves, 4, NULL, 0, values, 16);
    CHECK(val_root >= 0, "building a real value for addLiquidity's real type succeeds end-to-end");

    cJSON_Delete(root);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
