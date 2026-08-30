#pragma once
/*
 * Beacon message schema — the JSON payloads exchanged after the crypto
 * handshake (see tezos_beacon_crypto.h). Field names and the message
 * envelope verified against the real beacon-sdk TypeScript source
 * (packages/beacon-types/src/types/beacon/messages -- BeaconMessageType.ts,
 * BeaconMessageType.ts) — not guessed. See
 * docs/build-notes/beacon-build-notes.md.
 *
 * Scoped to what this device needs as a WALLET (the receiving/responding
 * side): parse incoming PermissionRequest/OperationRequest from a dApp,
 * build outgoing PermissionResponse/OperationResponse. Not building the
 * dApp-side request-construction functions, since this device is never
 * the dApp side of a Beacon pairing.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TEZOS_BEACON_MAX_ID_LEN 64
#define TEZOS_BEACON_MAX_NAME_LEN 64
#define TEZOS_BEACON_MAX_OPS 8

typedef struct {
    char type[32];        /* "permission_request", "operation_request", etc. --
                              see BeaconMessageType in the real SDK */
    char version[16];
    char id[TEZOS_BEACON_MAX_ID_LEN];
    char sender_id[TEZOS_BEACON_MAX_ID_LEN];
} tezos_beacon_envelope_t;

typedef struct {
    tezos_beacon_envelope_t envelope;
    char app_name[TEZOS_BEACON_MAX_NAME_LEN];
    char app_sender_id[TEZOS_BEACON_MAX_ID_LEN];
    char network_type[32]; /* "mainnet", "ghostnet", etc. */
} tezos_beacon_permission_request_t;

/* A single operation within an OperationRequest's operationDetails
   array. Scoped to what our forging layer already supports (plain
   transfers + entrypoint calls with an int parameter) -- see
   tezos_forge.h/tezos_micheline.h. Real Beacon operationDetails can be
   richer; this covers the actual cases this device can act on. */
typedef struct {
    char kind[32];        /* "transaction" */
    char destination[40];
    char amount[32];      /* decimal mutez string */
    bool has_entrypoint;
    char entrypoint[32];
    char parameter_int[32]; /* decimal string, if the entrypoint takes a single int */
} tezos_beacon_op_t;

typedef struct {
    tezos_beacon_envelope_t envelope;
    char network_type[32];
    char source_address[40];
    tezos_beacon_op_t operations[TEZOS_BEACON_MAX_OPS];
    int operation_count;
} tezos_beacon_operation_request_t;

/* --- Parsing incoming requests --- */
bool tezos_beacon_parse_permission_request(const char *json, tezos_beacon_permission_request_t *out);
bool tezos_beacon_parse_operation_request(const char *json, tezos_beacon_operation_request_t *out);

/* --- Building outgoing responses ---
   `json_out` receives a newly-allocated string (caller frees with
   free()), matching cJSON_Print's ownership convention rather than
   requiring the caller to guess a buffer size for arbitrarily-nested
   JSON. */
bool tezos_beacon_build_permission_response(const tezos_beacon_permission_request_t *request,
                                            const char *wallet_name, const char *public_key,
                                            const char *address, char **json_out);
bool tezos_beacon_build_operation_response(const tezos_beacon_operation_request_t *request,
                                           const char *transaction_hash, char **json_out);
