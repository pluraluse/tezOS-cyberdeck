/* Ed25519 signer -- verified against Taquito's own complete test vector
   (seed -> pubkey -> address -> sign -> signature), exact match. */
#include "../src/chain/tezos_signer.h"
#include "../src/chain/tezos_forge.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    tezos_mock_signer_t signer;
    CHECK(tezos_mock_signer_init(&signer, "edsk4TjJWEszkHKono7XMnepVqwi37FrpbVt1KCsifJeAGimxheShG"),
          "signer init from edsk seed succeeds");

    char pubkey[128];
    tezos_mock_signer_public_key_b58(&signer, pubkey, sizeof pubkey);
    CHECK(strcmp(pubkey, "edpkuhmrbunxumoiVdQuxBZUPMmwkPt7yLtY5Qnua3VJVTLWr3vXXa") == 0,
          "public key matches Taquito golden vector");

    char address[128];
    tezos_mock_signer_address_b58(&signer, address, sizeof address);
    CHECK(strcmp(address, "tz1b9kV41KV9N3sp69ycLdSoZ2Ak8jXwtNPv") == 0,
          "address matches Taquito golden vector");

    tezos_bytes_t msg; tezos_bytes_init(&msg);
    tezos_bytes_append_byte(&msg, 0x12);
    tezos_bytes_append_byte(&msg, 0x34);

    char sig[128];
    CHECK(tezos_mock_signer_sign(&signer, &msg, sig, sizeof sig), "sign returns true");
    CHECK(strcmp(sig, "sigpKAnfQGzG4Rk5pV7z9mx2TL9veQCHD7qN4PhsUZMj1BqsumBoApBS9Ue616vKVymxrzfZE2L4h27zzxRUVy6BNPRMpufb") == 0,
          "signature matches Taquito golden vector EXACTLY");
    CHECK(tezos_mock_signer_self_verify(&signer, msg.data, msg.len), "self-verify confirms the signature validates");
    tezos_bytes_free(&msg);

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
