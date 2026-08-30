/* BIP39 + SLIP-0010 -- verified against official trezor/python-mnemonic
   test vectors, the official satoshilabs/slips SLIP-0010 reference
   algorithm, and an independent Python cross-check for the full
   end-to-end chain (real BIP39 seed -> our path 44'/1729'/0'/0'). */
#include "../src/chain/tezos_bip39.h"
#include "../src/chain/tezos_slip10.h"
#include "../src/chain/tezos_base58.h"
#include "../src/chain/tezos_signer.h"
#include "../src/chain/tezos_forge.h"
#include <sodium.h>
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void hex(const uint8_t *b, size_t n, char *out) {
    for (size_t i = 0; i < n; i++) sprintf(out + i*2, "%02x", b[i]);
    out[n*2] = '\0';
}

int main(void) {
    uint8_t entropy[16] = {0};
    char mnemonic[TEZOS_BIP39_MAX_MNEMONIC_LEN];
    CHECK(tezos_bip39_entropy_to_mnemonic(entropy, 16, mnemonic, sizeof mnemonic), "entropy_to_mnemonic returns true");
    const char *expected_mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    CHECK(strcmp(mnemonic, expected_mnemonic) == 0, "mnemonic matches official BIP39 test vector exactly");
    CHECK(tezos_bip39_validate(mnemonic, 12), "validate() accepts the correct mnemonic");
    CHECK(!tezos_bip39_validate("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon zoo", 12),
          "validate() correctly rejects a bad checksum");

    uint8_t seed[64];
    CHECK(tezos_bip39_to_seed(mnemonic, "TREZOR", seed), "to_seed returns true");
    char seed_hex[129]; hex(seed, 64, seed_hex);
    const char *expected_seed = "c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04";
    CHECK(strcmp(seed_hex, expected_seed) == 0, "seed matches official BIP39 test vector exactly");

    uint8_t spec_seed[16];
    sodium_hex2bin(spec_seed, sizeof spec_seed, "000102030405060708090a0b0c0d0e0f", 32, NULL, NULL, NULL);
    uint8_t h[64];
    crypto_auth_hmacsha512_state st;
    crypto_auth_hmacsha512_init(&st, (const uint8_t *)"ed25519 seed", 12);
    crypto_auth_hmacsha512_update(&st, spec_seed, 16);
    crypto_auth_hmacsha512_final(&st, h);
    char key_hex[65]; hex(h, 32, key_hex);
    CHECK(strcmp(key_hex, "2b4be7f19ee27bbf30c667b642d5f4aa69fd169872f8fc3059c08ebae2eb19e7") == 0,
          "SLIP-0010 master-key algorithm matches the official spec vector exactly");

    tezos_slip10_node_t node;
    tezos_slip10_derive_tezos_path(seed, 0, &node);
    hex(node.key, 32, key_hex);
    CHECK(strcmp(key_hex, "dc4cbf6aa61ef8bc06916378071971dc4e1b97cf3d93338e45f3ff064cfcab28") == 0,
          "end-to-end BIP39->SLIP-0010->our path matches independent Python cross-check exactly");

    tezos_mock_signer_t signer;
    CHECK(tezos_mock_signer_from_mnemonic(&signer, mnemonic, 12, "TREZOR", 0),
          "signer initializes correctly from mnemonic + passphrase + account 0");
    char sig_address[128];
    tezos_mock_signer_address_b58(&signer, sig_address, sizeof sig_address);

    tezos_transaction_params_t tp = {0};
    strncpy(tp.source, sig_address, sizeof tp.source - 1);
    strcpy(tp.fee, "374"); strcpy(tp.counter, "1"); strcpy(tp.gas_limit, "1000");
    strcpy(tp.storage_limit, "0"); strcpy(tp.amount, "1");
    strncpy(tp.destination, sig_address, sizeof tp.destination - 1);
    tezos_bytes_t content; tezos_bytes_init(&content);
    CHECK(tezos_forge_transaction(&content, &tp), "forge succeeds using the mnemonic-derived address");
    char sig_out[128];
    CHECK(tezos_mock_signer_sign(&signer, &content, sig_out, sizeof sig_out), "mnemonic-derived signer signs successfully");
    CHECK(tezos_mock_signer_self_verify(&signer, content.data, content.len), "signature self-verifies correctly");
    tezos_bytes_free(&content);

    CHECK(!tezos_mock_signer_from_mnemonic(&signer, "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon zoo", 12, "TREZOR", 0),
          "signer correctly refuses a mnemonic with a bad checksum");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
