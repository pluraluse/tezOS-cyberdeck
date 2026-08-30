/* Beacon crypto handshake -- verified against real beacon-sdk source
   and independent PyNaCl output, including genuine cross-implementation
   decryption (not just internal self-consistency). */
#include "../src/chain/tezos_beacon_crypto.h"
#include <sodium.h>
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void hex_decode(const char *hex, uint8_t *out) {
    sodium_hex2bin(out, 128, hex, strlen(hex), NULL, NULL, NULL);
}
static void hex_print(const uint8_t *b, size_t n, char *out) {
    for (size_t i = 0; i < n; i++) sprintf(out + i*2, "%02x", b[i]);
}

int main(void) {
    tezos_beacon_keypair_t bob;
    tezos_beacon_keypair_from_seed("bob-test-seed", &bob);

    uint8_t expected_bob_pk[32]; hex_decode("dad1adec2a19cea4536a3aeeb9d6df415eb0aae052fc56a311e3d1c01ba4632a", expected_bob_pk);
    char computed[65]; hex_print(bob.public_key, 32, computed);
    CHECK(strcmp(computed, "dad1adec2a19cea4536a3aeeb9d6df415eb0aae052fc56a311e3d1c01ba4632a") == 0,
          "Beacon identity keypair matches PyNaCl cross-check exactly");

    /* opening a payload sealed by the INDEPENDENT PyNaCl implementation */
    uint8_t sealed[128];
    const char *sealed_hex = "27d7a7be9d8c25b97382a000ad4a0a69a7567239ec91365ad6d8a30f7a43d446fee0b9735e8af83c6f07f19e6edf918c0936247521971e5ae84fb966";
    size_t sealed_len = strlen(sealed_hex) / 2;
    hex_decode(sealed_hex, sealed);

    uint8_t opened[128]; size_t opened_len;
    bool ok = tezos_beacon_open(sealed, sealed_len, &bob, opened, sizeof opened, &opened_len);
    CHECK(ok, "opening a message sealed by the INDEPENDENT PyNaCl implementation succeeds");
    opened[opened_len] = 0;
    CHECK(strcmp((char*)opened, "hello beacon") == 0, "decrypted plaintext matches exactly -- real cross-implementation interop");

    /* our own round trip */
    tezos_beacon_keypair_t alice;
    tezos_beacon_keypair_from_seed("alice-test-seed", &alice);
    uint8_t rt_sealed[128]; size_t rt_sealed_len;
    const char *rt_msg = "round trip test message";
    tezos_beacon_seal((const uint8_t *)rt_msg, strlen(rt_msg), bob.public_key, rt_sealed, sizeof rt_sealed, &rt_sealed_len);
    uint8_t rt_opened[128]; size_t rt_opened_len;
    tezos_beacon_open(rt_sealed, rt_sealed_len, &bob, rt_opened, sizeof rt_opened, &rt_opened_len);
    rt_opened[rt_opened_len] = 0;
    CHECK(strcmp((char*)rt_opened, rt_msg) == 0, "full seal->open round trip recovers the exact original message");

    uint8_t wrong_opened[128]; size_t wrong_len;
    CHECK(!tezos_beacon_open(rt_sealed, rt_sealed_len, &alice, wrong_opened, sizeof wrong_opened, &wrong_len),
          "a message sealed for bob correctly FAILS to open with alice's keypair");

    /* session keys + secretbox (ongoing communication scheme) */
    tezos_beacon_session_keys_t alice_as_client, bob_as_server;
    tezos_beacon_session_keys_client(&alice, bob.public_key, &alice_as_client);
    tezos_beacon_session_keys_server(&bob, alice.public_key, &bob_as_server);
    char h1[65], h2[65];
    hex_print(alice_as_client.tx, 32, h1); hex_print(bob_as_server.rx, 32, h2);
    CHECK(strcmp(h1, h2) == 0, "alice's client.tx == bob's server.rx (required for decryption to work)");

    char ciphertext_hex[512];
    tezos_beacon_secretbox_encrypt("test message", alice_as_client.tx, ciphertext_hex, sizeof ciphertext_hex);
    char decrypted[256];
    CHECK(tezos_beacon_secretbox_decrypt(ciphertext_hex, bob_as_server.rx, decrypted, sizeof decrypted)
          && strcmp(decrypted, "test message") == 0, "secretbox round trip works between independently-derived session keys");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
