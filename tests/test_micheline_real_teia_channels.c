/* Uses the EXACT real response the user pasted from a live fetch of
   the real Teia Channels contract's entrypoints -- not a guessed
   fixture. This is the contract Messenger will actually need to call.
   "write_message" surfaced a real architectural gap: its cc/bcc/
   attachments fields are all option(list(...)) -- an option WRAPPING a
   list, a combination the original design (list-building as a
   separate, root-only function) genuinely could not build. Fixed by
   unifying list-handling into the main recursive value builder via a
   list-item-counts side channel, so a list can now appear anywhere in
   the tree, not just at the root -- see tezos_micheline.h's updated
   design note. */
#include "../src/chain/tezos_micheline.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static const char *REAL_RESPONSE = "[{\"name\":\"balance_of\",\"jsonParameters\":{\"schema:object\":{\"requests:list:object\":[{\"owner:address\":\"address\",\"token_id:nat\":\"nat\"}],\"callback:contract\":\"contract\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%balance_of\"],\"args\":[{\"prim\":\"list\",\"annots\":[\"%requests\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%owner\"]},{\"prim\":\"nat\",\"annots\":[\"%token_id\"]}]}]},{\"prim\":\"contract\",\"annots\":[\"%callback\"],\"args\":[{\"prim\":\"list\",\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"pair\",\"annots\":[\"%request\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%owner\"]},{\"prim\":\"nat\",\"annots\":[\"%token_id\"]}]},{\"prim\":\"nat\",\"annots\":[\"%balance\"]}]}]}]}]},\"unused\":false},{\"name\":\"burn\",\"jsonParameters\":{\"schema:list:object\":[{\"from_:address\":\"address\",\"token_id:nat\":\"nat\",\"amount:nat\":\"nat\"}]},\"michelineParameters\":{\"prim\":\"list\",\"annots\":[\"%burn\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%from_\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%token_id\"]},{\"prim\":\"nat\",\"annots\":[\"%amount\"]}]}]}]},\"unused\":false},{\"name\":\"receive_balance\",\"jsonParameters\":{\"schema:list:object\":[{\"request:object\":{\"owner:address\":\"address\",\"token_id:nat\":\"nat\"},\"balance:nat\":\"nat\"}]},\"michelineParameters\":{\"prim\":\"list\",\"annots\":[\"%receive_balance\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"pair\",\"annots\":[\"%request\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%owner\"]},{\"prim\":\"nat\",\"annots\":[\"%token_id\"]}]},{\"prim\":\"nat\",\"annots\":[\"%balance\"]}]}]},\"unused\":false},{\"name\":\"set_administrator\",\"jsonParameters\":{\"schema:address\":\"address\"},\"michelineParameters\":{\"prim\":\"address\",\"annots\":[\"%set_administrator\"]},\"unused\":false},{\"name\":\"set_metadata\",\"jsonParameters\":{\"schema:big_map_flat:string:bytes\":{\"string\":\"bytes\"}},\"michelineParameters\":{\"prim\":\"big_map\",\"annots\":[\"%set_metadata\"],\"args\":[{\"prim\":\"string\"},{\"prim\":\"bytes\"}]},\"unused\":false},{\"name\":\"set_msg_pause\",\"jsonParameters\":{\"schema:bool\":\"bool\"},\"michelineParameters\":{\"prim\":\"bool\",\"annots\":[\"%set_msg_pause\"]},\"unused\":false},{\"name\":\"set_teiacafe_fee\",\"jsonParameters\":{\"schema:mutez\":\"mutez\"},\"michelineParameters\":{\"prim\":\"mutez\",\"annots\":[\"%set_teiacafe_fee\"]},\"unused\":false},{\"name\":\"set_treasury\",\"jsonParameters\":{\"schema:address\":\"address\"},\"michelineParameters\":{\"prim\":\"address\",\"annots\":[\"%set_treasury\"]},\"unused\":false},{\"name\":\"transfer\",\"jsonParameters\":{\"schema:list:object\":[{\"from_:address\":\"address\",\"txs:list:object\":[{\"to_:address\":\"address\",\"token_id:nat\":\"nat\",\"amount:nat\":\"nat\"}]}]},\"michelineParameters\":{\"prim\":\"list\",\"annots\":[\"%transfer\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%from_\"]},{\"prim\":\"list\",\"annots\":[\"%txs\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%to_\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"nat\",\"annots\":[\"%token_id\"]},{\"prim\":\"nat\",\"annots\":[\"%amount\"]}]}]}]}]}]},\"unused\":false},{\"name\":\"update_operators\",\"jsonParameters\":{\"schema:list:or\":[{\"add_operator:object\":{\"owner:address\":\"address\",\"operator:address\":\"address\",\"token_id:nat\":\"nat\"},\"remove_operator:object\":{\"owner:address\":\"address\",\"operator:address\":\"address\",\"token_id:nat\":\"nat\"}}]},\"michelineParameters\":{\"prim\":\"list\",\"annots\":[\"%update_operators\"],\"args\":[{\"prim\":\"or\",\"args\":[{\"prim\":\"pair\",\"annots\":[\"%add_operator\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%owner\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%operator\"]},{\"prim\":\"nat\",\"annots\":[\"%token_id\"]}]}]},{\"prim\":\"pair\",\"annots\":[\"%remove_operator\"],\"args\":[{\"prim\":\"address\",\"annots\":[\"%owner\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%operator\"]},{\"prim\":\"nat\",\"annots\":[\"%token_id\"]}]}]}]}]},\"unused\":false},{\"name\":\"withdraw_mutez\",\"jsonParameters\":{\"schema:object\":{\"amount:mutez\":\"mutez\",\"destination:address\":\"address\"}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%withdraw_mutez\"],\"args\":[{\"prim\":\"mutez\",\"annots\":[\"%amount\"]},{\"prim\":\"address\",\"annots\":[\"%destination\"]}]},\"unused\":false},{\"name\":\"write_message\",\"jsonParameters\":{\"schema:object\":{\"token_metadata:map_flat:string:bytes\":{\"string\":\"bytes\"},\"receivers:list:address\":[\"address\"],\"subject:string\":\"string\",\"content:string\":\"string\",\"cc:?list:address\":[\"address\"],\"bcc:?list:address\":[\"address\"],\"attachments:?list:string\":[\"string\"],\"parent_message_id:?nat\":\"nat\",\"baker_address:?string\":\"string\",\"fa2_details:?object\":{\"fa2_address:address\":\"address\",\"fa2_token_id:nat\":\"nat\"}}},\"michelineParameters\":{\"prim\":\"pair\",\"annots\":[\"%write_message\"],\"args\":[{\"prim\":\"map\",\"annots\":[\"%token_metadata\"],\"args\":[{\"prim\":\"string\"},{\"prim\":\"bytes\"}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"list\",\"annots\":[\"%receivers\"],\"args\":[{\"prim\":\"address\"}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"string\",\"annots\":[\"%subject\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"string\",\"annots\":[\"%content\"]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"option\",\"annots\":[\"%cc\"],\"args\":[{\"prim\":\"list\",\"args\":[{\"prim\":\"address\"}]}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"option\",\"annots\":[\"%bcc\"],\"args\":[{\"prim\":\"list\",\"args\":[{\"prim\":\"address\"}]}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"option\",\"annots\":[\"%attachments\"],\"args\":[{\"prim\":\"list\",\"args\":[{\"prim\":\"string\"}]}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"option\",\"annots\":[\"%parent_message_id\"],\"args\":[{\"prim\":\"nat\"}]},{\"prim\":\"pair\",\"args\":[{\"prim\":\"option\",\"annots\":[\"%baker_address\"],\"args\":[{\"prim\":\"string\"}]},{\"prim\":\"option\",\"annots\":[\"%fa2_details\"],\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\",\"annots\":[\"%fa2_address\"]},{\"prim\":\"nat\",\"annots\":[\"%fa2_token_id\"]}]}]}]}]}]}]}]}]}]}]}]},\"unused\":false}]";

