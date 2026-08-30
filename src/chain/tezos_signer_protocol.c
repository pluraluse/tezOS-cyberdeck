#include "tezos_signer_protocol.h"
#include "tezos_signer.h"
#include "tezos_base58.h"
#include <string.h>

#define FRAME_START 0x01

void tezos_signer_frame_encode(tezos_bytes_t *out, uint8_t type,
                               const uint8_t *payload, size_t payload_len) {
    uint16_t len = (uint16_t)(payload_len + 1); /* +1 for TYPE */
    tezos_bytes_append_byte(out, FRAME_START);
    tezos_bytes_append_byte(out, (uint8_t)(len >> 8));
    tezos_bytes_append_byte(out, (uint8_t)len);
    tezos_bytes_append_byte(out, type);
    if (payload_len) tezos_bytes_append(out, payload, payload_len);

    uint8_t checksum = type;
    for (size_t i = 0; i < payload_len; i++) checksum ^= payload[i];
    tezos_bytes_append_byte(out, checksum);
}

size_t tezos_signer_frame_decode(const uint8_t *data, size_t data_len,
                                 uint8_t *type_out, const uint8_t **payload_out,
                                 size_t *payload_len_out) {
    if (data_len < 5) return 0; /* START+LEN(2)+TYPE+CHECKSUM minimum, empty payload */
    if (data[0] != FRAME_START) return 0;

    uint16_t len = (uint16_t)((data[1] << 8) | data[2]);
    if (len < 1) return 0; /* must at least cover TYPE */
    size_t frame_total = 3 + len + 1; /* START+LEN(2) + (TYPE+PAYLOAD) + CHECKSUM */
    if (data_len < frame_total) return 0; /* not enough data yet */

    uint8_t type = data[3];
    const uint8_t *payload = data + 4;
    size_t payload_len = len - 1;
    uint8_t received_checksum = data[3 + len];

    uint8_t computed = type;
    for (size_t i = 0; i < payload_len; i++) computed ^= payload[i];
    if (computed != received_checksum) return 0; /* corrupted frame */

    *type_out = type;
    *payload_out = payload;
    *payload_len_out = payload_len;
    return frame_total;
}

/* --- Request encoders --- */

void tezos_signer_encode_ping(tezos_bytes_t *out) {
    tezos_signer_frame_encode(out, TEZOS_SIGNER_MSG_PING, NULL, 0);
}

static void append_u32_be(tezos_bytes_t *b, uint32_t v) {
    uint8_t bytes[4] = { (uint8_t)(v>>24), (uint8_t)(v>>16), (uint8_t)(v>>8), (uint8_t)v };
    tezos_bytes_append(b, bytes, 4);
}

void tezos_signer_encode_get_address(tezos_bytes_t *out, uint32_t account_index) {
    tezos_bytes_t payload; tezos_bytes_init(&payload);
    append_u32_be(&payload, account_index);
    tezos_signer_frame_encode(out, TEZOS_SIGNER_MSG_GET_ADDRESS, payload.data, payload.len);
    tezos_bytes_free(&payload);
}

void tezos_signer_encode_sign_request(tezos_bytes_t *out, uint32_t account_index,
                                      const uint8_t *forged_bytes, size_t forged_len) {
    tezos_bytes_t payload; tezos_bytes_init(&payload);
    append_u32_be(&payload, account_index);
    tezos_bytes_append_byte(&payload, (uint8_t)(forged_len >> 8));
    tezos_bytes_append_byte(&payload, (uint8_t)forged_len);
    tezos_bytes_append(&payload, forged_bytes, forged_len);
    tezos_signer_frame_encode(out, TEZOS_SIGNER_MSG_SIGN_REQUEST, payload.data, payload.len);
    tezos_bytes_free(&payload);
}

/* --- Response decoders --- */

bool tezos_signer_decode_pong(const uint8_t *payload, size_t len, char *version_out, size_t version_cap) {
    if (len < 1) return false;
    uint8_t vlen = payload[0];
    if (len < (size_t)(1 + vlen) || (size_t)(vlen + 1) > version_cap) return false;
    memcpy(version_out, payload + 1, vlen);
    version_out[vlen] = '\0';
    return true;
}

bool tezos_signer_decode_address_response(const uint8_t *payload, size_t len, char *address_out, size_t address_cap) {
    if (len + 1 > address_cap) return false;
    memcpy(address_out, payload, len);
    address_out[len] = '\0';
    return true;
}

bool tezos_signer_decode_sign_response(const uint8_t *payload, size_t len, uint8_t sig_out[64]) {
    if (len != 64) return false;
    memcpy(sig_out, payload, 64);
    return true;
}

bool tezos_signer_decode_error(const uint8_t *payload, size_t len, uint8_t *code_out, char *msg_out, size_t msg_cap) {
    if (len < 2) return false;
    *code_out = payload[0];
    uint8_t mlen = payload[1];
    if (len < (size_t)(2 + mlen) || (size_t)(mlen + 1) > msg_cap) return false;
    memcpy(msg_out, payload + 2, mlen);
    msg_out[mlen] = '\0';
    return true;
}

/* --- Simulation --- */

