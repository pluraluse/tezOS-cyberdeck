#pragma once
/*
 * Camera Capture Mode — the Game Boy Camera-style art pipeline. Full
 * spec: docs/build-notes/camera-app-build-notes.md section 2. This is
 * the piece that was fully spec'd but genuinely unbuilt, unlike Scan
 * Mode (real QR pairing) which is blocked on real camera hardware —
 * this pipeline is pure image computation, testable end-to-end with a
 * synthetic source frame standing in for a real sensor capture, same
 * pattern used for the mock signer/QR/network pieces elsewhere.
 *
 * Pipeline, per spec, always in this order, no shortcuts:
 *   1. Box-average downsample directly to the target resolution --
 *      NOT nearest-neighbor (aliasing), NOT bilinear/bicubic (blur,
 *      defeats the pixel-art look). Each output pixel is the true
 *      average of its corresponding source block.
 *   2. Color-depth reduction (one of 4 modes below).
 *   3. Result IS what's displayed, saved, and fed to minting -- no
 *      separate "visible" vs "backing" image.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct { uint8_t r, g, b; } tezos_rgb888_t;

typedef enum {
    TEZOS_CAM_RES_320x480, /* exact 1:1 fill, no scroll/letterbox */
    TEZOS_CAM_RES_320x240, /* fits width, vertical letterbox */
    TEZOS_CAM_RES_800x480, /* fits height, horizontal scroll */
    TEZOS_CAM_RES_640x480, /* overflow both axes, scroll both */
} tezos_cam_resolution_t;

typedef enum {
    TEZOS_CAM_DEPTH_1BIT_DITHER,  /* ordered Bayer dither, Game Boy Camera look */
    TEZOS_CAM_DEPTH_8BIT_GRAY,    /* weighted luminance, no dither */
    TEZOS_CAM_DEPTH_8BIT_COLOR,   /* 3-3-2 RGB truncation + Bayer dither */
    TEZOS_CAM_DEPTH_16BIT_COLOR,  /* native RGB565 passthrough */
} tezos_cam_depth_t;

typedef enum {
    TEZOS_CAM_DITHER_4x4, /* coarser, chunkier -- closer to the original GBC look */
    TEZOS_CAM_DITHER_8x8, /* finer gradation, less visible patterning */
} tezos_cam_dither_size_t;

/* Returns the pixel dimensions for a resolution preset. */
void tezos_cam_resolution_dims(tezos_cam_resolution_t res, int *w, int *h);

/* Step 1: box-average downsample. `src` is RGB888, row-major,
   src_w*src_h. `dst` must be pre-allocated for dst_w*dst_h RGB888
   pixels. dst dimensions need not evenly divide src dimensions --
   block boundaries are computed per output pixel from real (not
   truncated-early) source coordinates, so no source row/column is
   silently dropped or double-counted at the edges. */
bool tezos_cam_downsample_box(const tezos_rgb888_t *src, int src_w, int src_h,
                              tezos_rgb888_t *dst, int dst_w, int dst_h);

/* Generates an NxN ordered Bayer dither matrix (N must be 4 or 8),
   values 0..N*N-1, via the standard recursive tiling construction --
   generated programmatically, not hand-transcribed, so there's no
   64-number transcription risk. `out` must hold N*N ints, row-major. */
void tezos_cam_generate_bayer_matrix(int n, int *out);

/* Step 2, mode 1: 1-bit ordered dither. Converts to weighted grayscale
   internally, then dithers against the chosen matrix size. `dst` must
   hold (dst_w*dst_h + 7)/8 bytes -- packed 1 bit per pixel, MSB first
   within each byte, row-major, no row padding. */
bool tezos_cam_apply_1bit_dither(const tezos_rgb888_t *src, int w, int h,
                                 tezos_cam_dither_size_t dither_size, uint8_t *dst);

/* Step 2, mode 2: 8-bit grayscale, weighted luminance
   (0.299R + 0.587G + 0.114B), no dither. `dst` holds w*h bytes. */
bool tezos_cam_apply_8bit_gray(const tezos_rgb888_t *src, int w, int h, uint8_t *dst);

/* Step 2, mode 3: 3-3-2 RGB truncation (3 bits R, 3 bits G, 2 bits B =
   256 colors) with ordered Bayer dither smoothing the quantization
   steps. `dst` holds w*h bytes, each byte packed RRRGGGBB. */
bool tezos_cam_apply_8bit_color(const tezos_rgb888_t *src, int w, int h,
                                tezos_cam_dither_size_t dither_size, uint8_t *dst);

/* Step 2, mode 4: native RGB565 passthrough, no quantization needed --
   the display's own native format. `dst` holds w*h uint16_t values. */
bool tezos_cam_apply_16bit_color(const tezos_rgb888_t *src, int w, int h, uint16_t *dst);
