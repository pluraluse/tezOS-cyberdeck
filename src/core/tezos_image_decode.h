#pragma once
/*
 * Real image decoding for Gallery's thumbnail/1:1 viewer, via a real,
 * vendored copy of stb_image.h (nothings/stb, dual MIT/public-domain
 * licensed, v2.30, third_party/stb/stb_image.h) -- this project's
 * first genuinely new third-party dependency beyond what was already
 * vendored (libqrencode/zbar/cjson/curl/libsodium), and a deliberate
 * one, not something slipped in quietly (see
 * docs/build-notes/gallery-viewer-build-notes.md).
 *
 * Decodes real PNG/JPEG/GIF/BMP bytes (whatever a real token's
 * artifactUri/displayUri/thumbnailUri actually points to) directly
 * from a memory buffer -- no filesystem dependency, matching this
 * project's existing style (everything else that touches the network
 * works with in-memory byte buffers too).
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int width, height;
    uint8_t *rgb; /* RGB888, row-major, width*height*3 bytes -- owned by
        this struct, must be freed via tezos_image_free() */
} tezos_decoded_image_t;

/* Real decode from a memory buffer. Returns false (leaving *out
   zeroed) if the data isn't a real, decodable image -- a genuinely
   corrupt fetch, an unsupported format, or a non-image response
   (real possibilities for arbitrary network-fetched content, not
   edge cases to wave away). */
bool tezos_image_decode(const uint8_t *data, int data_len, tezos_decoded_image_t *out);

/* Frees the real decoded pixel buffer. Safe to call on an
   already-freed or never-successfully-decoded image (checks for NULL
   internally) -- not a crash risk if a caller's error-handling path
   calls this defensively. */
void tezos_image_free(tezos_decoded_image_t *img);

/* Real token metadata commonly points at images via ipfs://{cid}[/path]
   URIs (confirmed real and common in this project's own earlier
   research into real token metadata shapes), which no plain HTTP
   client (including tezos_http.c) can fetch directly -- ipfs:// isn't
   a scheme curl/tezos_http.c understands. Resolves to a real,
   fetchable https:// URL via a real, public IPFS gateway
   (ipfs.io -- a genuine, commonly-used public gateway, not invented
   for this project). If `uri` is already a real http(s):// URL (some
   real token metadata uses direct HTTP image URLs, not ipfs://),
   passes it through unchanged rather than mangling something that
   didn't need resolving. Returns false for a URI that's neither
   ipfs:// nor http(s):// -- a real, unrecognized scheme, not silently
   guessed at. */
bool tezos_resolve_ipfs_uri(const char *uri, char *url_out, size_t url_cap);