void tezos_signer_sim_init(tezos_signer_sim_t *sim, const char *mnemonic, int word_count,
                           const char *passphrase) {
    strncpy(sim->mnemonic, mnemonic, TEZOS_BIP39_MAX_MNEMONIC_LEN - 1);
    sim->mnemonic[TEZOS_BIP39_MAX_MNEMONIC_LEN - 1] = '\0';
    sim->word_count = word_count;
    strncpy(sim->passphrase, passphrase ? passphrase : "", sizeof sim->passphrase - 1);
    sim->passphrase[sizeof sim->passphrase - 1] = '\0';
}

static void encode_error(tezos_bytes_t *response_out, uint8_t code, const char *msg) {
    tezos_bytes_t payload; tezos_bytes_init(&payload);
    tezos_bytes_append_byte(&payload, code);
    uint8_t mlen = (uint8_t)strlen(msg);
    tezos_bytes_append_byte(&payload, mlen);
    tezos_bytes_append(&payload, (const uint8_t *)msg, mlen);
    tezos_signer_frame_encode(response_out, TEZOS_SIGNER_MSG_ERROR, payload.data, payload.len);
    tezos_bytes_free(&payload);
}

bool tezos_signer_sim_process(tezos_signer_sim_t *sim, uint8_t request_type,
                              const uint8_t *request_payload, size_t request_payload_len,
                              tezos_bytes_t *response_out) {
    switch (request_type) {
        case TEZOS_SIGNER_MSG_PING: {
            const char *version = "tezos-signer-sim/0.1.0";
            tezos_bytes_t payload; tezos_bytes_init(&payload);
            tezos_bytes_append_byte(&payload, (uint8_t)strlen(version));
            tezos_bytes_append(&payload, (const uint8_t *)version, strlen(version));
            tezos_signer_frame_encode(response_out, TEZOS_SIGNER_MSG_PONG, payload.data, payload.len);
            tezos_bytes_free(&payload);
            return true;
        }

        case TEZOS_SIGNER_MSG_GET_ADDRESS: {
            if (request_payload_len != 4) { encode_error(response_out, TEZOS_SIGNER_ERR_MALFORMED, "bad GET_ADDRESS payload length"); return true; }
            uint32_t account = ((uint32_t)request_payload[0] << 24) | ((uint32_t)request_payload[1] << 16)
                              | ((uint32_t)request_payload[2] << 8) | request_payload[3];

            tezos_mock_signer_t signer;
            if (!tezos_mock_signer_from_mnemonic(&signer, sim->mnemonic, sim->word_count, sim->passphrase, account)) {
                encode_error(response_out, TEZOS_SIGNER_ERR_INTERNAL, "derivation failed");
                return true;
            }
            char address[128];
            tezos_mock_signer_address_b58(&signer, address, sizeof address);
            tezos_signer_frame_encode(response_out, TEZOS_SIGNER_MSG_ADDRESS_RESPONSE,
                                      (const uint8_t *)address, strlen(address));
            return true;
        }

        case TEZOS_SIGNER_MSG_SIGN_REQUEST: {
            if (request_payload_len < 6) { encode_error(response_out, TEZOS_SIGNER_ERR_MALFORMED, "bad SIGN_REQUEST payload"); return true; }
            uint32_t account = ((uint32_t)request_payload[0] << 24) | ((uint32_t)request_payload[1] << 16)
                              | ((uint32_t)request_payload[2] << 8) | request_payload[3];
            uint16_t forged_len = (uint16_t)((request_payload[4] << 8) | request_payload[5]);
            if (request_payload_len != (size_t)(6 + forged_len)) {
                encode_error(response_out, TEZOS_SIGNER_ERR_MALFORMED, "forged_len mismatch"); return true;
            }

            /* *** SIMULATION GAP, documented in SIGNER-PROTOCOL.md: real
               firmware waits for physical confirmation here. This
               auto-confirms immediately -- there is no button in
               software. *** */
            tezos_mock_signer_t signer;
            if (!tezos_mock_signer_from_mnemonic(&signer, sim->mnemonic, sim->word_count, sim->passphrase, account)) {
                encode_error(response_out, TEZOS_SIGNER_ERR_INTERNAL, "derivation failed");
                return true;
            }
            tezos_bytes_t forged; tezos_bytes_init(&forged);
            tezos_bytes_append(&forged, request_payload + 6, forged_len);

            char sig_b58[128];
            if (!tezos_mock_signer_sign(&signer, &forged, sig_b58, sizeof sig_b58)) {
                tezos_bytes_free(&forged);
                encode_error(response_out, TEZOS_SIGNER_ERR_INTERNAL, "signing failed");
                return true;
            }
            /* the raw 64-byte signature is the last 64 bytes of `forged`
               now (tezos_mock_signer_sign appends it in place) */
            const uint8_t *raw_sig = forged.data + (forged.len - 64);
            tezos_signer_frame_encode(response_out, TEZOS_SIGNER_MSG_SIGN_RESPONSE, raw_sig, 64);
            tezos_bytes_free(&forged);
            return true;
        }

        default:
            encode_error(response_out, TEZOS_SIGNER_ERR_UNKNOWN_MSG, "unknown message type");
            return true;
    }
}
