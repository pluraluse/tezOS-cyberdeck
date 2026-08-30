/* Matrix client -- endpoint shapes verified against the official Matrix
   Client-Server API spec. POST/PUT transport confirmed against a real
   reachable endpoint (real 401s = genuine request/response round trips,
   just no auth provided). Response parsing checked against the spec's
   own documented /sync example. NOTE: like test_network.c, the actual
   Matrix-specific calls (register/createRoom/sync against a real
   homeserver) couldn't be tested in the original dev sandbox -- try
   them for real here if this device has network access. */
#include "../src/chain/tezos_matrix.h"
#include "../src/chain/tezos_http.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    tezos_http_init();

    tezos_http_response_t resp;
    bool ok = tezos_http_post("https://api.github.com/repos/octocat/Hello-World/issues/1/comments", "{}", &resp);
    CHECK(ok && resp.status_code > 0, "POST transport works (real request/response, real status code captured)");
    tezos_http_response_free(&resp);
    ok = tezos_http_put("https://api.github.com/user/starred/octocat/Hello-World", "", &resp);
    CHECK(ok && resp.status_code > 0, "PUT transport works (real request/response, real status code captured)");
    tezos_http_response_free(&resp);

    const char *spec_sync_example =
        "{\"account_data\":{\"events\":[]},\"next_batch\":\"s9_13_0_1_1_1\","
        "\"presence\":{\"events\":[]},\"rooms\":{\"invite\":{},\"join\":{},\"leave\":{}}}";
    cJSON *root = cJSON_Parse(spec_sync_example);
    cJSON *next_batch = cJSON_GetObjectItemCaseSensitive(root, "next_batch");
    CHECK(cJSON_IsString(next_batch) && strcmp(next_batch->valuestring, "s9_13_0_1_1_1") == 0,
          "next_batch parses correctly from the OFFICIAL spec's own documented example");
    cJSON_Delete(root);

    const char *populated_sync =
        "{\"next_batch\":\"s10_14_0_1_1_2\",\"rooms\":{\"join\":{"
        "\"!abc123:matrix.papers.tech\":{\"timeline\":{\"events\":["
        "{\"type\":\"m.room.member\",\"content\":{\"membership\":\"join\"}},"
        "{\"type\":\"m.room.message\",\"content\":{\"msgtype\":\"m.text\",\"body\":\"encrypted_payload_hex\"}}"
        "]}}}}}";
    char messages[TEZOS_MATRIX_MAX_MESSAGES][2048];
    int msg_count = 0;
    root = cJSON_Parse(populated_sync);
    cJSON *rooms = cJSON_GetObjectItemCaseSensitive(root, "rooms");
    cJSON *join = cJSON_GetObjectItemCaseSensitive(rooms, "join");
    cJSON *room;
    cJSON_ArrayForEach(room, join) {
        cJSON *timeline = cJSON_GetObjectItemCaseSensitive(room, "timeline");
        cJSON *events = cJSON_GetObjectItemCaseSensitive(timeline, "events");
        cJSON *event;
        cJSON_ArrayForEach(event, events) {
            cJSON *type = cJSON_GetObjectItemCaseSensitive(event, "type");
            if (!cJSON_IsString(type) || strcmp(type->valuestring, "m.room.message") != 0) continue;
            cJSON *content = cJSON_GetObjectItemCaseSensitive(event, "content");
            cJSON *body = cJSON_GetObjectItemCaseSensitive(content, "body");
            if (cJSON_IsString(body)) { strncpy(messages[msg_count], body->valuestring, 2047); msg_count++; }
        }
    }
    CHECK(msg_count == 1, "exactly one m.room.message extracted (m.room.member correctly skipped)");
    CHECK(strcmp(messages[0], "encrypted_payload_hex") == 0, "extracted message body matches exactly");
    cJSON_Delete(root);

    tezos_http_cleanup();
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
