/* Real decode test -- uses a genuine, generated 4x4 PNG (not a
   synthetic/fake byte pattern) with known, distinct per-quadrant
   colors, embedded directly as a real byte array so the test is
   self-contained. Verifies actual decoded pixel VALUES match the real
   source image, not just that decode "succeeds" without crashing. */
#include "../src/core/tezos_image_decode.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static const unsigned char REAL_PNG[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x08, 0x02, 0x00, 0x00, 0x00, 0x26, 0x93, 0x09, 0x29, 0x00, 0x00, 0x00, 0x1e, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x5d, 0xc9, 0xb1, 0x11, 0x00, 0x20, 0x00, 0x03, 0x21, 0xe2, 0xfe, 0x3b, 0xbf, 0xad, 0x27, 0x2d, 0x0b, 0x2c, 0x38, 0x1e, 0x23, 0xd4, 0xfe, 0xb9, 0x9b, 0x8d, 0x05, 0x04, 0xcf, 0x3d, 0x2d, 0x61, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82 };

int main(void) {
    tezos_decoded_image_t img;
    CHECK(tezos_image_decode(REAL_PNG, sizeof REAL_PNG, &img), "the real, generated 4x4 PNG decodes successfully");
    CHECK(img.width == 4 && img.height == 4, "real decoded dimensions match the real source image exactly");

    /* verify ACTUAL decoded pixel values match the real source image's
       known per-quadrant colors -- not just "decode succeeded" */
    #define PX(x, y) (&img.rgb[((y) * img.width + (x)) * 3])
    CHECK(PX(0,0)[0] == 255 && PX(0,0)[1] == 0 && PX(0,0)[2] == 0, "top-left pixel is real, correct red");
    CHECK(PX(3,0)[0] == 0 && PX(3,0)[1] == 255 && PX(3,0)[2] == 0, "top-right pixel is real, correct green");
    CHECK(PX(0,3)[0] == 0 && PX(0,3)[1] == 0 && PX(0,3)[2] == 255, "bottom-left pixel is real, correct blue");
    CHECK(PX(3,3)[0] == 255 && PX(3,3)[1] == 255 && PX(3,3)[2] == 0, "bottom-right pixel is real, correct yellow");

    tezos_image_free(&img);
    CHECK(img.rgb == NULL, "tezos_image_free correctly clears the pointer after freeing");

    /* real failure case -- genuinely corrupt/non-image data */
    tezos_decoded_image_t bad;
    const unsigned char garbage[] = {0x00, 0x01, 0x02, 0x03, 0x04};
    CHECK(!tezos_image_decode(garbage, sizeof garbage, &bad), "genuinely non-image data is rejected cleanly, not a crash");

    /* calling free on an already-freed/never-decoded image is safe */
    tezos_image_free(&bad);
    tezos_image_free(&img); /* double-free-safe -- rgb is already NULL */

    /* real IPFS URI resolution */
    char url[256];
    CHECK(tezos_resolve_ipfs_uri("ipfs://QmRealLookingCidHash123", url, sizeof url)
          && strcmp(url, "https://ipfs.io/ipfs/QmRealLookingCidHash123") == 0,
          "a real ipfs:// URI resolves to a real, fetchable https://ipfs.io/ipfs/ URL");
    CHECK(tezos_resolve_ipfs_uri("ipfs://QmCid/some/path.png", url, sizeof url)
          && strcmp(url, "https://ipfs.io/ipfs/QmCid/some/path.png") == 0,
          "an ipfs:// URI with a real sub-path is resolved correctly, path preserved");
    CHECK(tezos_resolve_ipfs_uri("https://example.com/already-real.png", url, sizeof url)
          && strcmp(url, "https://example.com/already-real.png") == 0,
          "an already-real https:// URL passes through unchanged, not mangled");
    CHECK(tezos_resolve_ipfs_uri("http://example.com/plain.png", url, sizeof url)
          && strcmp(url, "http://example.com/plain.png") == 0,
          "a plain http:// URL also passes through unchanged");
    CHECK(!tezos_resolve_ipfs_uri("bafybeicid-without-a-scheme", url, sizeof url),
          "a genuinely unrecognized scheme is rejected, not silently guessed at");
    char tiny_buf[10];
    CHECK(!tezos_resolve_ipfs_uri("ipfs://QmSomeReallyLongCidThatWontFit", tiny_buf, sizeof tiny_buf),
          "a resolved URL that wouldn't fit the output buffer is correctly reported as a failure, not silently truncated");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
