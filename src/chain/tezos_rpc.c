#include "tezos_rpc.h"
#include "tezos_http.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

bool tezos_rpc_inject_operation(const char *node_base_url,
                                const uint8_t *signed_bytes, size_t signed_len,
                                char *op_hash_out, size_t op_hash_cap) {
    op_hash_out[0] = '\0';

    /* hex-encode the signed operation bytes */
    char *hex = malloc(signed_len * 2 + 1);
    if (!hex) return false;
    for (size_t i = 0; i < signed_len; i++) sprintf(hex + i * 2, "%02x", signed_bytes[i]);

    /* body is a bare JSON STRING containing the hex, not an object --
       confirmed by a real working curl example (see header). Building
       it through cJSON rather than manual string concatenation so any
       future non-hex content is correctly escaped, even though hex
       itself never needs escaping. */
    cJSON *body_value = cJSON_CreateString(hex);
    char *body_json = cJSON_PrintUnformatted(body_value);
    cJSON_Delete(body_value);
    free(hex);
    if (!body_json) return false;

    char url[512];
    snprintf(url, sizeof url, "%s/injection/operation", node_base_url);

    tezos_http_response_t resp;
    bool sent = tezos_http_post(url, body_json, &resp);
    free(body_json);
    if (!sent) return false;

    if (resp.status_code != 200) {
        tezos_http_response_free(&resp);
        return false; /* node rejected the operation, or a non-2xx response */
    }

    /* success response is also a bare JSON string: the operation hash */
    cJSON *hash_value = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!hash_value || !cJSON_IsString(hash_value)) {
        if (hash_value) cJSON_Delete(hash_value);
        return false;
    }

    strncpy(op_hash_out, hash_value->valuestring, op_hash_cap - 1);
    op_hash_out[op_hash_cap - 1] = '\0';
    cJSON_Delete(hash_value);
    return true;
}
