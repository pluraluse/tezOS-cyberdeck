#pragma once
/*
 * Beacon (TZIP-10) pairing crypto — the "cryptobox" seal/open scheme
 * Beacon uses for its initial (pre-shared-secret) P2P handshake
 * messages, plus the symmetric secretbox scheme used for ongoing
 * communication once a shared key exists.
 *
 * Verified byte-for-byte against the real beacon-sdk source
 * (packages/beacon-utils/src/utils/crypto.ts: getKeypairFromSeed,
 * sealCryptobox, openCryptobox) and an independent PyNaCl
 * reimplementation of that exact algorithm — see
 * docs/build-notes/beacon-build-notes.md for the captured vectors and
 * commit reference.
 *
 * The algorithm, precisely:
 *   - Identity keypair: Ed25519, seeded by BLAKE2b-256(seed_string) —
 *     NOT the same keypair as a Tezos account; this is Beacon's own
 *     peer identity, separate from any tz1 address.
 *   - seal(payload, their_ed25519_pk): converts their Ed25519 pk to
 *     X25519, generates a FRESH ephemeral X25519 keypair, derives a
 *     DETERMINISTIC nonce = BLAKE2b-24(ephemeral_pk || their_x25519_pk)
 *     (not random -- this is a real deviation from typical NaCl usage,
 *     verified intentional by matching the reference exactly), then a
 *     standard crypto_box. Output = ephemeral_pk || ciphertext.
 *   - open(payload, my_ed25519_pk, my_ed25519_sk): the receiving side,
 *     mirrors the nonce derivation using its own X25519-converted keys.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint8_t public_key[32]; /* Ed25519 */
    uint8_t secret_key[64]; /* Ed25519, libsodium expanded form */
} tezos_beacon_keypair_t;

/* Derives Beacon's peer identity keypair from a seed string --
   Ed25519 seeded by BLAKE2b-256(seed). This is a SEPARATE identity from
   any Tezos account keypair (see tezos_signer.h) -- don't conflate the
   two, they serve different purposes (this one identifies a Beacon
   peer/pairing session, not a signing account). */
void tezos_beacon_keypair_from_seed(const char *seed_str, tezos_beacon_keypair_t *out);

/* Seals `payload` (arbitrary bytes) for `their_pk` (their Ed25519
   public key) -- used for the initial pairing handshake, before a
   shared secret exists. Appends ephemeral_pk(32) + ciphertext to `out`.
   Generates a fresh ephemeral keypair internally each call. */
bool tezos_beacon_seal(const uint8_t *payload, size_t payload_len,
                       const uint8_t their_pk[32],
                       uint8_t *out, size_t out_cap, size_t *out_len);

/* Opens a sealed payload using this peer's own keypair. */
bool tezos_beacon_open(const uint8_t *sealed, size_t sealed_len,
                       const tezos_beacon_keypair_t *my_keypair,
                       uint8_t *out, size_t out_cap, size_t *out_len);

/* --- The rest of the pieces needed for ONGOING communication after
   pairing (seal/open above is for the initial handshake only) ---
   Verified against the real beacon-sdk source
   (CommunicationClient.ts: createCryptoBoxClient/Server using
   @stablelib/x25519-session's clientSessionKeys/serverSessionKeys --
   the same standard construction as libsodium's crypto_kx, whose own
   primitive name is literally "x25519blake2b"; crypto.ts:
   encryptCryptoboxPayload/decryptCryptoboxPayload for the secretbox
   scheme; get-sender-id.ts / getHexHash for the two different address
   hashes Beacon uses).

   HONEST VERIFICATION NOTE, different tier than seal/open above: this
   session-key derivation was NOT independently cross-checked against a
   non-libsodium implementation this session (unlike seal/open, which
   was verified against real PyNaCl output). It implements the
   standard, named crypto_kx construction correctly and is tested for
   the property that must hold for ANY correct X25519 key exchange
   (client's tx == server's rx and vice versa), but that's a weaker
   claim than "confirmed byte-identical to an independent
   implementation." Flagged here rather than glossed over. */

typedef struct {
    uint8_t tx[32]; /* this side's send key */
    uint8_t rx[32]; /* this side's receive key */
} tezos_beacon_session_keys_t;

/* Derives session keys as the client (the side that initiates
   sendMessage -- see beacon-sdk's sendMessage()). */
bool tezos_beacon_session_keys_client(const tezos_beacon_keypair_t *my_keypair,
                                      const uint8_t their_pk[32],
                                      tezos_beacon_session_keys_t *out);
/* Derives session keys as the server (the side that's
   listenForEncryptedMessage()-ing). Algebraically: client.tx == server.rx
   and client.rx == server.tx, for the same two keypairs -- tested. */
bool tezos_beacon_session_keys_server(const tezos_beacon_keypair_t *my_keypair,
                                      const uint8_t their_pk[32],
                                      tezos_beacon_session_keys_t *out);

/* getSenderId: bs58check(BLAKE2b(pubkey, 5 bytes)) -- used in the
   message envelope's senderId field. */
bool tezos_beacon_sender_id(const uint8_t pk[32], char *out, size_t out_cap);
/* getHexHash: hex(BLAKE2b(pubkey, 32 bytes)) -- used SPECIFICALLY for
   Matrix room addressing ("@{hexhash}:{relayServer}"), a DIFFERENT hash
   than sender_id above -- verified as genuinely different in
   beacon-build-notes.md's test, not a copy-paste mistake. */
bool tezos_beacon_hex_hash(const uint8_t pk[32], char *out, size_t out_cap);

/* encryptCryptoboxPayload: random 24-byte nonce || secretbox(message,
   key, nonce), then hex-encoded for transmission as Matrix message
   text. `tx_key` is the sending side's session key (tx from
   session_keys_client/server above). */
bool tezos_beacon_secretbox_encrypt(const char *message, const uint8_t tx_key[32],
                                    char *hex_out, size_t hex_out_cap);
/* Reverse. `rx_key` is the receiving side's session key. */
bool tezos_beacon_secretbox_decrypt(const char *hex_payload, const uint8_t rx_key[32],
                                    char *message_out, size_t message_out_cap);
