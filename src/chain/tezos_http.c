#include "tezos_http.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

bool tezos_http_init(void) {
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}
void tezos_http_cleanup(void) {
    curl_global_cleanup();
}

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    tezos_http_response_t *resp = (tezos_http_response_t *)userdata;
    size_t chunk_len = size * nmemb;
    char *new_body = realloc(resp->body, resp->body_len + chunk_len + 1);
    if (!new_body) return 0; /* signals error to libcurl */
    resp->body = new_body;
    memcpy(resp->body + resp->body_len, ptr, chunk_len);
    resp->body_len += chunk_len;
    resp->body[resp->body_len] = '\0';
    return chunk_len;
}

bool tezos_http_get(const char *url, tezos_http_response_t *response_out) {
    response_out->body = calloc(1, 1);
    response_out->body_len = 0;
    response_out->status_code = 0;

    CURL *curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_out);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "tezos-cyberdeck/0.1");

    CURLcode res = curl_easy_perform(curl);
    bool ok = (res == CURLE_OK);
    if (ok) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_out->status_code);

    curl_easy_cleanup(curl);
    return ok;
}

static bool http_body_request(const char *url, const char *json_body, const char *method,
                              tezos_http_response_t *response_out) {
    response_out->body = calloc(1, 1);
    response_out->body_len = 0;
    response_out->status_code = 0;

    CURL *curl = curl_easy_init();
    if (!curl) return false;

    struct curl_slist *headers = curl_slist_append(NULL, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body ? json_body : "");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_out);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "tezos-cyberdeck/0.1");

    CURLcode res = curl_easy_perform(curl);
    bool ok = (res == CURLE_OK);
    if (ok) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_out->status_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ok;
}

bool tezos_http_post(const char *url, const char *json_body, tezos_http_response_t *response_out) {
    return http_body_request(url, json_body, "POST", response_out);
}
bool tezos_http_put(const char *url, const char *json_body, tezos_http_response_t *response_out) {
    return http_body_request(url, json_body, "PUT", response_out);
}

void tezos_http_response_free(tezos_http_response_t *response) {
    free(response->body);
    response->body = NULL;
    response->body_len = 0;
}
