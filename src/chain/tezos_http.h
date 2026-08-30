#pragma once
/*
 * Thin libcurl wrapper — GET requests returning the response body as a
 * growable string plus the HTTP status code. Nothing TZKT-specific
 * lives here; see tezos_tzkt.h for that.
 *
 * HONEST LIMITATION: this compiles and links against real libcurl, and
 * its request/response mechanics are verified against a real, reachable
 * endpoint (see docs/build-notes/wallet-app-build-notes.md) — but this
 * dev sandbox's network access is domain-restricted and does not
 * include api.tzkt.io specifically, so the TZKT-facing calls in
 * tezos_tzkt.c are verified by parsing real, genuine API responses
 * captured via a different tool (not this HTTP client) as fixtures, not
 * by an actual live round-trip through this client to the real TZKT
 * endpoint. That live round-trip needs to happen on real hardware (or
 * any environment with unrestricted network), not from here.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char *body;      /* heap-allocated, caller must tezos_http_response_free() */
    size_t body_len;
    long status_code;
} tezos_http_response_t;

/* One-time global init/cleanup for libcurl -- call tezos_http_init()
   once at program start, tezos_http_cleanup() once at exit. Not
   per-request. */
bool tezos_http_init(void);
void tezos_http_cleanup(void);

/* Blocking GET. Returns false only on a transport-level failure
   (couldn't connect, DNS failure, timeout) -- a non-2xx HTTP status is
   NOT a false return, it's a normal response with that status code;
   callers check response->status_code themselves. */
bool tezos_http_get(const char *url, tezos_http_response_t *response_out);

/* Blocking POST/PUT with a JSON body (Content-Type: application/json).
   Same false-only-on-transport-failure convention as tezos_http_get.
   Added for the Matrix client (tezos_matrix.h) -- TZKT only ever needed
   GET. */
bool tezos_http_post(const char *url, const char *json_body, tezos_http_response_t *response_out);
bool tezos_http_put(const char *url, const char *json_body, tezos_http_response_t *response_out);

void tezos_http_response_free(tezos_http_response_t *response);
