#include "../src/chain/tezos_micheline.h"
#include "../src/chain/tezos_forge.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void print_hex(const tezos_bytes_t *b) {
    for (size_t i = 0; i < b->len; i++) printf("%02x", b->data[i]);
}

int main(void) {
    /* === Simple leaf type: bare int === */
    {
        tezos_micheline_type_pool_t types;
        int root = tezos_micheline_parse_type("{\"prim\":\"int\"}", &types);
        CHECK(root == 0, "bare int type parses to a single root node");
        CHECK(tezos_micheline_type_leaf_count(&types, root) == 1, "bare int needs exactly 1 leaf");

        const char *leaves[] = {"4021"};
        tezos_micheline_node_t values[16];
        int val_root = tezos_micheline_build_value(&types, root, leaves, 1, NULL, 0, values, 16);
        CHECK(val_root >= 0, "building the value from a leaf succeeds");

        tezos_bytes_t encoded, expected;
        tezos_bytes_init(&encoded); tezos_bytes_init(&expected);
        tezos_micheline_encode(&values[val_root], &encoded);
        tezos_micheline_node_t direct = tezos_micheline_int("4021");
        tezos_micheline_encode(&direct, &expected);
        CHECK(encoded.len == expected.len && memcmp(encoded.data, expected.data, encoded.len) == 0,
              "dynamically-built int value encodes IDENTICALLY to the existing directly-constructed equivalent");
    }

    /* === Composite: pair(int, string) === */
    {
        tezos_micheline_type_pool_t types;
        int root = tezos_micheline_parse_type(
            "{\"prim\":\"pair\",\"args\":[{\"prim\":\"int\",\"annots\":[\"%amount\"]},{\"prim\":\"string\",\"annots\":[\"%memo\"]}]}",
            &types);
        CHECK(root >= 0, "pair(int,string) type parses correctly");
        CHECK(tezos_micheline_type_leaf_count(&types, root) == 2, "pair(int,string) needs exactly 2 leaves");
        CHECK(strcmp(types.nodes[types.nodes[root].args[0]].annot, "amount") == 0, "field annotation 'amount' parsed correctly (leading %% stripped)");
        CHECK(strcmp(types.nodes[types.nodes[root].args[1]].annot, "memo") == 0, "field annotation 'memo' parsed correctly");

        const char *leaves[] = {"42", "hello"};
        tezos_micheline_node_t values[16];
        int val_root = tezos_micheline_build_value(&types, root, leaves, 2, NULL, 0, values, 16);
        CHECK(val_root >= 0, "building pair(int,string) value succeeds");

        tezos_bytes_t encoded, expected;
        tezos_bytes_init(&encoded); tezos_bytes_init(&expected);
        tezos_micheline_encode(&values[val_root], &encoded);
        tezos_micheline_node_t i = tezos_micheline_int("42");
        tezos_micheline_node_t s = tezos_micheline_string("hello");
        tezos_micheline_node_t direct_pair = tezos_micheline_pair(&i, &s);
        tezos_micheline_encode(&direct_pair, &expected);
        CHECK(encoded.len == expected.len && memcmp(encoded.data, expected.data, encoded.len) == 0,
              "dynamically-built pair(int,string) encodes IDENTICALLY to the direct equivalent");
    }

    /* === THE CRITICAL BUG-FIX VERIFICATION: pair(address, address) with
       TWO DIFFERENT real addresses -- proves the aliasing bug (caught
       before shipping) is actually fixed, not just theoretically fixed === */
    {
        tezos_micheline_type_pool_t types;
        int root = tezos_micheline_parse_type(
            "{\"prim\":\"pair\",\"args\":[{\"prim\":\"address\"},{\"prim\":\"address\"}]}", &types);
        CHECK(root >= 0, "pair(address,address) type parses correctly");
        CHECK(tezos_micheline_type_leaf_count(&types, root) == 2, "pair(address,address) needs exactly 2 leaves");

        const char *addr1 = "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL"; /* account 0 from earlier verified tests */
        const char *addr2 = "tz1gvekQVEwFFdxT2KrcY6kZgFK1qMN3mmWF"; /* account 1 -- genuinely different */
        const char *leaves[] = {addr1, addr2};
        tezos_micheline_node_t values[16];
        int val_root = tezos_micheline_build_value(&types, root, leaves, 2, NULL, 0, values, 16);
        CHECK(val_root >= 0, "building pair(address,address) with TWO DIFFERENT addresses succeeds");

        tezos_bytes_t encoded;
        tezos_bytes_init(&encoded);
        tezos_micheline_encode(&values[val_root], &encoded);

        /* independently verify BOTH addresses appear correctly and
           distinctly in the final encoded output -- if the aliasing
           bug were still present, both would encode as the SECOND
           address's bytes */
        tezos_bytes_t expected1, expected2;
        tezos_bytes_init(&expected1); tezos_bytes_init(&expected2);
        tezos_forge_address(&expected1, addr1);
        tezos_forge_address(&expected2, addr2);
        CHECK(memcmp(expected1.data, expected2.data, expected1.len) != 0,
              "sanity check: the two test addresses really do encode to different bytes");

        /* search for both expected byte sequences within the final
           encoded pair -- crude but correct verification that BOTH
           are genuinely present and distinct, not just the last one
           duplicated */
        bool found1 = false, found2 = false;
        for (size_t i = 0; i + expected1.len <= encoded.len; i++) {
            if (memcmp(encoded.data + i, expected1.data, expected1.len) == 0) found1 = true;
            if (memcmp(encoded.data + i, expected2.data, expected2.len) == 0) found2 = true;
        }
        CHECK(found1, "the FIRST address's correct bytes are present in the final encoding");
        CHECK(found2, "the SECOND address's correct bytes are ALSO present -- not overwritten by the first (the aliasing bug this caught)");
    }

    /* === option(int) -- always builds Some === */
    {
        tezos_micheline_type_pool_t types;
        int root = tezos_micheline_parse_type("{\"prim\":\"option\",\"args\":[{\"prim\":\"int\"}]}", &types);
        CHECK(tezos_micheline_type_leaf_count(&types, root) == 1, "option(int) needs exactly 1 leaf");
        const char *leaves[] = {"7"};
        tezos_micheline_node_t values[16];
        int val_root = tezos_micheline_build_value(&types, root, leaves, 1, NULL, 0, values, 16);
        CHECK(val_root >= 0 && strcmp(values[val_root].prim_name, "Some") == 0, "option(int) builds a real 'Some' wrapper");
    }

    /* === Failure cases -- must fail cleanly, not crash or fabricate === */
    {
        tezos_micheline_type_pool_t types;
        CHECK(tezos_micheline_parse_type("not valid json", &types) == -1, "malformed JSON fails cleanly");
        CHECK(tezos_micheline_parse_type("{\"prim\":\"map\",\"args\":[]}", &types) == -1,
              "an unsupported type (map) fails cleanly instead of being silently approximated");

        int root = tezos_micheline_parse_type("{\"prim\":\"int\"}", &types);
        const char *bad_leaf[] = {"not_a_number"};
        tezos_micheline_node_t values[16];
        CHECK(tezos_micheline_build_value(&types, root, bad_leaf, 1, NULL, 0, values, 16) == -1,
              "a non-numeric leaf for an int type fails cleanly, doesn't encode garbage");

        const char *bad_addr[] = {"not_a_real_address"};
        int addr_root = tezos_micheline_parse_type("{\"prim\":\"address\"}", &types);
        CHECK(tezos_micheline_build_value(&types, addr_root, bad_addr, 1, NULL, 0, values, 16) == -1,
              "an invalid address string fails cleanly (real base58 validation via tezos_forge_address)");
    }

    (void)print_hex;
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
