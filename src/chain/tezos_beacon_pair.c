#include "tezos_beacon_pair.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

bool tezos_beacon_prepare(tezos_beacon_session_t *session, const char *my_seed,
                          const tezos_beacon_peer_info_t *peer) {
    memset(session, 0, sizeof *session);
    tezos_beacon_keypair_from_seed(my_seed, &session->my_keypair);
    session->peer = *peer;

    /* recipient addressing: "@{hexhash of THEIR pubkey}:{their relay
       server}" -- verified against beacon-sdk's sendMessage(), which
       computes getHexHash(peer.publicKey) and recipientString(hash, peer.relayServer) */
    uint8_t their_pk_raw[32];
    if (strlen(peer->public_key) != 64) return false; /* expects 32-byte hex pubkey */
    for (int i = 0; i < 32; i++) {
        unsigned int byte;
        if (sscanf(peer->public_key + i * 2, "%2x", &byte) != 1) return false;
        their_pk_raw[i] = (uint8_t)byte;
    }
    char hex_hash[65];
    if (!tezos_beacon_hex_hash(their_pk_raw, hex_hash, sizeof hex_hash)) return false;

    int n = snprintf(session->recipient, sizeof session->recipient, "@%s:%s", hex_hash, peer->relay_server);
    return n > 0 && (size_t)n < sizeof session->recipient;
}

bool tezos_beacon_connect(tezos_beacon_session_t *session) {
    char homeserver[300];
    snprintf(homeserver, sizeof homeserver, "https://%s", session->peer.relay_server);

    if (!tezos_matrix_register_guest(homeserver, &session->matrix)) return false;
    if (!tezos_matrix_create_room(&session->matrix, session->recipient,
                                  session->room_id, sizeof session->room_id)) return false;

    session->paired = true;
    return true;
}

bool tezos_beacon_send(tezos_beacon_session_t *session, const char *json_payload) {
    uint8_t their_pk_raw[32];
    for (int i = 0; i < 32; i++) {
        unsigned int byte;
        if (sscanf(session->peer.public_key + i * 2, "%2x", &byte) != 1) return false;
        their_pk_raw[i] = (uint8_t)byte;
    }

    tezos_beacon_session_keys_t keys;
    if (!tezos_beacon_session_keys_client(&session->my_keypair, their_pk_raw, &keys)) return false;

    char hex_ciphertext[4096];
    if (!tezos_beacon_secretbox_encrypt(json_payload, keys.tx, hex_ciphertext, sizeof hex_ciphertext)) return false;

    cJSON *content = cJSON_CreateObject();
    cJSON_AddStringToObject(content, "msgtype", "m.text");
    cJSON_AddStringToObject(content, "body", hex_ciphertext);
    char *body_json = cJSON_PrintUnformatted(content);
    cJSON_Delete(content);
    if (!body_json) return false;

    char txn_id[32];
    snprintf(txn_id, sizeof txn_id, "%ld", (long)time(NULL));

    bool ok = tezos_matrix_send_event(&session->matrix, session->room_id, "m.room.message", body_json, txn_id);
    free(body_json);
    return ok;
}

bool tezos_beacon_receive(tezos_beacon_session_t *session, int timeout_ms,
                          char out_messages[TEZOS_BEACON_MAX_MESSAGES][2048], int *message_count) {
    *message_count = 0;

    uint8_t their_pk_raw[32];
    for (int i = 0; i < 32; i++) {
        unsigned int byte;
        if (sscanf(session->peer.public_key + i * 2, "%2x", &byte) != 1) return false;
        their_pk_raw[i] = (uint8_t)byte;
    }

    tezos_beacon_session_keys_t keys;
    if (!tezos_beacon_session_keys_server(&session->my_keypair, their_pk_raw, &keys)) return false;

    char raw_messages[TEZOS_MATRIX_MAX_MESSAGES][2048];
    int raw_count = 0;
    if (!tezos_matrix_sync(&session->matrix, timeout_ms, raw_messages, &raw_count)) return false;

    for (int i = 0; i < raw_count && *message_count < TEZOS_BEACON_MAX_MESSAGES; i++) {
        if (tezos_beacon_secretbox_decrypt(raw_messages[i], keys.rx,
                                           out_messages[*message_count], 2048)) {
            (*message_count)++;
        }
        /* decrypt failures are silently skipped -- a message that
           doesn't decrypt with this session's key isn't necessarily
           corrupt, it may just be a connection-open control message or
           addressed to a different pairing, per beacon-sdk's own
           try/catch around this exact case in listenForEncryptedMessage */
    }
    return true;
}
