/* Structural verification of messenger_build_write_message_value --
   checked directly against write_message's real parameter type (from
   the live data pasted this session), not just trusted via an
   end-to-end UI flow that could mask a mistake behind a network
   failure either way. */
#include "../src/apps/messenger/messenger.h"
#include "../src/chain/tezos_forge.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    const char *receiver = "tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL";
    const char *subject = "Hello";
    const char *content = "Test message from tezOS";

    tezos_micheline_node_t root;
    CHECK(messenger_build_write_message_value(receiver, subject, content, &root),
          "building write_message's real value succeeds");

    /* structural walk, matching write_message's real confirmed type:
       pair(map, pair(list(address), pair(string, pair(string,
            pair(option, pair(option, pair(option, pair(option,
                 pair(option, option))))))))) */
    CHECK(root.kind == MICHELINE_PRIM && strcmp(root.prim_name, "Pair") == 0,
          "root is a real Pair (token_metadata, rest)");
    CHECK(root.args[0]->kind == MICHELINE_SEQUENCE && root.args[0]->seq_count == 0,
          "token_metadata (arg 0) is a genuinely EMPTY sequence -- the correct wire encoding for an empty map, not a placeholder");

    const tezos_micheline_node_t *rest = root.args[1];
    CHECK(rest->kind == MICHELINE_PRIM && strcmp(rest->prim_name, "Pair") == 0, "2nd level is Pair (receivers, rest)");
    CHECK(rest->args[0]->kind == MICHELINE_SEQUENCE && rest->args[0]->seq_count == 1,
          "receivers is a real sequence with exactly 1 item (the single real receiver)");

    /* verify the receiver's address bytes are the CORRECT, real encoding
       -- not just "some bytes present" */
    tezos_bytes_t expected_addr; tezos_bytes_init(&expected_addr);
    tezos_forge_address(&expected_addr, receiver);
    const tezos_micheline_node_t *receiver_node = rest->args[0]->seq_items[0];
    CHECK(receiver_node->kind == MICHELINE_BYTES && receiver_node->bytes_len == expected_addr.len
          && memcmp(receiver_node->bytes_data, expected_addr.data, expected_addr.len) == 0,
          "the receiver's address encodes to the CORRECT real bytes (via tezos_forge_address), not a guess");

    rest = rest->args[1];
    CHECK(rest->kind == MICHELINE_PRIM && strcmp(rest->prim_name, "Pair") == 0, "3rd level is Pair (subject, rest)");
    CHECK(rest->args[0]->kind == MICHELINE_STRING && strcmp(rest->args[0]->string_value, subject) == 0,
          "subject field holds the real, correct subject string");

    rest = rest->args[1];
    CHECK(rest->kind == MICHELINE_PRIM && strcmp(rest->prim_name, "Pair") == 0, "4th level is Pair (content, rest)");
    CHECK(rest->args[0]->kind == MICHELINE_STRING && strcmp(rest->args[0]->string_value, content) == 0,
          "content field holds the real, correct content string");

    /* the remaining 5 levels should all be real "None"s -- cc, bcc,
       attachments, parent_message_id, then a final pair(baker_address,
       fa2_details), both also None */
    rest = rest->args[1];
    for (int i = 0; i < 3; i++) {
        CHECK(rest->kind == MICHELINE_PRIM && strcmp(rest->prim_name, "Pair") == 0, "optional-fields level is a real Pair");
        CHECK(rest->args[0]->kind == MICHELINE_PRIM && strcmp(rest->args[0]->prim_name, "None") == 0,
              "cc/bcc/attachments field is a real Micheline None, not a placeholder or omitted");
        rest = rest->args[1];
    }
    /* now at pair(parent_message_id_none, pair(baker_none, fa2_none)) */
    CHECK(rest->kind == MICHELINE_PRIM && strcmp(rest->prim_name, "Pair") == 0, "parent_message_id level is a real Pair");
    CHECK(rest->args[0]->kind == MICHELINE_PRIM && strcmp(rest->args[0]->prim_name, "None") == 0,
          "parent_message_id is a real None");
    const tezos_micheline_node_t *final_pair = rest->args[1];
    CHECK(final_pair->kind == MICHELINE_PRIM && strcmp(final_pair->prim_name, "Pair") == 0,
          "final level is a real Pair (baker_address, fa2_details)");
    CHECK(final_pair->args[0]->kind == MICHELINE_PRIM && strcmp(final_pair->args[0]->prim_name, "None") == 0,
          "baker_address is a real None");
    CHECK(final_pair->args[1]->kind == MICHELINE_PRIM && strcmp(final_pair->args[1]->prim_name, "None") == 0,
          "fa2_details is a real None -- the full 9-field structure matches write_message's real type exactly");

    /* === real encoding round-trip -- the whole structure actually
       encodes without error, producing real, non-empty bytes === */
    tezos_bytes_t encoded; tezos_bytes_init(&encoded);
    CHECK(tezos_micheline_encode(&root, &encoded) && encoded.len > 0,
          "the full hand-built value encodes to real, non-empty Micheline bytes");
    tezos_bytes_free(&encoded);
    tezos_bytes_free(&expected_addr);

    /* === invalid receiver address fails cleanly === */
    tezos_micheline_node_t bad_root;
    CHECK(!messenger_build_write_message_value("not_a_real_address", subject, content, &bad_root),
          "an invalid receiver address is rejected cleanly (real base58 validation), doesn't build garbage");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
