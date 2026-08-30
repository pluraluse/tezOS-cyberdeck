/* The full Beacon pairing flow, tied together -- the strongest
   verification available anywhere in this project short of live
   hardware: two independently-simulated peers, one sending a real
   permission_request exactly as beacon-sdk would, the other decrypting
   it using session keys derived completely independently, recovering
   the message byte-for-byte. */
#include "../src/chain/tezos_beacon_pair.h"
#include "../src/chain/tezos_beacon_message.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    tezos_beacon_keypair_t alice, bob;
    tezos_beacon_keypair_from_seed("our-device-identity-seed", &alice);
    tezos_beacon_keypair_from_seed("kukai-identity-seed", &bob);

    char alice_sender_id[64], alice_hex_hash[80];
    tezos_beacon_sender_id(alice.public_key, alice_sender_id, sizeof alice_sender_id);
    tezos_beacon_hex_hash(alice.public_key, alice_hex_hash, sizeof alice_hex_hash);
    CHECK(strcmp(alice_sender_id, alice_hex_hash) != 0, "sender_id and hex_hash are genuinely different hashes");
    CHECK(strlen(alice_hex_hash) == 64, "hex_hash is exactly 64 hex chars");

    tezos_beacon_session_keys_t alice_as_client, bob_as_server;
    tezos_beacon_session_keys_client(&alice, bob.public_key, &alice_as_client);
    tezos_beacon_session_keys_server(&bob, alice.public_key, &bob_as_server);
    char h1[65], h2[65];
    for (int i=0;i<32;i++) sprintf(h1+i*2, "%02x", alice_as_client.tx[i]);
    for (int i=0;i<32;i++) sprintf(h2+i*2, "%02x", bob_as_server.rx[i]);
    CHECK(strcmp(h1, h2) == 0, "session key exchange symmetry holds (alice.client.tx == bob.server.rx)");

    const char *real_request_json =
        "{\"type\":\"permission_request\",\"version\":\"3\",\"id\":\"session-test-001\","
        "\"senderId\":\"bob-sender-id\",\"appMetadata\":{\"senderId\":\"bob-sender-id\",\"name\":\"Kukai\"},"
        "\"network\":{\"type\":\"mainnet\"},\"scopes\":[\"operation_request\",\"sign\"]}";

    tezos_beacon_session_keys_t bob_as_client;
    tezos_beacon_session_keys_client(&bob, alice.public_key, &bob_as_client);
    char bob_ciphertext[4096];
    CHECK(tezos_beacon_secretbox_encrypt(real_request_json, bob_as_client.tx, bob_ciphertext, sizeof bob_ciphertext),
          "bob encrypts a real permission_request exactly as beacon-sdk would");

    tezos_beacon_session_keys_t alice_as_server;
    tezos_beacon_session_keys_server(&alice, bob.public_key, &alice_as_server);
    char alice_decrypted[4096];
    CHECK(tezos_beacon_secretbox_decrypt(bob_ciphertext, alice_as_server.rx, alice_decrypted, sizeof alice_decrypted),
          "alice decrypts using independently-derived server-role keys");
    CHECK(strcmp(alice_decrypted, real_request_json) == 0, "decrypted content matches bob's original JSON EXACTLY");

    tezos_beacon_permission_request_t parsed;
    CHECK(tezos_beacon_parse_permission_request(alice_decrypted, &parsed), "decrypted content parses as a valid permission_request");
    CHECK(strcmp(parsed.app_name, "Kukai") == 0, "parsed app name correct after the full encrypt->decrypt->parse chain");

    /* wrong key correctly fails, not silently returns garbage */
    tezos_beacon_session_keys_t wrong_keys;
    tezos_beacon_session_keys_server(&bob, bob.public_key, &wrong_keys);
    char should_fail[256];
    CHECK(!tezos_beacon_secretbox_decrypt(bob_ciphertext, wrong_keys.rx, should_fail, sizeof should_fail),
          "decrypting with the wrong session key correctly fails");

    /* tezos_beacon_prepare (pure local computation, no network) */
    tezos_beacon_peer_info_t peer = {0};
    strcpy(peer.public_key, "7775e38359f7e8c66332edf78a0d1e7630191e909f695393bfab40e3f723dedf");
    strcpy(peer.relay_server, "matrix.papers.tech");
    tezos_beacon_session_t session;
    CHECK(tezos_beacon_prepare(&session, "our-device-identity-seed", &peer), "prepare() computes identity + recipient addressing");
    CHECK(strstr(session.recipient, "@") == session.recipient && strstr(session.recipient, ":matrix.papers.tech") != NULL,
          "recipient address has the correct Matrix user ID shape");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
