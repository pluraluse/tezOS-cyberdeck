/* Uses the EXACT real response the user pasted from a live fetch of
   api.tzkt.io/v1/contracts/KT1J8Hr3BP8bpbfmgGpRPoC9nAMSYtStZG43/entrypoints?micheline=true
   (the real QuipuSwap DEX 2.0 factory, 30 real entrypoints) -- not a
   guessed fixture. This is a much richer, more demanding dataset than
   Liquidity Baking: it surfaced two real, common types this project
   didn't support at all until now ("list" and "key_hash"), and several
   more that remain deliberately out of scope ("contract", "key",
   "signature", "map", "lambda"/"operation") -- confirmed real on this
   contract's actual entrypoints, not hypothetical.

   NOTE on this test's data: two entries in the user's original paste
   (divest_liquidity, invest_liquidity) were missing a closing bracket
   each -- a real transcription slip caught while preparing this test,
   not a defect in the actual live API response. Rather than risk
   introducing a second error by manually re-counting brackets, both
   were rebuilt programmatically from their own field lists (matching
   the exact structure the surrounding, correctly-transcribed entries
   already established) and spliced back in via a JSON library, not
   hand-typed a second time. */
#include "../src/chain/tezos_micheline.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static const char *REAL_RESPONSE = "[{\"name\":\"default\",\"jsonParameters\":{\"schema:unit\":\"unit\"},\"michelineParameters\":{\"prim\":\"unit\",\"annots\":[\"%default\"]},\"unused\":false},{\"name\":\"setup_func\",\"jsonParameters\":{\"schema:object\":{\"idx:nat\":\"nat\",\"func_bytes:bytes\":\"bytes\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%setup_func\"],\"args\":[{\"prim\":\"nat\",\"annots\":[\"%idx\"]},{\"prim\":\"bytes\",\"annots\":[\"%func_bytes\"]}]},\"unused\":false},{\"name\":\"add_managers\",\"jsonParameters\":{\"schema:list:object\":[{\"manager:address\":\"address\",\"add:bool\":\"bool\"}]},\"michelineParameters\":{\"prim\":\"list\",\"annots\":[\"%add_managers\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%manager\"]},{\"prim\":\"bool\",\"annots\":[\"%add\"]}]}]},\"unused\":false},{\"name\":\"balance_of\",\"jsonParameters\":{\"schema:object\":{\"requests:list:object\":[{\"owner:address\":\"address\",\"token_id:nat\":\"nat\"}],\"callback:contract\":\"contract\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%balance_of\"],\"args\":[{\"prim\":\"list\",\"annots\":[\"%requests\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%owner\"]},{\"prim\":\"nat\",\"annots\":[\"%token_id\"]}]}]},{\"prim\":\"contract\",\"annots\":[\"%callback\"],\"args\":[{\"prim\":\"list\",\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"pair\",\"annots\":[\"%request\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%owner\"]},{\"prim\":\"nat\",\"annots\":[\"%token_id\"]}]},{\"prim\":\"nat\",\"annots\":[\"%balance\"]}]}]}]}]},\"unused\":false},{\"name\":\"ban\",\"jsonParameters\":{\"schema:object\":{\"pair_id:nat\":\"nat\",\"ban_params:object\":{\"baker:key_hash\":\"key_hash\",\"ban_period:nat\":\"nat\"}}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%ban\"],\"args\":[{\"prim\":\"nat\",\"annots\":[\"%pair_id\"]},{\"prim\":\"pair\",\"annots\":[\"%ban_params\"],\"args\":[{\"prim\":\"key_hash\",\"annots\":[\"%baker\"]},{\"prim\":\"nat\",\"annots\":[\"%ban_period\"]}]}]},\"unused\":false},{\"name\":\"claim\",\"jsonParameters\":{\"schema:object\":{\"pair_id:nat\":\"nat\",\"receiver:address\":\"address\",\"referral_code:?nat\":\"nat\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%claim\"],\"args\":[{\"prim\":\"nat\",\"annots\":[\"%pair_id\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%receiver\"]},{\"prim\":\"option\",\"annots\":[\"%referral_code\"],\"args\":[{\"prim\":\"nat\"}]}]}]},\"unused\":false},{\"name\":\"claim_interface_fee\",\"jsonParameters\":{\"schema:object\":{\"token:or\":{\"fa12:address\":\"address\",\"fa2:object\":{\"token:address\":\"address\",\"id:nat\":\"nat\"},\"tez:unit\":\"unit\"},\"receiver:address\":\"address\",\"referral_code:?nat\":\"nat\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%claim_interface_fee\"],\"args\":[{\"prim\":\"or\",\"annots\":[\"%token\"],\"args\":[{\"prim\":\"or\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%fa12\"]},{\"prim\":\"pair\",\"annots\":[\"%fa2\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%token\"]},{\"prim\":\"nat\",\"annots\":[\"%id\"]}]}]},{\"prim\":\"unit\",\"annots\":[\"%tez\"]}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%receiver\"]},{\"prim\":\"option\",\"annots\":[\"%referral_code\"],\"args\":[{\"prim\":\"nat\"}]}]}]},\"unused\":false},{\"name\":\"claim_interface_tez_fee\",\"jsonParameters\":{\"schema:object\":{\"pair_id:nat\":\"nat\",\"receiver:address\":\"address\",\"referral_code:?nat\":\"nat\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%claim_interface_tez_fee\"],\"args\":[{\"prim\":\"nat\",\"annots\":[\"%pair_id\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%receiver\"]},{\"prim\":\"option\",\"annots\":[\"%referral_code\"],\"args\":[{\"prim\":\"nat\"}]}]}]},\"unused\":false},{\"name\":\"close\",\"jsonParameters\":{\"schema:unit\":\"unit\"},\"michelineParameters\":{\"prim\":\"unit\",\"annots\":[\"%close\"]},\"unused\":false},{\"name\":\"confirm_admin\",\"jsonParameters\":{\"schema:unit\":\"unit\"},\"michelineParameters\":{\"prim\":\"unit\",\"annots\":[\"%confirm_admin\"]},\"unused\":false},{\"name\":\"divest_liquidity\",\"michelineParameters\":{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%pair_id\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%min_token_a_out\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%min_token_b_out\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%shares\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%liquidity_receiver\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"key_hash\",\"annots\":[\"%candidate\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"timestamp\",\"annots\":[\"%deadline\"]},{\"prim\":\"option\",\"args\":[{\"prim\":\"nat\"}],\"annots\":[\"%referral_code\"]}]}]}]}]}]}]}],\"annots\":[\"%divest_liquidity\"]},\"unused\":false},{\"name\":\"flash_swap_callback\",\"jsonParameters\":{\"schema:object\":{\"pair_id:nat\":\"nat\",\"prev_tez_balance:nat\":\"nat\",\"amount_in:nat\":\"nat\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%flash_swap_callback\"],\"args\":[{\"prim\":\"nat\",\"annots\":[\"%pair_id\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%prev_tez_balance\"]},{\"prim\":\"nat\",\"annots\":[\"%amount_in\"]}]}]},\"unused\":false},{\"name\":\"invest_liquidity\",\"michelineParameters\":{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%pair_id\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%token_a_in\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%token_b_in\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%shares\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%shares_receiver\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"key_hash\",\"annots\":[\"%candidate\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"timestamp\",\"annots\":[\"%deadline\"]},{\"prim\":\"option\",\"args\":[{\"prim\":\"nat\"}],\"annots\":[\"%referral_code\"]}]}]}]}]}]}]}],\"annots\":[\"%invest_liquidity\"]},\"unused\":false},{\"name\":\"launch_callback\",\"jsonParameters\":{\"schema:object\":{\"vote_params:object\":{\"voter:address\":\"address\",\"candidate:key_hash\":\"key_hash\",\"execute_voting:bool\":\"bool\",\"votes:nat\":\"nat\"},\"bucket:address\":\"address\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%launch_callback\"],\"args\":[{\"prim\":\"pair\",\"annots\":[\"%vote_params\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%voter\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"key_hash\",\"annots\":[\"%candidate\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"bool\",\"annots\":[\"%execute_voting\"]},{\"prim\":\"nat\",\"annots\":[\"%votes\"]}]}]}]},{\"prim\":\"address\",\"annots\":[\"%bucket\"]}]},\"unused\":false},{\"name\":\"launch_exchange\",\"jsonParameters\":{\"schema:object\":{\"pair:object\":{\"token_a:or\":{\"fa12:address\":\"address\",\"fa2:object\":{\"token:address\":\"address\",\"id:nat\":\"nat\"},\"tez:unit\":\"unit\"},\"token_b:or\":{\"fa12:address\":\"address\",\"fa2:object\":{\"token:address\":\"address\",\"id:nat\":\"nat\"},\"tez:unit\":\"unit\"}},\"token_a_in:nat\":\"nat\",\"token_b_in:nat\":\"nat\",\"shares_receiver:address\":\"address\",\"candidate:key_hash\":\"key_hash\",\"deadline:timestamp\":\"timestamp\",\"referral_code:?nat\":\"nat\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%launch_exchange\"],\"args\":[{\"prim\":\"pair\",\"annots\":[\"%pair\"],\"args\":[{\"prim\":\"or\",\"annots\":[\"%token_a\"],\"args\":[{\"prim\":\"or\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%fa12\"]},{\"prim\":\"pair\",\"annots\":[\"%fa2\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%token\"]},{\"prim\":\"nat\",\"annots\":[\"%id\"]}]}]},{\"prim\":\"unit\",\"annots\":[\"%tez\"]}]},{\"prim\":\"or\",\"annots\":[\"%token_b\"],\"args\":[{\"prim\":\"or\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%fa12\"]},{\"prim\":\"pair\",\"annots\":[\"%fa2\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%token\"]},{\"prim\":\"nat\",\"annots\":[\"%id\"]}]}]},{\"prim\":\"unit\",\"annots\":[\"%tez\"]}]}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%token_a_in\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%token_b_in\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%shares_receiver\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"key_hash\",\"annots\":[\"%candidate\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"timestamp\",\"annots\":[\"%deadline\"]},{\"prim\":\"option\",\"annots\":[\"%referral_code\"],\"args\":[{\"prim\":\"nat\"}]}]}]}]}]}]}]},\"unused\":false},{\"name\":\"permit\",\"jsonParameters\":{\"schema:list:object\":[{\"key:key\":\"key\",\"signature:signature\":\"signature\",\"permit_hash:bytes\":\"bytes\"}]},\"michelineParameters\":{\"prim\":\"list\",\"annots\":[\"%permit\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"key\"},{\"prim\":\"pair\",\"args\":[{\"prim\":\"signature\"},{\"prim\":\"bytes\",\"annots\":[\"%permit_hash\"]}]}]}]},\"unused\":false},{\"name\":\"set_admin\",\"jsonParameters\":{\"schema:address\":\"address\"},\"michelineParameters\":{\"prim\":\"address\",\"annots\":[\"%set_admin\"]},\"unused\":false},{\"name\":\"set_auction\",\"jsonParameters\":{\"schema:address\":\"address\"},\"michelineParameters\":{\"prim\":\"address\",\"annots\":[\"%set_auction\"]},\"unused\":false},{\"name\":\"set_baker_rate\",\"jsonParameters\":{\"schema:nat\":\"nat\"},\"michelineParameters\":{\"prim\":\"nat\",\"annots\":[\"%set_baker_rate\"]},\"unused\":false},{\"name\":\"set_collecting_period\",\"jsonParameters\":{\"schema:nat\":\"nat\"},\"michelineParameters\":{\"prim\":\"nat\",\"annots\":[\"%set_collecting_period\"]},\"unused\":false},{\"name\":\"set_expiry\",\"jsonParameters\":{\"schema:object\":{\"issuer:address\":\"address\",\"expiry:nat\":\"nat\",\"permit_hash:?bytes\":\"bytes\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%set_expiry\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%issuer\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%expiry\"]},{\"prim\":\"option\",\"annots\":[\"%permit_hash\"],\"args\":[{\"prim\":\"bytes\"}]}]}]},\"unused\":false},{\"name\":\"set_fees\",\"jsonParameters\":{\"schema:object\":{\"interface_fee:nat\":\"nat\",\"swap_fee:nat\":\"nat\",\"auction_fee:nat\":\"nat\",\"withdraw_fee_reward:nat\":\"nat\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%set_fees\"],\"args\":[{\"prim\":\"nat\",\"annots\":[\"%interface_fee\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%swap_fee\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%auction_fee\"]},{\"prim\":\"nat\",\"annots\":[\"%withdraw_fee_reward\"]}]}]}]},\"unused\":false},{\"name\":\"set_flash_swaps_proxy\",\"jsonParameters\":{\"schema:address\":\"address\"},\"michelineParameters\":{\"prim\":\"address\",\"annots\":[\"%set_flash_swaps_proxy\"]},\"unused\":false},{\"name\":\"swap\",\"jsonParameters\":{\"schema:object\":{\"lambda:?lambda\":\"lambda\",\"swaps:list:object\":[{\"direction:or\":{\"a_to_b:unit\":\"unit\",\"b_to_a:unit\":\"unit\"},\"pair_id:nat\":\"nat\"}],\"deadline:timestamp\":\"timestamp\",\"receiver:address\":\"address\",\"referrer:address\":\"address\",\"amount_in:nat\":\"nat\",\"min_amount_out:nat\":\"nat\",\"referral_code:?nat\":\"nat\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%swap\"],\"args\":[{\"prim\":\"option\",\"annots\":[\"%lambda\"],\"args\":[{\"prim\":\"lambda\",\"args\":[{\"prim\":\"unit\"},{\"prim\":\"list\",\"args\":[{\"prim\":\"operation\"}]}]}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"list\",\"annots\":[\"%swaps\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"or\",\"annots\":[\"%direction\"],\"args\":[{\"prim\":\"unit\",\"annots\":[\"%a_to_b\"]},{\"prim\":\"unit\",\"annots\":[\"%b_to_a\"]}]},{\"prim\":\"nat\",\"annots\":[\"%pair_id\"]}]}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"timestamp\",\"annots\":[\"%deadline\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%receiver\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%referrer\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%amount_in\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%min_amount_out\"]},{\"prim\":\"option\",\"annots\":[\"%referral_code\"],\"args\":[{\"prim\":\"nat\"}]}]}]}]}]}]}]}]},\"unused\":false},{\"name\":\"transfer\",\"jsonParameters\":{\"schema:list:object\":[{\"from_:address\":\"address\",\"txs:list:object\":[{\"to_:address\":\"address\",\"token_id:nat\":\"nat\",\"amount:nat\":\"nat\"}]}]},\"michelineParameters\":{\"prim\":\"list\",\"annots\":[\"%transfer\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%from_\"]},{\"prim\":\"list\",\"annots\":[\"%txs\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%to_\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%token_id\"]},{\"prim\":\"nat\",\"annots\":[\"%amount\"]}]}]}]}]}]},\"unused\":false},{\"name\":\"update_operators\",\"jsonParameters\":{\"schema:list:or\":[{\"add_operator:object\":{\"owner:address\":\"address\",\"operator:address\":\"address\",\"token_id:nat\":\"nat\"},\"remove_operator:object\":{\"owner:address\":\"address\",\"operator:address\":\"address\",\"token_id:nat\":\"nat\"}}]},\"michelineParameters\":{\"prim\":\"list\",\"annots\":[\"%update_operators\"],\"args\":[{\"prim\":\"or\",\"args\":[{\"prim\":\"pair\",\"annots\":[\"%add_operator\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%owner\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%operator\"]},{\"prim\":\"nat\",\"annots\":[\"%token_id\"]}]}]},{\"prim\":\"pair\",\"annots\":[\"%remove_operator\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%owner\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%operator\"]},{\"prim\":\"nat\",\"annots\":[\"%token_id\"]}]}]}]}]},\"unused\":false},{\"name\":\"update_token_metadata\",\"jsonParameters\":{\"schema:object\":{\"token_id:nat\":\"nat\",\"token_info:map_flat:string:bytes\":{\"string\":\"bytes\"}}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%update_token_metadata\"],\"args\":[{\"prim\":\"nat\",\"annots\":[\"%token_id\"]},{\"prim\":\"map\",\"annots\":[\"%token_info\"],\"args\":[{\"prim\":\"string\"},{\"prim\":\"bytes\"}]}]},\"unused\":false},{\"name\":\"vote\",\"jsonParameters\":{\"schema:object\":{\"pair_id:nat\":\"nat\",\"candidate:key_hash\":\"key_hash\",\"referral_code:?nat\":\"nat\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%vote\"],\"args\":[{\"prim\":\"nat\",\"annots\":[\"%pair_id\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"key_hash\",\"annots\":[\"%candidate\"]},{\"prim\":\"option\",\"annots\":[\"%referral_code\"],\"args\":[{\"prim\":\"nat\"}]}]}]},\"unused\":false},{\"name\":\"withdraw_auction_fee\",\"jsonParameters\":{\"schema:object\":{\"pair_id:?nat\":\"nat\",\"token:or\":{\"fa12:address\":\"address\",\"fa2:object\":{\"token:address\":\"address\",\"id:nat\":\"nat\"},\"tez:unit\":\"unit\"},\"referral_code:?nat\":\"nat\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%withdraw_auction_fee\"],\"args\":[{\"prim\":\"option\",\"annots\":[\"%pair_id\"],\"args\":[{\"prim\":\"nat\"}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"or\",\"annots\":[\"%token\"],\"args\":[{\"prim\":\"or\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%fa12\"]},{\"prim\":\"pair\",\"annots\":[\"%fa2\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%token\"]},{\"prim\":\"nat\",\"annots\":[\"%id\"]}]}]},{\"prim\":\"unit\",\"annots\":[\"%tez\"]}]},{\"prim\":\"option\",\"annots\":[\"%referral_code\"],\"args\":[{\"prim\":\"nat\"}]}]}]},\"unused\":false},{\"name\":\"withdraw_profit\",\"jsonParameters\":{\"schema:object\":{\"receiver:contract\":\"contract\",\"pair_id:nat\":\"nat\",\"referral_code:?nat\":\"nat\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%withdraw_profit\"],\"args\":[{\"prim\":\"contract\",\"annots\":[\"%receiver\"],\"args\":[{\"prim\":\"unit\"}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%pair_id\"]},{\"prim\":\"option\",\"annots\":[\"%referral_code\"],\"args\":[{\"prim\":\"nat\"}]}]}]},\"unused\":false}]";

