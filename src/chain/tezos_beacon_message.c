#include "tezos_beacon_message.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void parse_envelope(cJSON *root, tezos_beacon_envelope_t *env) {
    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    cJSON *sender_id = cJSON_GetObjectItemCaseSensitive(root, "senderId");
    if (cJSON_IsString(type)) strncpy(env->type, type->valuestring, sizeof env->type - 1);
    if (cJSON_IsString(version)) strncpy(env->version, version->valuestring, sizeof env->version - 1);
    if (cJSON_IsString(id)) strncpy(env->id, id->valuestring, sizeof env->id - 1);
    if (cJSON_IsString(sender_id)) strncpy(env->sender_id, sender_id->valuestring, sizeof env->sender_id - 1);
}

bool tezos_beacon_parse_permission_request(const char *json, tezos_beacon_permission_request_t *out) {
    memset(out, 0, sizeof *out);
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    parse_envelope(root, &out->envelope);
    bool ok = strcmp(out->envelope.type, "permission_request") == 0;

    cJSON *app_metadata = cJSON_GetObjectItemCaseSensitive(root, "appMetadata");
    if (cJSON_IsObject(app_metadata)) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(app_metadata, "name");
        cJSON *sid = cJSON_GetObjectItemCaseSensitive(app_metadata, "senderId");
        if (cJSON_IsString(name)) strncpy(out->app_name, name->valuestring, sizeof out->app_name - 1);
        if (cJSON_IsString(sid)) strncpy(out->app_sender_id, sid->valuestring, sizeof out->app_sender_id - 1);
    }
    cJSON *network = cJSON_GetObjectItemCaseSensitive(root, "network");
    if (cJSON_IsObject(network)) {
        cJSON *ntype = cJSON_GetObjectItemCaseSensitive(network, "type");
        if (cJSON_IsString(ntype)) strncpy(out->network_type, ntype->valuestring, sizeof out->network_type - 1);
    }

    cJSON_Delete(root);
    return ok;
}

bool tezos_beacon_parse_operation_request(const char *json, tezos_beacon_operation_request_t *out) {
    memset(out, 0, sizeof *out);
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    parse_envelope(root, &out->envelope);
    bool ok = strcmp(out->envelope.type, "operation_request") == 0;

    cJSON *network = cJSON_GetObjectItemCaseSensitive(root, "network");
    if (cJSON_IsObject(network)) {
        cJSON *ntype = cJSON_GetObjectItemCaseSensitive(network, "type");
        if (cJSON_IsString(ntype)) strncpy(out->network_type, ntype->valuestring, sizeof out->network_type - 1);
    }
    cJSON *src = cJSON_GetObjectItemCaseSensitive(root, "sourceAddress");
    if (cJSON_IsString(src)) strncpy(out->source_address, src->valuestring, sizeof out->source_address - 1);

    cJSON *details = cJSON_GetObjectItemCaseSensitive(root, "operationDetails");
    if (cJSON_IsArray(details)) {
        int i = 0;
        cJSON *op;
        cJSON_ArrayForEach(op, details) {
            if (i >= TEZOS_BEACON_MAX_OPS) break;
            tezos_beacon_op_t *dst = &out->operations[i];
            cJSON *kind = cJSON_GetObjectItemCaseSensitive(op, "kind");
            cJSON *dest = cJSON_GetObjectItemCaseSensitive(op, "destination");
            cJSON *amount = cJSON_GetObjectItemCaseSensitive(op, "amount");
            if (cJSON_IsString(kind)) strncpy(dst->kind, kind->valuestring, sizeof dst->kind - 1);
            if (cJSON_IsString(dest)) strncpy(dst->destination, dest->valuestring, sizeof dst->destination - 1);
            if (cJSON_IsString(amount)) strncpy(dst->amount, amount->valuestring, sizeof dst->amount - 1);

            cJSON *params = cJSON_GetObjectItemCaseSensitive(op, "parameters");
            if (cJSON_IsObject(params)) {
                cJSON *entrypoint = cJSON_GetObjectItemCaseSensitive(params, "entrypoint");
                cJSON *value = cJSON_GetObjectItemCaseSensitive(params, "value");
                if (cJSON_IsString(entrypoint)) {
                    dst->has_entrypoint = true;
                    strncpy(dst->entrypoint, entrypoint->valuestring, sizeof dst->entrypoint - 1);
                }
                if (cJSON_IsObject(value)) {
                    cJSON *int_val = cJSON_GetObjectItemCaseSensitive(value, "int");
                    if (cJSON_IsString(int_val)) strncpy(dst->parameter_int, int_val->valuestring, sizeof dst->parameter_int - 1);
                }
            }
            i++;
        }
        out->operation_count = i;
    }

    cJSON_Delete(root);
    return ok;
}

bool tezos_beacon_build_permission_response(const tezos_beacon_permission_request_t *request,
                                            const char *wallet_name, const char *public_key,
                                            const char *address, char **json_out) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "permission_response");
    cJSON_AddStringToObject(root, "version", request->envelope.version[0] ? request->envelope.version : "3");
    cJSON_AddStringToObject(root, "id", request->envelope.id);
    cJSON_AddStringToObject(root, "senderId", request->app_sender_id); /* echoes the requester's id, per BeaconBaseMessage's "same ID in request and response" pairing convention */
    cJSON_AddStringToObject(root, "walletType", "implicit");

    cJSON *app_metadata = cJSON_CreateObject();
    cJSON_AddStringToObject(app_metadata, "senderId", request->app_sender_id);
    cJSON_AddStringToObject(app_metadata, "name", wallet_name);
    cJSON_AddItemToObject(root, "appMetadata", app_metadata);

    cJSON_AddStringToObject(root, "publicKey", public_key);
    (void)address; /* PermissionResponse's optional "address" field is only used for
        abstracted_account wallets, per the real schema -- not our case (implicit) */

    cJSON *network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "type", request->network_type);
    cJSON_AddItemToObject(root, "network", network);

    cJSON *scopes = cJSON_CreateArray();
    cJSON_AddItemToArray(scopes, cJSON_CreateString("operation_request"));
    cJSON_AddItemToArray(scopes, cJSON_CreateString("sign"));
    cJSON_AddItemToObject(root, "scopes", scopes);

    *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return *json_out != NULL;
}

bool tezos_beacon_build_operation_response(const tezos_beacon_operation_request_t *request,
                                           const char *transaction_hash, char **json_out) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "operation_response");
    cJSON_AddStringToObject(root, "version", request->envelope.version[0] ? request->envelope.version : "3");
    cJSON_AddStringToObject(root, "id", request->envelope.id);
    cJSON_AddStringToObject(root, "senderId", request->envelope.sender_id);
    cJSON_AddStringToObject(root, "transactionHash", transaction_hash);

    *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return *json_out != NULL;
}
