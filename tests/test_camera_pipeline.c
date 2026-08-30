/* Camera Capture Mode pipeline -- box-average downsample, ordered
   Bayer dithering (matrix generated programmatically, verified against
   the well-known 4x4 reference), and all 4 color-depth modes. Full
   spec: docs/build-notes/camera-app-build-notes.md section 2. */
#include "../src/apps/camera/camera_pipeline.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    int matrix4[16];
    tezos_cam_generate_bayer_matrix(4, matrix4);
    int expected4[16] = {0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5};
    CHECK(memcmp(matrix4, expected4, sizeof expected4) == 0,
          "generated 4x4 Bayer matrix matches the well-known reference exactly");

    int matrix8[64];
    tezos_cam_generate_bayer_matrix(8, matrix8);
    int seen[64] = {0};
    bool all_unique = true;
    for (int i = 0; i < 64; i++) {
        if (matrix8[i] < 0 || matrix8[i] > 63 || seen[matrix8[i]]) all_unique = false;
        else seen[matrix8[i]] = 1;
    }
    CHECK(all_unique, "generated 8x8 Bayer matrix contains each value 0-63 exactly once");
    bool quadrant_correct = true;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (matrix8[y*8+x] != 4*expected4[y*4+x] + 0) quadrant_correct = false;
    CHECK(quadrant_correct, "8x8 matrix's top-left quadrant matches the recursive construction from the 4x4 matrix");

    tezos_rgb888_t checkerboard[16];
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            checkerboard[y*4+x] = ((x+y)%2==0) ? (tezos_rgb888_t){255,255,255} : (tezos_rgb888_t){0,0,0};
    tezos_rgb888_t downsampled[1];
    tezos_cam_downsample_box(checkerboard, 4, 4, downsampled, 1, 1);
    CHECK(downsampled[0].r == 127 || downsampled[0].r == 128,
          "box-average of a black/white checkerboard is genuinely GRAY -- nearest-neighbor never produces this");

    tezos_rgb888_t row[6] = {{0,0,0},{100,0,0},{50,0,0},{150,0,0},{200,0,0},{250,0,0}};
    tezos_rgb888_t row_ds[3];
    tezos_cam_downsample_box(row, 6, 1, row_ds, 3, 1);
    CHECK(row_ds[0].r == 50 && row_ds[1].r == 100 && row_ds[2].r == 225,
          "box-average downsample produces exact arithmetic means for a known case");

    tezos_rgb888_t red = {255,0,0}, green = {0,255,0}, blue = {0,0,255}, white = {255,255,255}, black = {0,0,0};
    uint8_t gray_out[5];
    tezos_cam_apply_8bit_gray(&red, 1, 1, &gray_out[0]);
    tezos_cam_apply_8bit_gray(&green, 1, 1, &gray_out[1]);
    tezos_cam_apply_8bit_gray(&blue, 1, 1, &gray_out[2]);
    tezos_cam_apply_8bit_gray(&white, 1, 1, &gray_out[3]);
    tezos_cam_apply_8bit_gray(&black, 1, 1, &gray_out[4]);
    CHECK(gray_out[0] == 76 && gray_out[1] == 150 && gray_out[2] == 29 && gray_out[3] == 255 && gray_out[4] == 0,
          "weighted grayscale luminance matches standard formula for pure R/G/B/white/black");

    tezos_rgb888_t midgray[64];
    for (int i = 0; i < 64; i++) midgray[i] = (tezos_rgb888_t){128,128,128};
    uint8_t dithered[8];
    tezos_cam_apply_1bit_dither(midgray, 8, 8, TEZOS_CAM_DITHER_4x4, dithered);
    int ones = 0;
    for (int i = 0; i < 64; i++) if (dithered[i/8] & (0x80 >> (i%8))) ones++;
    CHECK(ones > 10 && ones < 54, "uniform mid-gray produces a genuine dither pattern, not flat black/white");

    uint8_t color_out[1];
    tezos_cam_apply_8bit_color(&white, 1, 1, TEZOS_CAM_DITHER_4x4, color_out);
    CHECK(color_out[0] == 0xFF, "pure white in 3-3-2 packing is 0xFF");
    tezos_cam_apply_8bit_color(&black, 1, 1, TEZOS_CAM_DITHER_4x4, color_out);
    CHECK(color_out[0] == 0x00, "pure black in 3-3-2 packing is 0x00");

    uint16_t rgb565_out[1];
    tezos_cam_apply_16bit_color(&white, 1, 1, rgb565_out);
    CHECK(rgb565_out[0] == 0xFFFF, "pure white -> RGB565 0xFFFF");
    tezos_rgb888_t pure_red = {255, 0, 0};
    tezos_cam_apply_16bit_color(&pure_red, 1, 1, rgb565_out);
    CHECK(rgb565_out[0] == 0xF800, "pure red -> RGB565 0xF800");

    int w, h;
    tezos_cam_resolution_dims(TEZOS_CAM_RES_800x480, &w, &h);
    CHECK(w == 800 && h == 480, "resolution preset dims correct");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