int main(void) {
    cJSON *root = cJSON_Parse(REAL_RESPONSE);
    CHECK(root != NULL, "the real captured response (post-correction) is valid JSON");
    CHECK(cJSON_GetArraySize(root) == 30, "the response has exactly 30 entrypoints");

    int total = 0, parsed_ok = 0;
    cJSON *entry;
    cJSON_ArrayForEach(entry, root) {
        cJSON *mf = cJSON_GetObjectItemCaseSensitive(entry, "michelineParameters");
        char *type_json = cJSON_PrintUnformatted(mf);
        tezos_micheline_type_pool_t pool;
        int root_idx = tezos_micheline_parse_type(type_json, &pool);
        free(type_json);
        total++;
        if (root_idx >= 0) parsed_ok++;
    }
    printf("  %d of %d real entrypoints parse fully with the current supported type list\n", parsed_ok, total);
    CHECK(parsed_ok == 25 && total == 30,
          "exactly 25 of 30 real entrypoints parse fully; the other 5 use still-out-of-scope types (contract/key/signature/map/lambda)");

    /* === key_hash: verify against "ban" === */
    {
        cJSON *ban = NULL;
        cJSON_ArrayForEach(entry, root) {
            cJSON *n = cJSON_GetObjectItemCaseSensitive(entry, "name");
            if (strcmp(n->valuestring, "ban") == 0) { ban = entry; break; }
        }
        CHECK(ban != NULL, "found the real 'ban' entrypoint");
        cJSON *mf = cJSON_GetObjectItemCaseSensitive(ban, "michelineParameters");
        char *type_json = cJSON_PrintUnformatted(mf);
        tezos_micheline_type_pool_t pool;
        int root_idx = tezos_micheline_parse_type(type_json, &pool);
        free(type_json);
        CHECK(root_idx >= 0, "'ban's real type (pair(nat, pair(key_hash, nat))) parses correctly, including key_hash");
        CHECK(tezos_micheline_type_leaf_count(&pool, root_idx) == 3, "'ban' needs exactly 3 leaves");

        const char *leaves[] = {"1", "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL", "30"};
        tezos_micheline_node_t values[16];
        int val_root = tezos_micheline_build_value(&pool, root_idx, leaves, 3, NULL, 0, values, 16);
        CHECK(val_root >= 0, "building a real value for 'ban', with a real key_hash leaf, succeeds end-to-end");

        tezos_bytes_t expected_pkh; tezos_bytes_init(&expected_pkh);
        tezos_forge_pkh(&expected_pkh, "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL");
        tezos_bytes_t encoded; tezos_bytes_init(&encoded);
        tezos_micheline_encode(&values[val_root], &encoded);
        bool found = false;
        for (size_t i = 0; i + expected_pkh.len <= encoded.len; i++) {
            if (memcmp(encoded.data + i, expected_pkh.data, expected_pkh.len) == 0) { found = true; break; }
        }
        CHECK(found, "the key_hash's correct 21-byte PKH encoding (via tezos_forge_pkh) is genuinely present in the final output");
    }

    /* === list: verify against "add_managers", with MULTIPLE items === */
    {
        cJSON *am = NULL;
        cJSON_ArrayForEach(entry, root) {
            cJSON *n = cJSON_GetObjectItemCaseSensitive(entry, "name");
            if (strcmp(n->valuestring, "add_managers") == 0) { am = entry; break; }
        }
        CHECK(am != NULL, "found the real 'add_managers' entrypoint");
        cJSON *mf = cJSON_GetObjectItemCaseSensitive(am, "michelineParameters");
        char *type_json = cJSON_PrintUnformatted(mf);
        tezos_micheline_type_pool_t pool;
        int root_idx = tezos_micheline_parse_type(type_json, &pool);
        free(type_json);
        CHECK(root_idx >= 0 && pool.nodes[root_idx].kind == TEZOS_MTYPE_LIST,
              "'add_managers's real type parses as a genuine LIST of pair(address,bool)");

        int element_type = pool.nodes[root_idx].args[0];
        int per_item = tezos_micheline_type_leaf_count(&pool, element_type);
        CHECK(per_item == 2, "each list item (pair(address,bool)) needs exactly 2 leaves");

        const char *addr1 = "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL";
        const char *addr2 = "tz1gvekQVEwFFdxT2KrcY6kZgFK1qMN3mmWF";
        const char *leaves[] = {addr1, "true", addr2, "false"};
        tezos_micheline_node_t values[16];
        int list_root = tezos_micheline_build_list_value(&pool, root_idx, leaves, 4, 2, values, 16);
        CHECK(list_root >= 0, "building a real 2-item list for 'add_managers' succeeds");
        CHECK(values[list_root].kind == MICHELINE_SEQUENCE, "the built list value is genuinely a Micheline sequence");
        CHECK(values[list_root].seq_count == 2, "the sequence has exactly 2 items, matching item_count");

        tezos_bytes_t encoded; tezos_bytes_init(&encoded);
        tezos_micheline_encode(&values[list_root], &encoded);
        tezos_bytes_t exp1, exp2; tezos_bytes_init(&exp1); tezos_bytes_init(&exp2);
        tezos_forge_address(&exp1, addr1); tezos_forge_address(&exp2, addr2);
        bool found1 = false, found2 = false;
        for (size_t i = 0; i + exp1.len <= encoded.len; i++) {
            if (memcmp(encoded.data + i, exp1.data, exp1.len) == 0) found1 = true;
            if (memcmp(encoded.data + i, exp2.data, exp2.len) == 0) found2 = true;
        }
        CHECK(found1 && found2, "BOTH list items' addresses are correctly, distinctly present in the final encoding");

        int empty_root = tezos_micheline_build_list_value(&pool, root_idx, NULL, 0, 0, values, 16);
        CHECK(empty_root >= 0 && values[empty_root].seq_count == 0, "building an EMPTY list (0 items) succeeds correctly");
    }

    /* === confirm the still-out-of-scope types fail cleanly on the REAL
       entrypoints that actually use them === */
    {
        const char *out_of_scope_names[] = {"balance_of", "permit", "swap", "update_token_metadata", "withdraw_profit"};
        for (int i = 0; i < 5; i++) {
            cJSON *found_entry = NULL;
            cJSON_ArrayForEach(entry, root) {
                cJSON *n = cJSON_GetObjectItemCaseSensitive(entry, "name");
                if (strcmp(n->valuestring, out_of_scope_names[i]) == 0) { found_entry = entry; break; }
            }
            cJSON *mf = cJSON_GetObjectItemCaseSensitive(found_entry, "michelineParameters");
            char *type_json = cJSON_PrintUnformatted(mf);
            tezos_micheline_type_pool_t pool;
            int root_idx = tezos_micheline_parse_type(type_json, &pool);
            free(type_json);
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' (uses a still-out-of-scope type) fails to parse cleanly, as expected", out_of_scope_names[i]);
            CHECK(root_idx == -1, msg);
        }
    }

    cJSON_Delete(root);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
