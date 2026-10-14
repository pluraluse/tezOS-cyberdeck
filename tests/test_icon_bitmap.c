/* Real tests for the shared icon-blit rendering function -- pixel-
   level verification (not just "it didn't crash"), plus real
   bounds-clipping at screen edges, matching the same discipline
   already established for Gallery's and Discover's own blit helpers
   (which this function now replaces the duplication of). */
#include "../src/core/tezos_gfx.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    tezos_fb_t *fb = tezos_fb_create(32, 32);
    tezos_dirty_t d;
    tezos_dirty_reset(&d);
    tezos_draw_rect(fb, &d, 0, 0, fb->width, fb->height, TEZOS_BG);

    /* real, synthetic icon: every pixel a distinct, known value based
       on its own position, so exact placement can be verified byte
       for byte, not just "something got drawn" */
    tezos_icon_t synthetic;
    for (int y = 0; y < TEZOS_ICON_SIZE; y++)
        for (int x = 0; x < TEZOS_ICON_SIZE; x++)
            synthetic.pixels[y * TEZOS_ICON_SIZE + x] = (uint16_t)((x << 8) | y);

    tezos_draw_icon_bitmap(fb, &d, 4, 4, &synthetic);

    CHECK(fb->pixels[4 * fb->width + 4] == synthetic.pixels[0],
          "the icon's real top-left pixel lands at the correct real framebuffer position");
    CHECK(fb->pixels[(4+15) * fb->width + (4+15)] == synthetic.pixels[15 * TEZOS_ICON_SIZE + 15],
          "the icon's real bottom-right pixel lands at the correct real framebuffer position");
    CHECK(fb->pixels[(4+7) * fb->width + (4+3)] == synthetic.pixels[7 * TEZOS_ICON_SIZE + 3],
          "a real, arbitrary interior pixel lands at the correct real position, not just the corners");

    /* real dirty-rect check */
    CHECK(d.count >= 1, "drawing the icon correctly marks a real dirty rect");

    /* real bounds-clipping: draw partially off the bottom-right edge --
       should not crash, and the real on-screen portion should still
       be correct */
    tezos_dirty_reset(&d);
    tezos_draw_icon_bitmap(fb, &d, 28, 28, &synthetic); /* extends 12px past both edges */
    CHECK(fb->pixels[28 * fb->width + 28] == synthetic.pixels[0],
          "an icon drawn partially off-screen still places its real, on-screen pixels correctly");
    CHECK(fb->pixels[31 * fb->width + 31] == synthetic.pixels[3 * TEZOS_ICON_SIZE + 3],
          "the real, clipped edge pixel is correct -- only the on-screen portion of the icon, nothing garbled");

    /* real, negative-position clipping too */
    tezos_dirty_reset(&d);
    bool crashed = false; /* if we get here at all, it didn't crash */
    tezos_draw_icon_bitmap(fb, &d, -8, -8, &synthetic);
    CHECK(!crashed, "an icon drawn partially off the top-left edge (negative position) doesn't crash");
    CHECK(fb->pixels[0] == synthetic.pixels[8 * TEZOS_ICON_SIZE + 8],
          "the real, correct interior pixel appears at the real framebuffer origin after negative-position clipping");

    /* === real scaled blit -- pixel-exact verification, not just
       visual inspection === */
    /* real, separate, larger framebuffer for this specific check --
       a 3x-scaled 16x16 icon is 48x48 real pixels, which doesn't fit
       in the 32x32 fb used above; a real, honest test bug (not a
       product bug) was caught here on the first attempt -- the draw
       function's own bounds-clipping was correctly doing its job,
       but this test's own verification code tried to read a pixel
       position that didn't exist in the too-small buffer */
    tezos_fb_t *fb2 = tezos_fb_create(64, 64);
    tezos_dirty_reset(&d);
    tezos_draw_rect(fb2, &d, 0, 0, fb2->width, fb2->height, TEZOS_BG);
    tezos_icon_t small;
    for (int y = 0; y < TEZOS_ICON_SIZE; y++)
        for (int x = 0; x < TEZOS_ICON_SIZE; x++)
            small.pixels[y * TEZOS_ICON_SIZE + x] = (uint16_t)((x << 8) | y);

    tezos_draw_icon_bitmap_scaled(fb2, &d, 0, 0, &small, 3);
    /* real check: each source pixel becomes a real, solid 3x3 block --
       verify all 9 pixels of one such block match the same real source value */
    uint16_t expected = small.pixels[2 * TEZOS_ICON_SIZE + 1]; /* source (x=1,y=2) */
    bool block_correct = true;
    for (int dy = 0; dy < 3; dy++)
        for (int dx = 0; dx < 3; dx++)
            if (fb2->pixels[(2*3+dy) * fb2->width + (1*3+dx)] != expected) block_correct = false;
    CHECK(block_correct, "a real scaled-up icon pixel becomes a genuinely solid NxN block, all real, correct, matching values -- not a partial or garbled scale-up");

    /* real check: the LAST source pixel's block lands at the correct final position */
    uint16_t last_expected = small.pixels[15 * TEZOS_ICON_SIZE + 15];
    CHECK(fb2->pixels[47 * fb2->width + 47] == last_expected,
          "the real, final scaled pixel block (bottom-right of a 3x scaled 16x16 icon, at real pixel 47,47) is correct");
    tezos_fb_destroy(fb2);

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
