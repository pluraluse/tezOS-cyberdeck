#include "camera_pipeline.h"
#include <string.h>
#include <stdlib.h>

void tezos_cam_resolution_dims(tezos_cam_resolution_t res, int *w, int *h) {
    switch (res) {
        case TEZOS_CAM_RES_320x480: *w = 320; *h = 480; break;
        case TEZOS_CAM_RES_320x240: *w = 320; *h = 240; break;
        case TEZOS_CAM_RES_800x480: *w = 800; *h = 480; break;
        case TEZOS_CAM_RES_640x480: *w = 640; *h = 480; break;
    }
}

bool tezos_cam_downsample_box(const tezos_rgb888_t *src, int src_w, int src_h,
                              tezos_rgb888_t *dst, int dst_w, int dst_h) {
    if (!src || !dst || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return false;

    for (int dy = 0; dy < dst_h; dy++) {
        /* Real (not integer-truncated-early) block boundaries, computed
           from the actual src/dst ratio -- this is what guarantees no
           source row/column is silently dropped or double-counted at
           the edges when dst doesn't evenly divide src. */
        int y0 = (dy * src_h) / dst_h;
        int y1 = ((dy + 1) * src_h) / dst_h;
        if (y1 <= y0) y1 = y0 + 1; /* guard against a degenerate empty block */
        if (y1 > src_h) y1 = src_h;

        for (int dx = 0; dx < dst_w; dx++) {
            int x0 = (dx * src_w) / dst_w;
            int x1 = ((dx + 1) * src_w) / dst_w;
            if (x1 <= x0) x1 = x0 + 1;
            if (x1 > src_w) x1 = src_w;

            long sum_r = 0, sum_g = 0, sum_b = 0;
            int count = 0;
            for (int sy = y0; sy < y1; sy++) {
                for (int sx = x0; sx < x1; sx++) {
                    tezos_rgb888_t p = src[sy * src_w + sx];
                    sum_r += p.r; sum_g += p.g; sum_b += p.b;
                    count++;
                }
            }
            tezos_rgb888_t avg = {
                (uint8_t)(sum_r / count), (uint8_t)(sum_g / count), (uint8_t)(sum_b / count)
            };
            dst[dy * dst_w + dx] = avg;
        }
    }
    return true;
}

void tezos_cam_generate_bayer_matrix(int n, int *out) {
    /* Standard recursive tiling construction: B(2k) built from B(k) as
       a 2x2 tiling of [4*B+0, 4*B+2; 4*B+3, 4*B+1]. Generated here, not
       hand-transcribed -- verified against the well-known reference 4x4
       matrix ([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]]) in the
       test suite, so there's no risk of a copy-paste digit error the
       way a 64-number 8x8 matrix would carry if typed by hand. */
    out[0] = 0; /* B(1) = [0] */
    int size = 1;
    while (size < n) {
        int new_size = size * 2;
        int *tmp = malloc((size_t)(new_size * new_size) * sizeof(int));
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int v = out[y * size + x];
                tmp[y * new_size + x]                     = 4 * v + 0; /* top-left */
                tmp[y * new_size + (x + size)]             = 4 * v + 2; /* top-right */
                tmp[(y + size) * new_size + x]             = 4 * v + 3; /* bottom-left */
                tmp[(y + size) * new_size + (x + size)]    = 4 * v + 1; /* bottom-right */
            }
        }
        memcpy(out, tmp, (size_t)(new_size * new_size) * sizeof(int));
        free(tmp);
        size = new_size;
    }
}

static double weighted_luminance(tezos_rgb888_t p) {
    return 0.299 * p.r + 0.587 * p.g + 0.114 * p.b;
}

bool tezos_cam_apply_1bit_dither(const tezos_rgb888_t *src, int w, int h,
                                 tezos_cam_dither_size_t dither_size, uint8_t *dst) {
    if (!src || !dst || w <= 0 || h <= 0) return false;
    int n = (dither_size == TEZOS_CAM_DITHER_4x4) ? 4 : 8;
    int *matrix = malloc((size_t)(n * n) * sizeof(int));
    tezos_cam_generate_bayer_matrix(n, matrix);

    size_t packed_len = (size_t)((w * h + 7) / 8);
    memset(dst, 0, packed_len);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double lum = weighted_luminance(src[y * w + x]);
            double threshold = ((double)matrix[(y % n) * n + (x % n)] + 0.5) / (n * n) * 255.0;
            if (lum >= threshold) {
                int idx = y * w + x;
                dst[idx / 8] |= (uint8_t)(0x80 >> (idx % 8)); /* MSB first, per header contract */
            }
        }
    }
    free(matrix);
    return true;
}

bool tezos_cam_apply_8bit_gray(const tezos_rgb888_t *src, int w, int h, uint8_t *dst) {
    if (!src || !dst || w <= 0 || h <= 0) return false;
    for (int i = 0; i < w * h; i++) {
        double lum = weighted_luminance(src[i]);
        int v = (int)(lum + 0.5); /* round, no dither -- 256 levels is already smooth */
        if (v > 255) v = 255;
        if (v < 0) v = 0;
        dst[i] = (uint8_t)v;
    }
    return true;
}

static int dither_channel(int value, int bits, int bayer_val, int n) {
    int levels = 1 << bits;
    double step = 255.0 / (double)(levels - 1);
    double bayer_normalized = ((double)bayer_val + 0.5) / (double)(n * n); /* in (0,1) */
    double dithered = (double)value + (bayer_normalized - 0.5) * step;
    int level = (int)(dithered / step + 0.5); /* round to nearest quantization level */
    if (level < 0) level = 0;
    if (level > levels - 1) level = levels - 1;
    return level;
}

bool tezos_cam_apply_8bit_color(const tezos_rgb888_t *src, int w, int h,
                                tezos_cam_dither_size_t dither_size, uint8_t *dst) {
    if (!src || !dst || w <= 0 || h <= 0) return false;
    int n = (dither_size == TEZOS_CAM_DITHER_4x4) ? 4 : 8;
    int *matrix = malloc((size_t)(n * n) * sizeof(int));
    tezos_cam_generate_bayer_matrix(n, matrix);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            tezos_rgb888_t p = src[y * w + x];
            int bayer_val = matrix[(y % n) * n + (x % n)];
            int r3 = dither_channel(p.r, 3, bayer_val, n);
            int g3 = dither_channel(p.g, 3, bayer_val, n);
            int b2 = dither_channel(p.b, 2, bayer_val, n);
            dst[y * w + x] = (uint8_t)((r3 << 5) | (g3 << 2) | b2); /* RRRGGGBB */
        }
    }
    free(matrix);
    return true;
}

bool tezos_cam_apply_16bit_color(const tezos_rgb888_t *src, int w, int h, uint16_t *dst) {
    if (!src || !dst || w <= 0 || h <= 0) return false;
    for (int i = 0; i < w * h; i++) {
        tezos_rgb888_t p = src[i];
        /* standard RGB565 packing, matching tezos_gfx.c's rgb565() helper */
        dst[i] = (uint16_t)(((p.r & 0xF8) << 8) | ((p.g & 0xFC) << 3) | (p.b >> 3));
    }
    return true;
}
