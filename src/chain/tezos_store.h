#pragma once
/*
 * Encrypted key store — persists the signer's mnemonic/seed material
 * across a restart, which nothing in this project did before this.
 * `wallet_set_active_signer()` was writing to a plain in-memory static;
 * reboot the device and a successfully-imported recovery phrase
 * silently reverted to the hardcoded dev seed with no indication to the
 * user. This closes that gap.
 *
 * DELIBERATE DESIGN CALL: this is a real encrypted store, not just a
 * file with a name that implies security. Encryption without a real
 * secret behind the key is theater, not protection — anyone with the
 * binary and the storage file could derive the same key from a fixed
 * device ID with no real secret involved. The key here is derived from
 * a user PIN via Argon2id (libsodium's crypto_pwhash, already the
 * project's KDF of choice elsewhere), meaning this is exactly as strong
 * as the PIN the user chooses and nothing else — stated plainly, not
 * oversold. This also makes Settings' "Lock / Passcode" row real for
 * the first time (see settings.c) — a PIN store without a PIN entry
 * screen doesn't accomplish anything.
 *
 * HONEST LIMITATION, worth being direct about: this protects the seed
 * at rest on the Linux side's storage. It is NOT the project's intended
 * primary trust boundary — that's the airgapped physical signer
 * (STM32/Pico), which doesn't exist yet (see
 * docs/architecture/SIGNER-PROTOCOL.md). This store exists because the
 * software mock signer currently has to hold real seed material
 * somewhere on the Linux side out of necessity, not because Linux-side
 * storage is meant to be the long-term answer. Once real signer
 * hardware exists, the seed should live there instead, and this store's
 * job shrinks to non-sensitive state only.
 *
 * Encryption: libsodium's crypto_secretbox (XSalsa20-Poly1305,
 * authenticated) — the same primitive already used and tested
 * elsewhere in this project (tezos_beacon_crypto.c's ongoing-message
 * encryption), for consistency rather than introducing a second scheme.
 * Key derivation: Argon2id (crypto_pwhash) with INTERACTIVE cost
 * parameters — deliberately not MODERATE/SENSITIVE, since this needs to
 * run acceptably on a Pi Zero 2 W, not just a dev machine.
 *
 * File format: [salt (crypto_pwhash_SALTBYTES)] [nonce
 * (crypto_secretbox_NONCEBYTES)] [ciphertext+MAC]. Authenticated
 * encryption means a wrong PIN or a corrupted/tampered file both fail
 * decryption cleanly and detectably — verified in the test suite, not
 * just asserted.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Not a production storage-location decision -- Settings' Wi-Fi/Storage
   rows are still stubs, so there's no real config layer yet to decide
   this properly. Defined once here so callers (wallet.c,
   import_recovery.c) can't drift out of sync by each hardcoding their
   own copy of the path string. */
#define TEZOS_STORE_DEFAULT_SIGNER_PATH "/tmp/tezos_cyberdeck_signer_store.bin"

#define TEZOS_STORE_MAX_MNEMONIC_LEN 256
#define TEZOS_STORE_MAX_PASSPHRASE_LEN 64

typedef struct {
    char mnemonic[TEZOS_STORE_MAX_MNEMONIC_LEN];
    int word_count; /* 12 or 24 */
    char passphrase[TEZOS_STORE_MAX_PASSPHRASE_LEN]; /* may be empty */
    uint32_t account; /* HD account index */
} tezos_store_signer_data_t;

/* Encrypts `data` with a key derived from `pin` and writes it to
   `path`. Overwrites any existing file at that path. Returns false on
   a genuine I/O or crypto failure (out of disk space, can't open the
   path, etc.) -- not on "wrong PIN", since there's no existing PIN to
   be wrong about when saving. */
bool tezos_store_save_signer(const char *path, const char *pin, const tezos_store_signer_data_t *data);

/* Attempts to decrypt `path` using a key derived from `pin`. Returns
   false if the file doesn't exist, the PIN is wrong, or the file is
   corrupted/tampered -- all three are indistinguishable by design
   (authenticated encryption doesn't tell an attacker which one
   happened), so callers should show one generic "couldn't unlock"
   message, not try to guess which case occurred. */
bool tezos_store_load_signer(const char *path, const char *pin, tezos_store_signer_data_t *data_out);

/* Whether a store file exists at all at `path` -- used to decide
   whether to show "enter PIN to unlock" vs. "no recovery phrase saved
   yet" at startup. Doesn't attempt decryption, just checks existence. */
bool tezos_store_exists(const char *path);

/* --- Non-sensitive plain storage (device name, etc.) --- Deliberately
   NOT behind the PIN. Gating something as innocuous as a device
   nickname behind authentication would be poor UX for no real security
   benefit -- this is a plain text file, simple key=value lines. */
bool tezos_store_save_plain(const char *path, const char *key, const char *value);
bool tezos_store_load_plain(const char *path, const char *key, char *value_out, size_t value_cap);
