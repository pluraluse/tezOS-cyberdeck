#pragma once
/*
 * Matrix client — the transport Beacon actually routes messages
 * through (see docs/build-notes/beacon-build-notes.md's "network hop"
 * section for why this exists as a separate layer). Endpoint paths and
 * field names verified against the official Matrix Client-Server API
 * specification (spec.matrix.org/latest/client-server-api/) — direct
 * primary source, not a secondary description.
 *
 * HONEST STATUS, stated plainly: this is a real, spec-shaped client —
 * request construction, response parsing, and error handling are all
 * real code, not stubs. What it is NOT: live-tested against a real
 * Matrix homeserver. This sandbox's network access doesn't reach one
 * (same restriction that applies to TZKT). Verified here instead: JSON
 * request/response shapes checked against the official spec's own
 * documented examples, and (like tezos_http.c) the underlying HTTP
 * transport mechanics were separately verified against a real,
 * reachable endpoint.
 *
 * ALSO NOT YET DONE: confirming this generic Matrix usage exactly
 * matches beacon-sdk's specific choices (event type for encrypted
 * payloads, guest-vs-deterministic registration, homeserver selection
 * logic — packages/beacon-transport-matrix/src/communication-client/
 * P2PCommunicationClient.ts in the real source). Built against the
 * general, confirmed-correct Matrix API; the Beacon-specific parameter
 * choices need a final check against that file before this is "Beacon-
 * compliant" rather than "a correct generic Matrix client."
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char homeserver[256];    /* e.g. "https://matrix.papers.tech" -- from
                                 the pairing QR's relayServer field */
    char access_token[256];
    char user_id[128];
    char device_id[64];
    char sync_since[128];    /* next_batch token from the last /sync call,
                                 empty until the first sync completes */
} tezos_matrix_client_t;

/* POST /register?kind=guest -- no password needed, matching Beacon's
   deterministic-identity model rather than a real user signup flow.
   Populates access_token/user_id/device_id on success. */
bool tezos_matrix_register_guest(const char *homeserver, tezos_matrix_client_t *client_out);

/* POST /createRoom, inviting `invite_user_id` directly (Beacon's
   recipientString format: "@{recipientHash}:{relayServer}" -- built by
   the caller, this function just takes the final string). */
bool tezos_matrix_create_room(const tezos_matrix_client_t *client, const char *invite_user_id,
                              char *room_id_out, size_t room_id_cap);

/* PUT /rooms/{roomId}/send/{eventType}/{txnId} -- sends `body_json`
   (already-serialized JSON, e.g. from tezos_beacon_message.h's builders)
   as the content of a room event. txn_id is caller-supplied since the
   spec requires client-generated idempotency keys (retries with the
   same txn_id are deduplicated server-side, per the spec's own
   explanation of why this exists). */
bool tezos_matrix_send_event(const tezos_matrix_client_t *client, const char *room_id,
                             const char *event_type, const char *body_json, const char *txn_id);

/* GET /sync?since=...&timeout=... -- long-polls for new events. Updates
   client->sync_since with the response's next_batch token for the next
   call. `messages_out`/`message_count` receive the raw message-event
   content strings found across all joined rooms (caller parses further
   with tezos_beacon_message.h). Returns true even if zero new messages
   arrived (that's a normal sync outcome, not a failure) -- check
   *message_count to distinguish. */
#define TEZOS_MATRIX_MAX_MESSAGES 16
bool tezos_matrix_sync(tezos_matrix_client_t *client, int timeout_ms,
                       char messages_out[TEZOS_MATRIX_MAX_MESSAGES][2048], int *message_count);
