#include "tezos_tzkt.h"
#include "tezos_http.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>

const char *tezos_tzkt_base_url(tezos_network_t network) {
    switch (network) {
        case TEZOS_NETWORK_MAINNET:  return "https://api.tzkt.io";
        case TEZOS_NETWORK_GHOSTNET: return "https://api.ghostnet.tzkt.io";
    }
    return "https://api.tzkt.io";
}

bool tezos_tzkt_get_head_hash(tezos_network_t network, char *hash_out, size_t hash_cap) {
    char url[256];
    snprintf(url, sizeof url, "%s/v1/head", tezos_tzkt_base_url(network));

    tezos_http_response_t resp;
    if (!tezos_http_get(url, &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; }

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    cJSON *hash = cJSON_GetObjectItemCaseSensitive(root, "hash");
    bool ok = cJSON_IsString(hash) && hash->valuestring != NULL;
    if (ok) {
        strncpy(hash_out, hash->valuestring, hash_cap - 1);
        hash_out[hash_cap - 1] = '\0';
    }
    cJSON_Delete(root);
    return ok;
}

bool tezos_tzkt_get_account(tezos_network_t network, const char *address,
                            char *balance_out, size_t balance_cap,
                            char *counter_out, size_t counter_cap) {
    char url[320];
    snprintf(url, sizeof url, "%s/v1/accounts/%s", tezos_tzkt_base_url(network), address);

    tezos_http_response_t resp;
    if (!tezos_http_get(url, &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; }

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    cJSON *balance = cJSON_GetObjectItemCaseSensitive(root, "balance");
    if (!cJSON_IsNumber(balance)) { cJSON_Delete(root); return false; }
    snprintf(balance_out, balance_cap, "%.0f", balance->valuedouble);
    /* balance can exceed what a plain 32-bit int holds (real supply is
       in the ~10^15 mutez range) -- cJSON stores numbers as double,
       which has 53 bits of exact integer precision, comfortably enough
       headroom for real tez balances. Formatting via %.0f avoids
       scientific notation for these magnitudes. */

    cJSON *counter = cJSON_GetObjectItemCaseSensitive(root, "counter");
    if (cJSON_IsNumber(counter)) {
        snprintf(counter_out, counter_cap, "%.0f", counter->valuedouble);
    } else {
        /* TZKT omits "counter" entirely for a never-used account --
           per the header doc, treat that as counter "0" */
        strncpy(counter_out, "0", counter_cap - 1);
        counter_out[counter_cap - 1] = '\0';
    }

    cJSON_Delete(root);
    return true;
}
