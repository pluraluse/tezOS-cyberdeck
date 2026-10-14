#include "tezos_store.h"
#include <sodium.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

bool tezos_store_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

bool tezos_store_save_signer(const char *path, const char *pin, const tezos_store_signer_data_t *data) {
    uint8_t salt[crypto_pwhash_SALTBYTES];
    randombytes_buf(salt, sizeof salt);

    uint8_t key[crypto_secretbox_KEYBYTES];
    /* INTERACTIVE cost, deliberately -- this needs to run acceptably on
       a Pi Zero 2 W, not just a dev machine. See header for why this
       isn't MODERATE/SENSITIVE. */
    if (crypto_pwhash(key, sizeof key, pin, strlen(pin), salt,
                      crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
                      crypto_pwhash_ALG_ARGON2ID13) != 0) {
        return false; /* out of memory during KDF -- a genuine failure, not a wrong PIN */
    }

    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof nonce);

    size_t ciphertext_len = sizeof(tezos_store_signer_data_t) + crypto_secretbox_MACBYTES;
    uint8_t *ciphertext = malloc(ciphertext_len);
    if (!ciphertext) return false;

    crypto_secretbox_easy(ciphertext, (const uint8_t *)data, sizeof(tezos_store_signer_data_t), nonce, key);
    sodium_memzero(key, sizeof key); /* don't leave the derived key in memory longer than needed */

    FILE *f = fopen(path, "wb");
    if (!f) { free(ciphertext); return false; }
    bool ok = fwrite(salt, 1, sizeof salt, f) == sizeof salt
           && fwrite(nonce, 1, sizeof nonce, f) == sizeof nonce
           && fwrite(ciphertext, 1, ciphertext_len, f) == ciphertext_len;
    fclose(f);
    free(ciphertext);
    return ok;
}

bool tezos_store_load_signer(const char *path, const char *pin, tezos_store_signer_data_t *data_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    uint8_t salt[crypto_pwhash_SALTBYTES];
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    if (fread(salt, 1, sizeof salt, f) != sizeof salt || fread(nonce, 1, sizeof nonce, f) != sizeof nonce) {
        fclose(f);
        return false; /* truncated/corrupted file -- fails here, not later as garbage */
    }

    size_t ciphertext_len = sizeof(tezos_store_signer_data_t) + crypto_secretbox_MACBYTES;
    uint8_t *ciphertext = malloc(ciphertext_len);
    if (!ciphertext) { fclose(f); return false; }
    size_t read_len = fread(ciphertext, 1, ciphertext_len, f);
    fclose(f);
    if (read_len != ciphertext_len) { free(ciphertext); return false; }

    uint8_t key[crypto_secretbox_KEYBYTES];
    if (crypto_pwhash(key, sizeof key, pin, strlen(pin), salt,
                      crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
                      crypto_pwhash_ALG_ARGON2ID13) != 0) {
        free(ciphertext);
        return false;
    }

    /* crypto_secretbox_open_easy is authenticated -- a wrong PIN (wrong
       derived key) or a tampered ciphertext both fail HERE, cleanly,
       rather than "succeeding" and handing back garbage that looks
       like a plausible mnemonic. This is the property actually being
       relied on for security, not an incidental detail. */
    bool ok = crypto_secretbox_open_easy((uint8_t *)data_out, ciphertext, ciphertext_len, nonce, key) == 0;
    sodium_memzero(key, sizeof key);
    free(ciphertext);
    return ok;
}

bool tezos_store_save_plain(const char *path, const char *key, const char *value) {
    /* Simple approach: read all existing key=value lines, replace or
       append the one being set, write back. Fine for a handful of
       settings on an embedded device; not built for a large config. */
    char lines[64][256];
    int line_count = 0;
    bool found = false;

    FILE *f = fopen(path, "r");
    if (f) {
        while (line_count < 64 && fgets(lines[line_count], sizeof lines[0], f)) {
            size_t len = strlen(lines[line_count]);
            if (len > 0 && lines[line_count][len-1] == '\n') lines[line_count][len-1] = '\0';

            char existing_key[64];
            if (sscanf(lines[line_count], "%63[^=]=", existing_key) == 1 && strcmp(existing_key, key) == 0) {
                snprintf(lines[line_count], sizeof lines[0], "%s=%s", key, value);
                found = true;
            }
            line_count++;
        }
        fclose(f);
    }
    if (!found && line_count < 64) {
        snprintf(lines[line_count], sizeof lines[0], "%s=%s", key, value);
        line_count++;
    }

    FILE *out = fopen(path, "w");
    if (!out) return false;
    for (int i = 0; i < line_count; i++) fprintf(out, "%s\n", lines[i]);
    fclose(out);
    return true;
}

bool tezos_store_load_plain(const char *path, const char *key, char *value_out, size_t value_cap) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[256];
    bool found = false;
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        char existing_key[64];
        if (sscanf(line, "%63[^=]=", existing_key) == 1 && strcmp(existing_key, key) == 0) {
            const char *val_start = line + strlen(existing_key) + 1;
            strncpy(value_out, val_start, value_cap - 1);
            value_out[value_cap - 1] = '\0';
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}
