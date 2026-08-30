/* Beacon message schema -- verified against real beacon-sdk TypeScript
   interfaces (PermissionRequest, OperationRequest, BeaconMessageType). */
#include "../src/chain/tezos_beacon_message.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    const char *perm_req_json =
        "{\"type\":\"permission_request\",\"version\":\"3\",\"id\":\"req-123\","
        "\"senderId\":\"peer-abc\",\"appMetadata\":{\"senderId\":\"peer-abc\",\"name\":\"Kukai\"},"
        "\"network\":{\"type\":\"mainnet\"},\"scopes\":[\"operation_request\",\"sign\"]}";
    tezos_beacon_permission_request_t preq;
    CHECK(tezos_beacon_parse_permission_request(perm_req_json, &preq), "parses a real-shaped permission_request");
    CHECK(strcmp(preq.envelope.id, "req-123") == 0, "envelope.id parsed correctly");
    CHECK(strcmp(preq.app_name, "Kukai") == 0, "appMetadata.name parsed correctly");
    CHECK(strcmp(preq.network_type, "mainnet") == 0, "network.type parsed correctly");

    char *resp_json = NULL;
    CHECK(tezos_beacon_build_permission_response(&preq, "tezOS Wallet", "edpk_TEST", "tz1TEST", &resp_json),
          "builds a permission_response successfully");
    cJSON *check = cJSON_Parse(resp_json);
    cJSON *type = cJSON_GetObjectItemCaseSensitive(check, "type");
    CHECK(cJSON_IsString(type) && strcmp(type->valuestring, "permission_response") == 0, "response type correct");
    cJSON *id = cJSON_GetObjectItemCaseSensitive(check, "id");
    CHECK(cJSON_IsString(id) && strcmp(id->valuestring, "req-123") == 0, "response id matches request id");
    cJSON_Delete(check); free(resp_json);

    const char *op_req_json =
        "{\"type\":\"operation_request\",\"version\":\"3\",\"id\":\"req-456\",\"senderId\":\"peer-abc\","
        "\"network\":{\"type\":\"mainnet\"},\"sourceAddress\":\"tz1SOURCE\","
        "\"operationDetails\":[{\"kind\":\"transaction\",\"destination\":\"KT1CONTRACT\","
        "\"amount\":\"0\",\"parameters\":{\"entrypoint\":\"collect\",\"value\":{\"int\":\"4021\"}}}]}";
    tezos_beacon_operation_request_t oreq;
    CHECK(tezos_beacon_parse_operation_request(op_req_json, &oreq), "parses a real-shaped operation_request");
    CHECK(oreq.operation_count == 1, "correct operation count parsed");
    CHECK(strcmp(oreq.operations[0].entrypoint, "collect") == 0, "entrypoint parsed correctly");
    CHECK(strcmp(oreq.operations[0].parameter_int, "4021") == 0, "int parameter value parsed correctly");

    char *op_resp_json = NULL;
    CHECK(tezos_beacon_build_operation_response(&oreq, "onvSomeRealLookingOpHash", &op_resp_json),
          "builds an operation_response successfully");
    check = cJSON_Parse(op_resp_json);
    cJSON *txh = cJSON_GetObjectItemCaseSensitive(check, "transactionHash");
    CHECK(cJSON_IsString(txh) && strcmp(txh->valuestring, "onvSomeRealLookingOpHash") == 0, "transactionHash correct");
    cJSON_Delete(check); free(op_resp_json);

    tezos_beacon_permission_request_t bad;
    CHECK(!tezos_beacon_parse_permission_request("{\"type\":\"operation_request\"}", &bad), "wrong-type message correctly rejected");
    CHECK(!tezos_beacon_parse_permission_request("not even json", &bad), "malformed JSON correctly rejected");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
