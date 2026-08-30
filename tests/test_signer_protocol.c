/* Signer wire protocol -- frame encode/decode, corruption detection,
   and a full simulated GET_ADDRESS -> SIGN_REQUEST round trip that
   produces a signature matching the direct-API path exactly. */
#include "../src/chain/tezos_signer_protocol.h"
#include "../src/chain/tezos_signer.h"
#include "../src/chain/tezos_forge.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    tezos_bytes_t frame; tezos_bytes_init(&frame);
    uint8_t sample_payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    tezos_signer_frame_encode(&frame, 0x42, sample_payload, sizeof sample_payload);

    uint8_t type; const uint8_t *payload; size_t payload_len;
    size_t consumed = tezos_signer_frame_decode(frame.data, frame.len, &type, &payload, &payload_len);
    CHECK(consumed == frame.len, "frame_decode consumes exactly the whole frame");
    CHECK(type == 0x42 && payload_len == sizeof sample_payload && memcmp(payload, sample_payload, payload_len) == 0,
          "decoded type and payload match exactly");
    tezos_bytes_free(&frame);

    tezos_bytes_init(&frame);
    tezos_signer_frame_encode(&frame, 0x42, sample_payload, sizeof sample_payload);
    frame.data[frame.len - 2] ^= 0xFF;
    consumed = tezos_signer_frame_decode(frame.data, frame.len, &type, &payload, &payload_len);
    CHECK(consumed == 0, "corrupted frame is correctly rejected (checksum mismatch)");
    tezos_bytes_free(&frame);

    tezos_signer_sim_t sim;
    tezos_signer_sim_init(&sim, "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about", 12, "TREZOR");

    tezos_bytes_t req; tezos_bytes_init(&req);
    tezos_signer_encode_get_address(&req, 0);
    tezos_signer_frame_decode(req.data, req.len, &type, &payload, &payload_len);
    tezos_bytes_t resp; tezos_bytes_init(&resp);
    tezos_signer_sim_process(&sim, type, payload, payload_len, &resp);
    uint8_t rtype; const uint8_t *rpayload; size_t rpayload_len;
    tezos_signer_frame_decode(resp.data, resp.len, &rtype, &rpayload, &rpayload_len);
    CHECK(rtype == TEZOS_SIGNER_MSG_ADDRESS_RESPONSE, "GET_ADDRESS gets an ADDRESS_RESPONSE");
    char address[128];
    tezos_signer_decode_address_response(rpayload, rpayload_len, address, sizeof address);

    tezos_mock_signer_t direct_signer;
    tezos_mock_signer_from_mnemonic(&direct_signer, "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about", 12, "TREZOR", 0);
    char direct_address[128];
    tezos_mock_signer_address_b58(&direct_signer, direct_address, sizeof direct_address);
    CHECK(strcmp(address, direct_address) == 0, "address obtained THROUGH THE PROTOCOL matches the direct-API address exactly");
    tezos_bytes_free(&req); tezos_bytes_free(&resp);

    tezos_transaction_params_t tp = {0};
    strncpy(tp.source, address, sizeof tp.source - 1);
    strcpy(tp.fee, "374"); strcpy(tp.counter, "1"); strcpy(tp.gas_limit, "1000");
    strcpy(tp.storage_limit, "0"); strcpy(tp.amount, "1");
    strncpy(tp.destination, address, sizeof tp.destination - 1);
    tezos_bytes_t forged; tezos_bytes_init(&forged);
    tezos_forge_transaction(&forged, &tp);

    tezos_bytes_init(&req);
    tezos_signer_encode_sign_request(&req, 0, forged.data, forged.len);
    tezos_signer_frame_decode(req.data, req.len, &type, &payload, &payload_len);
    tezos_bytes_init(&resp);
    tezos_signer_sim_process(&sim, type, payload, payload_len, &resp);
    tezos_signer_frame_decode(resp.data, resp.len, &rtype, &rpayload, &rpayload_len);
    CHECK(rtype == TEZOS_SIGNER_MSG_SIGN_RESPONSE, "SIGN_REQUEST gets a SIGN_RESPONSE");
    uint8_t sig[64];
    CHECK(tezos_signer_decode_sign_response(rpayload, rpayload_len, sig), "SIGN_RESPONSE decodes to exactly 64 bytes");
    tezos_bytes_append(&forged, sig, 64);
    CHECK(tezos_mock_signer_self_verify(&direct_signer, forged.data, forged.len),
          "signature obtained THROUGH THE PROTOCOL is genuinely valid (self-verifies)");
    tezos_bytes_free(&forged); tezos_bytes_free(&req); tezos_bytes_free(&resp);

    tezos_bytes_init(&resp);
    tezos_signer_sim_process(&sim, 0x77, NULL, 0, &resp);
    tezos_signer_frame_decode(resp.data, resp.len, &rtype, &rpayload, &rpayload_len);
    CHECK(rtype == TEZOS_SIGNER_MSG_ERROR, "unknown message type correctly gets an ERROR_RESPONSE");
    uint8_t err_code; char err_msg[128];
    tezos_signer_decode_error(rpayload, rpayload_len, &err_code, err_msg, sizeof err_msg);
    CHECK(err_code == TEZOS_SIGNER_ERR_UNKNOWN_MSG, "error code is correctly TEZOS_SIGNER_ERR_UNKNOWN_MSG");
    tezos_bytes_free(&resp);

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
