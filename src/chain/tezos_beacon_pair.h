#pragma once
/*
 * The actual pairing + send/receive flow, tying together the three
 * previously-separate, individually-verified layers:
 *   - tezos_beacon_crypto.h (identity keypair, session keys, secretbox)
 *   - tezos_beacon_message.h (the JSON message schema)
 *   - tezos_matrix.h (the transport)
 * plus tezos_beacon_qr.h for the peer info that starts this whole flow
 * (from a scanned pairing QR -- see src/apps/camera/camera.c).
 *
 * See docs/build-notes/beacon-build-notes.md for the full verification
 * breakdown. Short version: the crypto chain (session key exchange +
 * secretbox encrypt/decrypt + real message JSON) is verified end-to-end
 * between two independently-simulated peers in this file's test suite.
 * The Matrix-touching functions here are real, spec-verified code that
 * cannot be live-tested from this environment (no reachable homeserver)
 * -- same honest limitation as every other network-touching piece in
 * this project.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "tezos_beacon_crypto.h"
#include "tezos_beacon_qr.h"
#include "tezos_matrix.h"

typedef struct {
    tezos_beacon_keypair_t my_keypair;
    tezos_beacon_peer_info_t peer;       /* from a scanned pairing QR */
    tezos_matrix_client_t matrix;
    char recipient[192];                 /* "@{hexhash}:{relayServer}" */
    char room_id[128];
    bool paired;
} tezos_beacon_session_t;

/* Step 1: derive this device's Beacon identity and compute the Matrix
   recipient address for the scanned peer. Pure local computation, no
   network -- fully testable, and tested. */
bool tezos_beacon_prepare(tezos_beacon_session_t *session, const char *my_seed,
                          const tezos_beacon_peer_info_t *peer);

/* Step 2: register on the peer's relay server and create/join the
   pairing room. Real Matrix calls -- cannot be live-verified here (see
   file header). Sets session->room_id and session->paired on success. */
bool tezos_beacon_connect(tezos_beacon_session_t *session);

/* Step 3: send a Beacon message (already-built JSON, e.g. from
   tezos_beacon_build_permission_response()) -- derives client-role
   session keys, secretbox-encrypts, sends via Matrix. Real network call,
   same caveat as tezos_beacon_connect. */
bool tezos_beacon_send(tezos_beacon_session_t *session, const char *json_payload);

/* Step 4: poll for and decrypt incoming messages -- derives server-role
   session keys, syncs Matrix, secretbox-decrypts each message found.
   Real network call, same caveat. Decrypted JSON strings go in
   `out_messages` for the caller to parse with tezos_beacon_message.h. */
#define TEZOS_BEACON_MAX_MESSAGES 8
bool tezos_beacon_receive(tezos_beacon_session_t *session, int timeout_ms,
                          char out_messages[TEZOS_BEACON_MAX_MESSAGES][2048], int *message_count);
