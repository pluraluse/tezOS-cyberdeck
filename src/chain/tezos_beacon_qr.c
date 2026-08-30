#include "tezos_beacon_qr.h"
#include "tezos_base58.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

bool tezos_beacon_build_pairing_uri(const tezos_beacon_peer_info_t *info, char *uri_out, size_t uri_cap) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", info->id);
    cJSON_AddStringToObject(root, "name", info->name);
    cJSON_AddStringToObject(root, "publicKey", info->public_key);
    cJSON_AddStringToObject(root, "version", info->version);
    cJSON_AddStringToObject(root, "relayServer", info->relay_server);
    if (info->icon[0]) cJSON_AddStringToObject(root, "icon", info->icon);
    if (info->app_url[0]) cJSON_AddStringToObject(root, "appUrl", info->app_url);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) return false;

    char encoded[1024];
    bool ok = tezos_b58check_encode_generic((const uint8_t *)json_str, strlen(json_str), encoded, sizeof encoded);
    free(json_str);
    if (!ok) return false;

    int n = snprintf(uri_out, uri_cap, "tezos://?type=tzip10&data=%s", encoded);
    return n > 0 && (size_t)n < uri_cap;
}

bool tezos_beacon_parse_pairing_uri(const char *uri, tezos_beacon_peer_info_t *out) {
    memset(out, 0, sizeof *out);

    /* accept either the full "tezos://?type=tzip10&data=XXX" URI, or a
       bare bs58check data string on its own -- be tolerant of both
       since real-world QR content framing can vary slightly */
    const char *data_marker = strstr(uri, "data=");
    const char *encoded = data_marker ? data_marker + 5 : uri;

    uint8_t decoded[2048];
    size_t decoded_len;
    if (!tezos_b58check_decode_generic(encoded, decoded, &decoded_len, sizeof decoded)) return false;
    if (decoded_len >= sizeof decoded) return false;

    char json_str[2048];
    memcpy(json_str, decoded, decoded_len);
    json_str[decoded_len] = '\0';

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return false;

    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *pk = cJSON_GetObjectItemCaseSensitive(root, "publicKey");
    cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    cJSON *relay = cJSON_GetObjectItemCaseSensitive(root, "relayServer");
    cJSON *icon = cJSON_GetObjectItemCaseSensitive(root, "icon");
    cJSON *app_url = cJSON_GetObjectItemCaseSensitive(root, "appUrl");

    bool ok = cJSON_IsString(id) && cJSON_IsString(name) && cJSON_IsString(pk)
            && cJSON_IsString(version) && cJSON_IsString(relay);
    if (ok) {
        strncpy(out->id, id->valuestring, sizeof out->id - 1);
        strncpy(out->name, name->valuestring, sizeof out->name - 1);
        strncpy(out->public_key, pk->valuestring, sizeof out->public_key - 1);
        strncpy(out->version, version->valuestring, sizeof out->version - 1);
        strncpy(out->relay_server, relay->valuestring, sizeof out->relay_server - 1);
        if (cJSON_IsString(icon)) strncpy(out->icon, icon->valuestring, sizeof out->icon - 1);
        if (cJSON_IsString(app_url)) strncpy(out->app_url, app_url->valuestring, sizeof out->app_url - 1);
    }

    cJSON_Delete(root);
    return ok;
}
