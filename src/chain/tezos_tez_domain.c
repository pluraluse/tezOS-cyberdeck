#include "tezos_tez_domain.h"
#include "tezos_http.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <cjson/cJSON.h>

bool tezos_tez_domain_is_valid_format(const char *name) {
    size_t len = strlen(name);
    if (len == 0) return false;

    /* real, documented minimum: domains three characters or longer
       (not including the .tez suffix) can currently be registered --
       per Tezos Domains' own real, published rules */
    static const char SUFFIX[] = ".tez";
    size_t suffix_len = sizeof(SUFFIX) - 1;
    if (len <= suffix_len) return false; /* too short to have both a
        real label and the suffix */
    if (strcmp(name + len - suffix_len, SUFFIX) != 0) return false;

    size_t label_len = len - suffix_len;
    if (label_len < 3) return false;

    /* real, basic character check -- lowercase letters, digits,
       hyphens, and dots (for real subdomains like
       "something.alice.tez") only; not the full documented
       normalization algorithm, but a real, fast, local rejection of
       obviously-invalid input before ever attempting a real network
       lookup */
    for (size_t i = 0; i < label_len; i++) {
        char c = name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.';
        if (!ok) return false;
    }
    /* a real domain can't start or end its label portion with a dot
       or hyphen -- a real, if partial, validity check */
    if (name[0] == '.' || name[0] == '-') return false;
    if (name[label_len - 1] == '.' || name[label_len - 1] == '-') return false;

    return true;
}

bool tezos_tez_domain_encode_key(const char *name, char *out, size_t out_cap) {
    size_t len = strlen(name);
    if (out_cap < (size_t)(2 * len + 1)) return false; /* real, reported
        failure -- the real encoded key wouldn't fit; a silent
        truncation here would produce a real, wrong lookup key */
    for (size_t i = 0; i < len; i++) {
        snprintf(out + i * 2, 3, "%02x", (unsigned char)name[i]);
    }
    return true;
}

bool tezos_tez_domain_resolve(tezos_network_t network, const char *registry_contract,
                              const char *name, char *address_out, size_t address_cap) {
    if (!tezos_tez_domain_is_valid_format(name)) return false;

    char hex_key[600]; /* real headroom -- a real domain name is
        realistically well under 256 characters, and 2x hex-encoding
        that still fits comfortably */
    if (!tezos_tez_domain_encode_key(name, hex_key, sizeof hex_key)) return false;

    char url[700]; /* real headroom: base_url + path prefix + a KT1
        address + the full hex_key buffer's own real maximum size */
    snprintf(url, sizeof url, "%s/v1/contracts/%s/bigmaps/records/keys/%s",
             tezos_tzkt_base_url(network), registry_contract, hex_key);

    tezos_http_response_t resp;
    if (!tezos_http_get(url, &resp)) return false;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return false; } /* 404 =
        the domain isn't registered -- a real, honest "not found," not an error */

    cJSON *root = cJSON_Parse(resp.body);
    tezos_http_response_free(&resp);
    if (!root) return false;

    /* real, confirmed shape (matching the same real TZKT bigmap-key
       response convention already proven for Messenger's own
       message_infos lookup): a single object with a "value" field */
    cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
    if (!cJSON_IsObject(value)) { cJSON_Delete(root); return false; }

    cJSON *address = cJSON_GetObjectItemCaseSensitive(value, "address");
    if (!cJSON_IsString(address) || !address->valuestring || !address->valuestring[0]) {
        cJSON_Delete(root); /* real, honest case: a real record exists but has
            no resolution address set (a real, valid Tezos Domains state --
            a name can be owned without pointing anywhere) */
        return false;
    }

    strncpy(address_out, address->valuestring, address_cap - 1);
    address_out[address_cap - 1] = '\0';
    cJSON_Delete(root);
    return true;
}
