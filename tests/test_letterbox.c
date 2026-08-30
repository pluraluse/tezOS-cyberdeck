/* Logical canvas letterboxing -- what makes testing via Pi HDMI to a
   1080p (or any non-matching) monitor show a correctly-proportioned UI
   instead of a stretched one. Critically verifies the same-resolution
   case (the real target panel) is pixel-identical to its source, since
   this new layer must not affect actual target hardware at all. */
#include "../src/core/tezos_gfx.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    tezos_fb_t *logical_320 = tezos_fb_create(320, 480);
    tezos_fb_t *physical_320 = tezos_fb_create(320, 480);
    for (int i = 0; i < 320 * 480; i++) logical_320->pixels[i] = (uint16_t)(i * 37 + 5);
    tezos_fb_blit_letterboxed(physical_320, logical_320, (tezos_color_t){0,0,0});
    CHECK(memcmp(logical_320->pixels, physical_320->pixels, sizeof(uint16_t) * 320 * 480) == 0,
          "1:1 same-resolution blit (real panel case) is pixel-identical to source -- this layer doesn't affect real hardware");
    tezos_fb_destroy(logical_320); tezos_fb_destroy(physical_320);

    tezos_fb_t *logical = tezos_fb_create(320, 480);
    tezos_fb_t *physical = tezos_fb_create(1920, 1080);
    uint16_t amber565 = (uint16_t)(((0xFF&0xF8)<<8)|((0xA5&0xFC)<<3)|(0x3D>>3));
    for (int i = 0; i < 320 * 480; i++) logical->pixels[i] = amber565;
    tezos_color_t bar = {0x10, 0x14, 0x3A};
    tezos_fb_blit_letterboxed(physical, logical, bar);
    uint16_t bar565 = (uint16_t)(((0x10&0xF8)<<8)|((0x14&0xFC)<<3)|(0x3A>>3));

    CHECK(physical->pixels[540 * 1920 + 960] == amber565, "center of 1920x1080 shows the logical content");
    CHECK(physical->pixels[0] == bar565, "top-left corner is the letterbox bar, not stretched content");
    CHECK(physical->pixels[1079 * 1920 + 1919] == bar565, "bottom-right corner is the letterbox bar");
    CHECK(physical->pixels[60 * 1920 + 640] == amber565, "exact computed top-left of the scaled region shows content");
    CHECK(physical->pixels[59 * 1920 + 640] == bar565,
          "one pixel above the scaled region is still bar color -- exact boundary, catches a real integer-division bug found here");
    tezos_fb_destroy(logical); tezos_fb_destroy(physical);

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
