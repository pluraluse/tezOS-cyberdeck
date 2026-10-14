/* Encrypted key store -- the security-critical properties: correct-PIN
   round trip, and both a wrong PIN and a tampered file failing cleanly
   via authenticated encryption (not silently returning garbage). */
#include "../src/chain/tezos_store.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    const char *path = "/tmp/test_store_signer_permanent.bin";
    remove(path);

    CHECK(!tezos_store_exists(path), "store correctly reports not existing before anything is saved");

    tezos_store_signer_data_t data = {0};
    strcpy(data.mnemonic, "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about");
    data.word_count = 12;
    data.account = 0;

    CHECK(tezos_store_save_signer(path, "1234", &data), "save with a real PIN succeeds");
    CHECK(tezos_store_exists(path), "store correctly reports existing after saving");

    tezos_store_signer_data_t loaded = {0};
    CHECK(tezos_store_load_signer(path, "1234", &loaded), "load with the CORRECT PIN succeeds");
    CHECK(strcmp(loaded.mnemonic, data.mnemonic) == 0, "recovered mnemonic matches exactly");

    tezos_store_signer_data_t wrong = {0};
    CHECK(!tezos_store_load_signer(path, "9999", &wrong),
          "load with the WRONG PIN fails cleanly (authenticated encryption, not silent garbage)");
    CHECK(strstr(wrong.mnemonic, "abandon") == NULL,
          "wrong-PIN attempt does not leak any recognizable fragment of the real mnemonic");

    FILE *f = fopen(path, "r+b");
    fseek(f, 100, SEEK_SET);
    uint8_t byte;
    fread(&byte, 1, 1, f);
    fseek(f, 100, SEEK_SET);
    byte ^= 0xFF;
    fwrite(&byte, 1, 1, f);
    fclose(f);

    tezos_store_signer_data_t tampered = {0};
    CHECK(!tezos_store_load_signer(path, "1234", &tampered),
          "a tampered file fails to decrypt even with the CORRECT PIN");

    remove(path);
    CHECK(!tezos_store_load_signer(path, "1234", &loaded), "loading a nonexistent file fails cleanly");

    const char *plain_path = "/tmp/test_store_plain_permanent.txt";
    remove(plain_path);
    CHECK(tezos_store_save_plain(plain_path, "device_name", "cyber-test"), "plain save succeeds");
    char name_out[64];
    CHECK(tezos_store_load_plain(plain_path, "device_name", name_out, sizeof name_out)
          && strcmp(name_out, "cyber-test") == 0, "plain load round-trips exactly");
    tezos_store_save_plain(plain_path, "device_name", "renamed-device");
    tezos_store_load_plain(plain_path, "device_name", name_out, sizeof name_out);
    CHECK(strcmp(name_out, "renamed-device") == 0, "updating an existing key replaces it, doesn't duplicate");
    remove(plain_path);

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