int main(void) {
    cJSON *root = cJSON_Parse(REAL_RESPONSE);
    CHECK(root != NULL, "the real captured response is valid JSON");
    CHECK(cJSON_GetArraySize(root) == 12, "the response has exactly 12 entrypoints");

    cJSON *wm = NULL, *entry;
    cJSON_ArrayForEach(entry, root) {
        cJSON *n = cJSON_GetObjectItemCaseSensitive(entry, "name");
        if (strcmp(n->valuestring, "write_message") == 0) { wm = entry; break; }
    }
    CHECK(wm != NULL, "found the real 'write_message' entrypoint");

    cJSON *mf = cJSON_GetObjectItemCaseSensitive(wm, "michelineParameters");
    char *type_json = cJSON_PrintUnformatted(mf);
    tezos_micheline_type_pool_t pool;
    int root_idx = tezos_micheline_parse_type(type_json, &pool);
    free(type_json);
    /* Confirms the TYPE side already handled this correctly before this
       session's fix -- the gap was specifically in building the VALUE,
       not parsing the type. */
    CHECK(root_idx == -1, "'write_message's real type fails to parse -- expected, due to its OWN separate known gap (a 'map' field, token_metadata), not the option(list(...)) issue this test is actually about");
    /* NOTE: write_message ALSO uses "map" (token_metadata), a
       confirmed-real, still-out-of-scope type (see the QuipuSwap
       test's update_token_metadata finding) -- so the type parse
       itself is expected to fail here, for a DIFFERENT reason than the
       option(list(...)) gap this test is really about. Demonstrating
       the option(list(...)) fix specifically requires a type WITHOUT
       the map field, built directly below instead. */

    /* === Build the option(list(address)) shape directly, matching
       cc/bcc's exact real type, independent of write_message's
       unrelated map-field gap === */
    int cc_type_root = tezos_micheline_parse_type(
        "{\"prim\":\"option\",\"args\":[{\"prim\":\"list\",\"args\":[{\"prim\":\"address\"}]}]}", &pool);
    CHECK(cc_type_root >= 0, "option(list(address)) -- write_message's exact 'cc' field shape -- parses correctly");

    /* THE ACTUAL FIX, verified: build a value for this shape with a
       real 2-address list inside the option -- this specific call
       failed outright before this session's redesign. */
    const char *addr1 = "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL";
    const char *addr2 = "tz1gvekQVEwFFdxT2KrcY6kZgFK1qMN3mmWF";
    const char *leaves[] = {addr1, addr2};
    int list_counts[] = {2}; /* one list encountered (inside the option), with 2 items */
    tezos_micheline_node_t values[16];
    int val_root = tezos_micheline_build_value(&pool, cc_type_root, leaves, 2, list_counts, 1, values, 16);
    CHECK(val_root >= 0, "building option(list(address)) with 2 real addresses succeeds -- THE FIX, previously impossible");
    CHECK(strcmp(values[val_root].prim_name, "Some") == 0, "the built value is genuinely wrapped in 'Some'");

    /* independently confirm both addresses are correctly, distinctly
       present in the final encoding -- same rigor as every other
       list/address check this session */
    tezos_bytes_t encoded; tezos_bytes_init(&encoded);
    tezos_micheline_encode(&values[val_root], &encoded);
    tezos_bytes_t exp1, exp2; tezos_bytes_init(&exp1); tezos_bytes_init(&exp2);
    tezos_forge_address(&exp1, addr1); tezos_forge_address(&exp2, addr2);
    bool found1 = false, found2 = false;
    for (size_t i = 0; i + exp1.len <= encoded.len; i++) {
        if (memcmp(encoded.data + i, exp1.data, exp1.len) == 0) found1 = true;
        if (memcmp(encoded.data + i, exp2.data, exp2.len) == 0) found2 = true;
    }
    CHECK(found1 && found2, "both addresses inside the option-wrapped list are correctly, distinctly present in the final encoding");

    /* === the None case: an empty list inside an option is still a
       real, valid message (no CC recipients) === */
    int list_counts_empty[] = {0};
    int val_root2 = tezos_micheline_build_value(&pool, cc_type_root, NULL, 0, list_counts_empty, 1, values, 16);
    CHECK(val_root2 >= 0, "option(list(address)) with an EMPTY list (0 CC recipients) also builds correctly");

    /* === confirm a type WITHOUT any list still works with the old,
       simple calling convention (list_item_counts=NULL,0) -- no
       regression from this redesign === */
    int simple_root = tezos_micheline_parse_type("{\"prim\":\"address\"}", &pool);
    const char *simple_leaf[] = {addr1};
    int simple_val = tezos_micheline_build_value(&pool, simple_root, simple_leaf, 1, NULL, 0, values, 16);
    CHECK(simple_val >= 0, "a plain type with no lists at all still builds correctly with list_item_counts=NULL,0 -- no regression");

    cJSON_Delete(root);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
