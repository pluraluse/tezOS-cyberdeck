#include "tezos_matrix.h"
#include "tezos_http.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

bool tezos_matrix_register_guest(const char *homeserver, tezos_matrix_client_t *client_out) {
    memset(client_out, 0, sizeof *client_out);
    strncpy(client_out->homeserver, homeserver, sizeof client_out->homeserver - 1);

    char url[640];
    snprintf(url, sizeof url, "%s/_matrix/client/r0/register?kind=guest", homeserver);

    tezos_http_response_t resp;
    if (!tezos_http_post(url, "{}", &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; }

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    cJSON *access_token = cJSON_GetObjectItemCaseSensitive(root, "access_token");
    cJSON *user_id = cJSON_GetObjectItemCaseSensitive(root, "user_id");
    cJSON *device_id = cJSON_GetObjectItemCaseSensitive(root, "device_id");

    bool ok = cJSON_IsString(access_token) && cJSON_IsString(user_id);
    if (ok) {
        strncpy(client_out->access_token, access_token->valuestring, sizeof client_out->access_token - 1);
        strncpy(client_out->user_id, user_id->valuestring, sizeof client_out->user_id - 1);
        if (cJSON_IsString(device_id)) strncpy(client_out->device_id, device_id->valuestring, sizeof client_out->device_id - 1);
    }
    cJSON_Delete(root);
    return ok;
}

bool tezos_matrix_create_room(const tezos_matrix_client_t *client, const char *invite_user_id,
                              char *room_id_out, size_t room_id_cap) {
    cJSON *body = cJSON_CreateObject();
    cJSON *invite = cJSON_CreateArray();
    cJSON_AddItemToArray(invite, cJSON_CreateString(invite_user_id));
    cJSON_AddItemToObject(body, "invite", invite);
    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!body_str) return false;

    char url[640];
    snprintf(url, sizeof url, "%s/_matrix/client/r0/createRoom?access_token=%s",
             client->homeserver, client->access_token);

    tezos_http_response_t resp;
    bool sent = tezos_http_post(url, body_str, &resp);
    free(body_str);
    if (!sent) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; }

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    cJSON *room_id = cJSON_GetObjectItemCaseSensitive(root, "room_id");
    bool ok = cJSON_IsString(room_id);
    if (ok) { strncpy(room_id_out, room_id->valuestring, room_id_cap - 1); room_id_out[room_id_cap-1] = '\0'; }
    cJSON_Delete(root);
    return ok;
}

bool tezos_matrix_send_event(const tezos_matrix_client_t *client, const char *room_id,
                             const char *event_type, const char *body_json, const char *txn_id) {
    /* Matrix room IDs (start with '!') need URL-encoding of the ':' and
       other special characters in a fully general client -- kept simple
       here since Beacon's own relay servers are well-formed hostnames
       and this isn't handling arbitrary user input, but flagged as a
       real simplification if this client is ever used more generally. */
    char url[900];
    snprintf(url, sizeof url, "%s/_matrix/client/r0/rooms/%s/send/%s/%s?access_token=%s",
             client->homeserver, room_id, event_type, txn_id, client->access_token);

    tezos_http_response_t resp;
    if (!tezos_http_put(url, body_json, &resp)) return false;
    bool ok = (resp.status_code == 200);
    tezos_http_response_free(&resp);
    return ok;
}

bool tezos_matrix_sync(tezos_matrix_client_t *client, int timeout_ms,
                       char messages_out[TEZOS_MATRIX_MAX_MESSAGES][2048], int *message_count) {
    *message_count = 0;

    char url[900];
    if (client->sync_since[0]) {
        snprintf(url, sizeof url, "%s/_matrix/client/r0/sync?access_token=%s&timeout=%d&since=%s",
                 client->homeserver, client->access_token, timeout_ms, client->sync_since);
    } else {
        /* first sync: no `since` token yet, per the spec -- this
           returns current room state as a baseline rather than
           incremental changes */
        snprintf(url, sizeof url, "%s/_matrix/client/r0/sync?access_token=%s&timeout=%d",
                 client->homeserver, client->access_token, timeout_ms);
    }

    tezos_http_response_t resp;
    if (!tezos_http_get(url, &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; }

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    cJSON *next_batch = cJSON_GetObjectItemCaseSensitive(root, "next_batch");
    if (cJSON_IsString(next_batch)) {
        strncpy(client->sync_since, next_batch->valuestring, sizeof client->sync_since - 1);
    }

    /* rooms.join.{roomId}.timeline.events[] -- per the spec's own
       documented /sync response shape */
    cJSON *rooms = cJSON_GetObjectItemCaseSensitive(root, "rooms");
    cJSON *join = rooms ? cJSON_GetObjectItemCaseSensitive(rooms, "join") : NULL;
    if (cJSON_IsObject(join)) {
        cJSON *room;
        cJSON_ArrayForEach(room, join) {
            if (*message_count >= TEZOS_MATRIX_MAX_MESSAGES) break;
            cJSON *timeline = cJSON_GetObjectItemCaseSensitive(room, "timeline");
            cJSON *events = timeline ? cJSON_GetObjectItemCaseSensitive(timeline, "events") : NULL;
            if (!cJSON_IsArray(events)) continue;
            cJSON *event;
            cJSON_ArrayForEach(event, events) {
                if (*message_count >= TEZOS_MATRIX_MAX_MESSAGES) break;
                cJSON *type = cJSON_GetObjectItemCaseSensitive(event, "type");
                if (!cJSON_IsString(type) || strcmp(type->valuestring, "m.room.message") != 0) continue;
                cJSON *content = cJSON_GetObjectItemCaseSensitive(event, "content");
                cJSON *body = content ? cJSON_GetObjectItemCaseSensitive(content, "body") : NULL;
                if (cJSON_IsString(body)) {
                    strncpy(messages_out[*message_count], body->valuestring, 2047);
                    messages_out[*message_count][2047] = '\0';
                    (*message_count)++;
                }
            }
        }
    }

    cJSON_Delete(root);
    return true;
}
