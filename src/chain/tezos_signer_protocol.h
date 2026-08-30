#pragma once
/*
 * Signer wire protocol — message framing + a software simulation of
 * the signer's request/response processing loop. See
 * docs/architecture/SIGNER-PROTOCOL.md for the full spec this
 * implements; read that first, this header just declares the API.
 *
 * The simulation exists so the REST of the stack (eventually Wallet's
 * real Send flow, Scanner) can be built against the actual wire
 * protocol shape now, rather than calling tezos_signer.h's functions
 * directly — making the eventual swap to real UART hardware a clean
 * transport swap, not an app-logic rewrite.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "tezos_forge.h" /* tezos_bytes_t */
#include "tezos_bip39.h" /* TEZOS_BIP39_MAX_MNEMONIC_LEN */

#define TEZOS_SIGNER_MSG_PING             0x01
#define TEZOS_SIGNER_MSG_PONG             0x81
#define TEZOS_SIGNER_MSG_GET_ADDRESS      0x02
#define TEZOS_SIGNER_MSG_ADDRESS_RESPONSE 0x82
#define TEZOS_SIGNER_MSG_SIGN_REQUEST     0x03
#define TEZOS_SIGNER_MSG_SIGN_RESPONSE    0x83
#define TEZOS_SIGNER_MSG_ERROR            0xFF

#define TEZOS_SIGNER_ERR_MALFORMED   0x01
#define TEZOS_SIGNER_ERR_UNKNOWN_MSG 0x02
#define TEZOS_SIGNER_ERR_REJECTED    0x03
#define TEZOS_SIGNER_ERR_INTERNAL    0x04

/* --- Frame encode/decode --- */

/* Wraps `payload` in the START/LEN/TYPE/PAYLOAD/CHECKSUM framing,
   appending to `out`. */
void tezos_signer_frame_encode(tezos_bytes_t *out, uint8_t type,
                               const uint8_t *payload, size_t payload_len);

/* Parses one frame from `data` (which may contain exactly one frame, or
   more — only the first is consumed). On success, *type and the
   internal payload pointer/length are set, and returns the number of
   bytes consumed from `data` (so a caller streaming multiple frames can
   advance past this one). Returns 0 on failure (malformed frame,
   checksum mismatch, insufficient data) — this is the
   TEZOS_SIGNER_ERR_MALFORMED case from the spec. */
size_t tezos_signer_frame_decode(const uint8_t *data, size_t data_len,
                                 uint8_t *type_out, const uint8_t **payload_out,
                                 size_t *payload_len_out);

/* --- Request encoders (Pi -> Signer) --- */

void tezos_signer_encode_ping(tezos_bytes_t *out);
void tezos_signer_encode_get_address(tezos_bytes_t *out, uint32_t account_index);
void tezos_signer_encode_sign_request(tezos_bytes_t *out, uint32_t account_index,
                                      const uint8_t *forged_bytes, size_t forged_len);

/* --- Response decoders (Signer -> Pi), called on already-frame-decoded payloads --- */

bool tezos_signer_decode_pong(const uint8_t *payload, size_t len, char *version_out, size_t version_cap);
bool tezos_signer_decode_address_response(const uint8_t *payload, size_t len, char *address_out, size_t address_cap);
bool tezos_signer_decode_sign_response(const uint8_t *payload, size_t len, uint8_t sig_out[64]);
bool tezos_signer_decode_error(const uint8_t *payload, size_t len, uint8_t *code_out, char *msg_out, size_t msg_cap);

/* --- Software simulation of the signer side's processing loop ---
   Holds the mnemonic/passphrase standing in for what a real signer's
   secure element would hold; derives fresh per request per the "signer
   never sends the seed" rule. Auto-confirms SIGN_REQUEST immediately --
   see docs/architecture/SIGNER-PROTOCOL.md's explicit note that this is
   a simulation gap, not something real firmware should copy. */
typedef struct {
    char mnemonic[TEZOS_BIP39_MAX_MNEMONIC_LEN];
    int word_count;
    char passphrase[64];
} tezos_signer_sim_t;

void tezos_signer_sim_init(tezos_signer_sim_t *sim, const char *mnemonic, int word_count,
                           const char *passphrase);

/* Processes one already-frame-decoded request (type + payload) and
   appends a fully-framed response to `response_out`. Returns false only
   on a genuine internal failure (never for "user rejected" -- that's a
   valid, correctly-framed ERROR_RESPONSE, not a C-level failure). */
bool tezos_signer_sim_process(tezos_signer_sim_t *sim, uint8_t request_type,
                              const uint8_t *request_payload, size_t request_payload_len,
                              tezos_bytes_t *response_out);
