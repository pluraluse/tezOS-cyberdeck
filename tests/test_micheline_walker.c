#include "../src/chain/tezos_micheline.h"
#include "../src/chain/tezos_forge.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    /* === bare int === */
    {
        tezos_micheline_type_pool_t types;
        int root = tezos_micheline_parse_type("{\"prim\":\"int\"}", &types);
        tezos_micheline_walker_t w;
        tezos_micheline_walker_init(&w, &types, root);
        tezos_micheline_type_kind_t kind;
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_NEED_LEAF && kind == TEZOS_MTYPE_INT,
              "bare int: walker asks for exactly one int leaf");
        CHECK(tezos_micheline_walker_submit_leaf(&w, "4021"), "submitting the leaf succeeds");
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_DONE, "walker correctly reports DONE after the one leaf");

        tezos_micheline_node_t values[16];
        int val_root = tezos_micheline_build_value(&types, root, w.leaf_ptrs, w.leaf_count,
                                                    w.list_item_counts, w.list_item_counts_len, values, 16);
        CHECK(val_root >= 0, "walker's collected leaves build a real value");
        tezos_bytes_t encoded, expected; tezos_bytes_init(&encoded); tezos_bytes_init(&expected);
        tezos_micheline_encode(&values[val_root], &encoded);
        tezos_micheline_node_t direct = tezos_micheline_int("4021");
        tezos_micheline_encode(&direct, &expected);
        CHECK(encoded.len == expected.len && memcmp(encoded.data, expected.data, encoded.len) == 0,
              "walker-driven int encodes IDENTICALLY to the direct equivalent");
    }

    /* === pair(int, string) -- order matters === */
    {
        tezos_micheline_type_pool_t types;
        int root = tezos_micheline_parse_type(
            "{\"prim\":\"pair\",\"args\":[{\"prim\":\"int\"},{\"prim\":\"string\"}]}", &types);
        tezos_micheline_walker_t w;
        tezos_micheline_walker_init(&w, &types, root);
        tezos_micheline_type_kind_t kind;
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_NEED_LEAF && kind == TEZOS_MTYPE_INT,
              "pair(int,string): first ask is for the int (args[0]), not the string");
        tezos_micheline_walker_submit_leaf(&w, "42");
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_NEED_LEAF && kind == TEZOS_MTYPE_STRING,
              "second ask is for the string (args[1]), in the correct order");
        tezos_micheline_walker_submit_leaf(&w, "hello");
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_DONE, "done after both leaves");

        tezos_micheline_node_t values[16];
        int val_root = tezos_micheline_build_value(&types, root, w.leaf_ptrs, w.leaf_count,
                                                    w.list_item_counts, w.list_item_counts_len, values, 16);
        tezos_bytes_t encoded, expected; tezos_bytes_init(&encoded); tezos_bytes_init(&expected);
        tezos_micheline_encode(&values[val_root], &encoded);
        tezos_micheline_node_t i = tezos_micheline_int("42"), s = tezos_micheline_string("hello");
        tezos_micheline_node_t direct_pair = tezos_micheline_pair(&i, &s);
        tezos_micheline_encode(&direct_pair, &expected);
        CHECK(encoded.len == expected.len && memcmp(encoded.data, expected.data, encoded.len) == 0,
              "walker-driven pair(int,string) encodes IDENTICALLY to the direct equivalent");
    }

    /* === pair(address, address) -- the critical aliasing-risk case,
       now via the walker specifically (not just build_value directly) === */
    {
        tezos_micheline_type_pool_t types;
        int root = tezos_micheline_parse_type(
            "{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\"},{\"prim\":\"address\"}]}", &types);
        tezos_micheline_walker_t w;
        tezos_micheline_walker_init(&w, &types, root);
        tezos_micheline_type_kind_t kind;
        const char *addr1 = "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL";
        const char *addr2 = "tz1gvekQVEwFFdxT2KrcY6kZgFK1qMN3mmWF";
        tezos_micheline_walker_next(&w, &kind);
        tezos_micheline_walker_submit_leaf(&w, addr1);
        tezos_micheline_walker_next(&w, &kind);
        tezos_micheline_walker_submit_leaf(&w, addr2);
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_DONE, "done after both addresses");

        tezos_micheline_node_t values[16];
        int val_root = tezos_micheline_build_value(&types, root, w.leaf_ptrs, w.leaf_count,
                                                    w.list_item_counts, w.list_item_counts_len, values, 16);
        CHECK(val_root >= 0, "walker-driven pair(address,address) with two DIFFERENT real addresses builds successfully");
        tezos_bytes_t encoded; tezos_bytes_init(&encoded);
        tezos_micheline_encode(&values[val_root], &encoded);
        tezos_bytes_t exp1, exp2; tezos_bytes_init(&exp1); tezos_bytes_init(&exp2);
        tezos_forge_address(&exp1, addr1); tezos_forge_address(&exp2, addr2);
        bool found1 = false, found2 = false;
        for (size_t i = 0; i + exp1.len <= encoded.len; i++) {
            if (memcmp(encoded.data + i, exp1.data, exp1.len) == 0) found1 = true;
            if (memcmp(encoded.data + i, exp2.data, exp2.len) == 0) found2 = true;
        }
        CHECK(found1 && found2, "BOTH addresses correctly, distinctly present in the walker-driven encoding -- no aliasing");
    }

    /* === list(pair(address,bool)) -- real list handling via the walker === */
    {
        tezos_micheline_type_pool_t types;
        int root = tezos_micheline_parse_type(
            "{\"prim\":\"list\",\"args\":[{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\"},{\"prim\":\"bool\"}]}]}", &types);
        tezos_micheline_walker_t w;
        tezos_micheline_walker_init(&w, &types, root);
        tezos_micheline_type_kind_t kind;
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_NEED_LIST_COUNT, "list type correctly asks for an item count first");
        CHECK(tezos_micheline_walker_submit_list_count(&w, 2), "submitting count=2 succeeds");

        const char *addr1 = "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL";
        const char *addr2 = "tz1gvekQVEwFFdxT2KrcY6kZgFK1qMN3mmWF";
        /* item 1: address, bool */
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_NEED_LEAF && kind == TEZOS_MTYPE_ADDRESS, "item 1's address asked first");
        tezos_micheline_walker_submit_leaf(&w, addr1);
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_NEED_LEAF && kind == TEZOS_MTYPE_BOOL, "item 1's bool asked next");
        tezos_micheline_walker_submit_leaf(&w, "true");
        /* item 2: address, bool */
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_NEED_LEAF && kind == TEZOS_MTYPE_ADDRESS, "item 2's address asked next");
        tezos_micheline_walker_submit_leaf(&w, addr2);
        tezos_micheline_walker_next(&w, &kind);
        tezos_micheline_walker_submit_leaf(&w, "false");
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_DONE, "done after both items' leaves");
        CHECK(w.list_item_counts_len == 1 && w.list_item_counts[0] == 2, "exactly one list-count recorded, value 2");

        tezos_micheline_node_t values[16];
        int val_root = tezos_micheline_build_value(&types, root, w.leaf_ptrs, w.leaf_count,
                                                    w.list_item_counts, w.list_item_counts_len, values, 16);
        CHECK(val_root >= 0 && values[val_root].kind == MICHELINE_SEQUENCE && values[val_root].seq_count == 2,
              "walker-driven list builds a real 2-item sequence");
    }

    /* === option(list(address)) -- write_message's exact cc/bcc shape,
       now driven through the general walker instead of hand-built === */
    {
        tezos_micheline_type_pool_t types;
        int root = tezos_micheline_parse_type(
            "{\"prim\":\"option\",\"args\":[{\"prim\":\"list\",\"args\":[{\"prim\":\"address\"}]}]}", &types);
        tezos_micheline_walker_t w;
        tezos_micheline_walker_init(&w, &types, root);
        tezos_micheline_type_kind_t kind;
        /* option is skipped internally (always Some) -- next ask should be the list's count directly */
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_NEED_LIST_COUNT,
              "option(list(...)) correctly skips straight to the list's count prompt -- option itself needs no separate step");
        tezos_micheline_walker_submit_list_count(&w, 1);
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_NEED_LEAF && kind == TEZOS_MTYPE_ADDRESS, "one address asked for the single item");
        tezos_micheline_walker_submit_leaf(&w, "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL");
        CHECK(tezos_micheline_walker_next(&w, &kind) == TEZOS_WALK_DONE, "done");

        tezos_micheline_node_t values[16];
        int val_root = tezos_micheline_build_value(&types, root, w.leaf_ptrs, w.leaf_count,
                                                    w.list_item_counts, w.list_item_counts_len, values, 16);
        CHECK(val_root >= 0 && strcmp(values[val_root].prim_name, "Some") == 0,
              "walker-driven option(list(address)) builds a real Some-wrapped list -- the exact shape write_message needed");
    }

    /* === real caller-error cases fail cleanly === */
    {
        tezos_micheline_type_pool_t types;
        int root = tezos_micheline_parse_type("{\"prim\":\"int\"}", &types);
        tezos_micheline_walker_t w;
        tezos_micheline_walker_init(&w, &types, root);
        tezos_micheline_type_kind_t kind;
        tezos_micheline_walker_next(&w, &kind);
        CHECK(!tezos_micheline_walker_submit_list_count(&w, 3),
              "submitting a LIST COUNT when the walker actually wants a LEAF fails cleanly, not silently accepted");

        tezos_micheline_type_pool_t types2;
        int root2 = tezos_micheline_parse_type("{\"prim\":\"list\",\"args\":[{\"prim\":\"nat\"}]}", &types2);
        tezos_micheline_walker_t w2;
        tezos_micheline_walker_init(&w2, &types2, root2);
        tezos_micheline_walker_next(&w2, &kind);
        CHECK(!tezos_micheline_walker_submit_leaf(&w2, "5"),
              "submitting a LEAF when the walker actually wants a LIST COUNT fails cleanly");

        tezos_micheline_walker_t w3;
        tezos_micheline_walker_init(&w3, &types, root);
        tezos_micheline_walker_next(&w3, &kind);
        tezos_micheline_walker_submit_leaf(&w3, "1");
        tezos_micheline_walker_next(&w3, &kind); /* DONE */
        CHECK(tezos_micheline_walker_next(&w3, &kind) == TEZOS_WALK_DONE, "calling next() again after DONE stays DONE, doesn't misbehave");
    }

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
