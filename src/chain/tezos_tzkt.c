#include "tezos_tzkt.h"
#include "tezos_http.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char *tezos_tzkt_base_url(tezos_network_t network) {
    switch (network) {
        case TEZOS_NETWORK_MAINNET:   return "https://api.tzkt.io";
        case TEZOS_NETWORK_GHOSTNET:  return "https://api.ghostnet.tzkt.io";
        case TEZOS_NETWORK_SHADOWNET: return "https://api.shadownet.tzkt.io";
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

bool tezos_tzkt_get_contract_entrypoints(tezos_network_t network, const char *address,
                                         char names_out[][32], int max_names, int *count_out) {
    *count_out = 0;
    char url[320];
    snprintf(url, sizeof url, "%s/v1/contracts/%s/entrypoints", tezos_tzkt_base_url(network), address);

    tezos_http_response_t resp;
    if (!tezos_http_get(url, &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; }

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    /* Defensive by necessity -- see the header comment for why this
       function's exact response shape wasn't verified against a live
       capture, unlike get_head_hash/get_account above. Handles the
       response as either a top-level array of entrypoint objects, or
       (if TZKT's actual shape differs) gracefully finds nothing and
       returns true with count 0 rather than crashing -- a genuinely
       different contract from returning false, which this reserves for
       real transport/parse failures. */
    cJSON *array = cJSON_IsArray(root) ? root : NULL;
    if (array) {
        cJSON *entry;
        cJSON_ArrayForEach(entry, array) {
            if (*count_out >= max_names) break;
            if (!cJSON_IsObject(entry)) continue;
            cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, "name");
            if (!cJSON_IsString(name) || !name->valuestring) continue; /* skip, don't fail the whole parse */
            strncpy(names_out[*count_out], name->valuestring, 31);
            names_out[*count_out][31] = '\0';
            (*count_out)++;
        }
    }

    cJSON_Delete(root);
    return true; /* transport+JSON-parse succeeded; count_out may legitimately be 0 */
}

bool tezos_tzkt_get_entrypoints_with_types(tezos_network_t network, const char *address,
                                           tezos_tzkt_entrypoint_t *entrypoints_out,
                                           int max_entrypoints, int *count_out) {
    *count_out = 0;
    char url[320];
    snprintf(url, sizeof url, "%s/v1/contracts/%s/entrypoints?micheline=true",
             tezos_tzkt_base_url(network), address);

    tezos_http_response_t resp;
    if (!tezos_http_get(url, &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; }

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    cJSON *array = cJSON_IsArray(root) ? root : NULL;
    if (array) {
        cJSON *entry;
        cJSON_ArrayForEach(entry, array) {
            if (*count_out >= max_entrypoints) break;
            if (!cJSON_IsObject(entry)) continue;
            cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, "name");
            if (!cJSON_IsString(name) || !name->valuestring) continue; /* skip, don't fail the whole parse */

            tezos_tzkt_entrypoint_t *ep = &entrypoints_out[*count_out];
            strncpy(ep->name, name->valuestring, sizeof ep->name - 1);
            ep->name[sizeof ep->name - 1] = '\0';
            ep->type_root = -1; /* default: type unknown, until proven otherwise below */

            /* HONEST GUESS, see the header's disclosed gap: try
               "michelineParameters" first, then plain "parameters" --
               neither confirmed against a live response this session.
               If neither is present/parseable, type_root correctly
               stays -1 and this entrypoint is still usable by name
               alone (matches tezos_tzkt_get_contract_entrypoints's
               existing name-only behavior). */
            cJSON *micheline_field = cJSON_GetObjectItemCaseSensitive(entry, "michelineParameters");
            if (!micheline_field) micheline_field = cJSON_GetObjectItemCaseSensitive(entry, "parameters");

            if (micheline_field) {
                char *type_json_str = cJSON_PrintUnformatted(micheline_field);
                if (type_json_str) {
                    ep->type_root = tezos_micheline_parse_type(type_json_str, &ep->type_pool);
                    free(type_json_str);
                }
            }
            (*count_out)++;
        }
    }

    cJSON_Delete(root);
    return true; /* transport+JSON-parse succeeded; individual type_roots may still be -1 */
}

bool tezos_tzkt_get_token_balances(tezos_network_t network, const char *account,
                                   const char *token_contract,
                                   char token_ids_out[][32], int max_ids, int *count_out) {
    *count_out = 0;
    char url[400];
    snprintf(url, sizeof url, "%s/v1/tokens/balances?account=%s&token.contract=%s",
             tezos_tzkt_base_url(network), account, token_contract);

    tezos_http_response_t resp;
    if (!tezos_http_get(url, &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; }

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    cJSON *array = cJSON_IsArray(root) ? root : NULL;
    if (array) {
        cJSON *entry;
        cJSON_ArrayForEach(entry, array) {
            if (*count_out >= max_ids) break;
            if (!cJSON_IsObject(entry)) continue;

            /* real, confirmed field names -- see the header's note on
               the live capture this was verified against */
            cJSON *balance = cJSON_GetObjectItemCaseSensitive(entry, "balance");
            if (!cJSON_IsString(balance) || !balance->valuestring) continue;
            if (strcmp(balance->valuestring, "0") == 0) continue; /* skip historical zero-balance entries */

            cJSON *token = cJSON_GetObjectItemCaseSensitive(entry, "token");
            if (!cJSON_IsObject(token)) continue;
            cJSON *token_id = cJSON_GetObjectItemCaseSensitive(token, "tokenId");
            if (!cJSON_IsString(token_id) || !token_id->valuestring) continue;

            strncpy(token_ids_out[*count_out], token_id->valuestring, 31);
            token_ids_out[*count_out][31] = '\0';
            (*count_out)++;
        }
    }

    cJSON_Delete(root);
    return true; /* transport+JSON-parse succeeded; count_out may legitimately be 0 (no messages) */
}

bool tezos_tzkt_get_message(tezos_network_t network, const char *contract_address,
                            const char *message_id, tezos_tzkt_message_t *message_out) {
    char url[400];
    snprintf(url, sizeof url, "%s/v1/contracts/%s/bigmaps/message_infos/keys/%s",
             tezos_tzkt_base_url(network), contract_address, message_id);

    tezos_http_response_t resp;
    if (!tezos_http_get(url, &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; } /* 404 = no such message, fails cleanly */

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    /* real, confirmed shape: a single object (not an array, since this
       is the specific-key variant) with a "value" field holding the
       message record */
    cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
    if (!cJSON_IsObject(value)) { cJSON_Delete(root); return false; }

    memset(message_out, 0, sizeof *message_out);
    cJSON *sender = cJSON_GetObjectItemCaseSensitive(value, "sender");
    cJSON *subject = cJSON_GetObjectItemCaseSensitive(value, "subject");
    cJSON *content = cJSON_GetObjectItemCaseSensitive(value, "content");
    cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(value, "timestamp");

    if (cJSON_IsString(sender) && sender->valuestring) strncpy(message_out->sender, sender->valuestring, sizeof message_out->sender - 1);
    if (cJSON_IsString(subject) && subject->valuestring) strncpy(message_out->subject, subject->valuestring, sizeof message_out->subject - 1);
    if (cJSON_IsString(content) && content->valuestring) strncpy(message_out->content, content->valuestring, sizeof message_out->content - 1);
    if (cJSON_IsString(timestamp) && timestamp->valuestring) strncpy(message_out->timestamp, timestamp->valuestring, sizeof message_out->timestamp - 1);

    cJSON_Delete(root);
    /* real success requires at minimum a sender -- a value object with
       none of the expected fields present suggests a shape mismatch,
       not a real message */
    return message_out->sender[0] != '\0';
}

bool tezos_tzkt_get_owned_tokens(tezos_network_t network, const char *account,
                                 tezos_tzkt_owned_token_t *tokens_out, int max_tokens, int *count_out) {
    *count_out = 0;
    char url[300];
    snprintf(url, sizeof url, "%s/v1/tokens/balances?account=%s",
             tezos_tzkt_base_url(network), account);

    tezos_http_response_t resp;
    if (!tezos_http_get(url, &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; }

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    cJSON *array = cJSON_IsArray(root) ? root : NULL;
    if (array) {
        cJSON *entry;
        cJSON_ArrayForEach(entry, array) {
            if (*count_out >= max_tokens) break;
            if (!cJSON_IsObject(entry)) continue;

            cJSON *balance = cJSON_GetObjectItemCaseSensitive(entry, "balance");
            if (!cJSON_IsString(balance) || !balance->valuestring) continue;
            if (strcmp(balance->valuestring, "0") == 0) continue; /* same exclusion as the Messenger-specific function */

            cJSON *token = cJSON_GetObjectItemCaseSensitive(entry, "token");
            if (!cJSON_IsObject(token)) continue;
            cJSON *token_id = cJSON_GetObjectItemCaseSensitive(token, "tokenId");
            if (!cJSON_IsString(token_id) || !token_id->valuestring) continue;

            tezos_tzkt_owned_token_t *out = &tokens_out[*count_out];
            memset(out, 0, sizeof *out);
            strncpy(out->token_id, token_id->valuestring, sizeof out->token_id - 1);
            strncpy(out->balance, balance->valuestring, sizeof out->balance - 1);

            cJSON *contract = cJSON_GetObjectItemCaseSensitive(token, "contract");
            if (cJSON_IsObject(contract)) {
                cJSON *addr = cJSON_GetObjectItemCaseSensitive(contract, "address");
                if (cJSON_IsString(addr) && addr->valuestring) strncpy(out->contract_address, addr->valuestring, sizeof out->contract_address - 1);
                cJSON *alias = cJSON_GetObjectItemCaseSensitive(contract, "alias");
                if (cJSON_IsString(alias) && alias->valuestring) strncpy(out->contract_alias, alias->valuestring, sizeof out->contract_alias - 1);
            }
            cJSON *metadata = cJSON_GetObjectItemCaseSensitive(token, "metadata");
            if (cJSON_IsObject(metadata)) {
                cJSON *name = cJSON_GetObjectItemCaseSensitive(metadata, "name");
                if (cJSON_IsString(name) && name->valuestring) strncpy(out->name, name->valuestring, sizeof out->name - 1);

                /* real fallback chain: displayUri first (the intended
                   "show this" image per TZIP-16 convention), then
                   thumbnailUri, then artifactUri -- take the first one
                   that's actually present, not assume displayUri always is */
                const char *uri_field_names[] = {"displayUri", "thumbnailUri", "artifactUri"};
                for (size_t f = 0; f < sizeof(uri_field_names)/sizeof(uri_field_names[0]); f++) {
                    cJSON *uri = cJSON_GetObjectItemCaseSensitive(metadata, uri_field_names[f]);
                    if (cJSON_IsString(uri) && uri->valuestring && uri->valuestring[0]) {
                        strncpy(out->image_uri, uri->valuestring, sizeof out->image_uri - 1);
                        break;
                    }
                }
            }
            (*count_out)++;
        }
    }

    cJSON_Delete(root);
    return true;
}

bool tezos_tzkt_get_contract_info(tezos_network_t network, const char *address,
                                  tezos_tzkt_contract_info_t *info_out) {
    char url[300];
    snprintf(url, sizeof url, "%s/v1/contracts/%s", tezos_tzkt_base_url(network), address);

    tezos_http_response_t resp;
    if (!tezos_http_get(url, &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; }

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    memset(info_out, 0, sizeof *info_out);

    cJSON *alias = cJSON_GetObjectItemCaseSensitive(root, "alias");
    if (cJSON_IsString(alias) && alias->valuestring) strncpy(info_out->alias, alias->valuestring, sizeof info_out->alias - 1);

    cJSON *balance = cJSON_GetObjectItemCaseSensitive(root, "balance");
    if (cJSON_IsNumber(balance)) snprintf(info_out->balance, sizeof info_out->balance, "%.0f", balance->valuedouble);

    cJSON *num_tx = cJSON_GetObjectItemCaseSensitive(root, "numTransactions");
    if (cJSON_IsNumber(num_tx)) info_out->num_transactions = (int)num_tx->valuedouble;

    cJSON *first_time = cJSON_GetObjectItemCaseSensitive(root, "firstActivityTime");
    if (cJSON_IsString(first_time) && first_time->valuestring) strncpy(info_out->first_activity_time, first_time->valuestring, sizeof info_out->first_activity_time - 1);

    cJSON *last_time = cJSON_GetObjectItemCaseSensitive(root, "lastActivityTime");
    if (cJSON_IsString(last_time) && last_time->valuestring) strncpy(info_out->last_activity_time, last_time->valuestring, sizeof info_out->last_activity_time - 1);

    cJSON_Delete(root);
    /* real success requires at minimum a balance field having been present
       (every real contract has one, even if "0") -- its absence suggests
       a shape mismatch or a nonexistent address, not a real contract */
    return balance != NULL;
}
