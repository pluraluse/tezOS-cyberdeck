#include "camera.h"
#include "camera_pipeline.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../chain/tezos_beacon_qr.h"
#include <qrencode.h>
#include <zbar.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================
   Home: Scan vs Capture mode select. Real separate code paths from
   here down, per docs/build-notes/camera-app-build-notes.md's
   "structurally separate modes" requirement -- a Capture-mode bug can
   never reach or affect Scan mode's code path.
   ============================================================ */

static tezos_screen_t *scan_screen_get(void);
static tezos_screen_t *capture_settings_screen_get(void);

typedef struct { tezos_list_state_t list; } camera_home_state_t;
static camera_home_state_t g_home_state;
static tezos_list_item_t g_home_items[2] = { {"Scan (Beacon QR)", NULL}, {"Capture (art)", NULL} };
static tezos_screen_t g_home_screen;

static void home_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    camera_home_state_t *st = (camera_home_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "CAMERA", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->list);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t home_input(tezos_screen_t *self, tezos_input_event_t ev,
                                 tezos_screen_t **next, bool *needs_redraw) {
    camera_home_state_t *st = (camera_home_state_t *)self->app_state;
    if (tezos_list_handle_input(&st->list, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        *next = (st->list.selected == 0) ? scan_screen_get() : capture_settings_screen_get();
        return TEZOS_ACTION_PUSH;
    }
    return TEZOS_ACTION_NONE;
}

tezos_screen_t *camera_entry_screen(void) {
    tezos_list_init(&g_home_state.list, g_home_items, 2);
    g_home_screen = (tezos_screen_t){
        .render = home_render, .handle_input = home_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_home_state, .debug_name = "camera_home",
    };
    return &g_home_screen;
}

/* ============================================================
   Scan Mode -- unchanged from before, just renamed from the old
   camera_entry_screen() to scan_screen_get() now that Home exists.
   ============================================================ */

typedef enum { SCAN_IDLE, SCAN_SUCCESS, SCAN_FAILED } scan_state_t;
typedef struct {
    scan_state_t state;
    tezos_beacon_peer_info_t peer;
    char error_msg[64];
} scan_screen_state_t;
static scan_screen_state_t g_scan_state;
static tezos_screen_t g_scan_screen;

static unsigned char *render_qr_to_grayscale(QRcode *qr, int scale, int *out_w, int *out_h) {
    int quiet = 4 * scale;
    int img_size = qr->width * scale + quiet * 2;
    unsigned char *img = malloc((size_t)img_size * img_size);
    memset(img, 255, (size_t)img_size * img_size);
    for (int y = 0; y < qr->width; y++) {
        for (int x = 0; x < qr->width; x++) {
            unsigned char module = qr->data[y * qr->width + x];
            unsigned char pixel = (module & 0x1) ? 0 : 255;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    img[(quiet + y * scale + dy) * img_size + (quiet + x * scale + dx)] = pixel;
        }
    }
    *out_w = *out_h = img_size;
    return img;
}

static bool simulate_scan(tezos_beacon_peer_info_t *out, char *err_out, size_t err_cap) {
    tezos_beacon_peer_info_t sample = {0};
    strcpy(sample.id, "kukai-pairing-session");
    strcpy(sample.name, "Kukai");
    strcpy(sample.public_key, "7775e38359f7e8c66332edf78a0d1e7630191e909f695393bfab40e3f723dedf");
    strcpy(sample.version, "3");
    strcpy(sample.relay_server, "matrix.papers.tech");

    char uri[1024];
    if (!tezos_beacon_build_pairing_uri(&sample, uri, sizeof uri)) {
        strncpy(err_out, "failed to build pairing URI", err_cap - 1);
        return false;
    }

    QRcode *qr = QRcode_encodeString(uri, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qr) { strncpy(err_out, "QR encode failed", err_cap - 1); return false; }

    int img_w, img_h;
    unsigned char *img = render_qr_to_grayscale(qr, 4, &img_w, &img_h);

    zbar_image_scanner_t *scanner = zbar_image_scanner_create();
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);
    zbar_image_t *zimg = zbar_image_create();
    zbar_image_set_format(zimg, zbar_fourcc('Y', '8', '0', '0'));
    zbar_image_set_size(zimg, (unsigned)img_w, (unsigned)img_h);
    zbar_image_set_data(zimg, img, (unsigned long)(img_w * img_h), NULL);

    bool ok = false;
    if (zbar_scan_image(scanner, zimg) > 0) {
        const zbar_symbol_t *sym = zbar_image_first_symbol(zimg);
        if (sym) {
            const char *decoded_text = zbar_symbol_get_data(sym);
            ok = tezos_beacon_parse_pairing_uri(decoded_text, out);
            if (!ok) strncpy(err_out, "decoded QR but failed to parse pairing data", err_cap - 1);
        } else {
            strncpy(err_out, "zbar found no symbol", err_cap - 1);
        }
    } else {
        strncpy(err_out, "zbar scan found nothing", err_cap - 1);
    }

    zbar_image_destroy(zimg);
    zbar_image_scanner_destroy(scanner);
    QRcode_free(qr);
    free(img);
    return ok;
}

static void scan_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    scan_screen_state_t *st = (scan_screen_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SCAN", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    tezos_draw_rect(fb, d, 8, 40, fb->width - 16, 160, (tezos_color_t){0x06, 0x08, 0x18});
    tezos_draw_rect_outline(fb, d, 8, 40, fb->width - 16, 160, TEZOS_MUTED);
    const char *placeholder = "no real camera sensor here";
    int pw = tezos_text_width(placeholder, &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, (fb->width - pw) / 2, 115, placeholder, &TEZOS_FONT_SMALL, TEZOS_MUTED);

    int y = 216;
    switch (st->state) {
        case SCAN_IDLE:
            tezos_draw_text(fb, d, 8, y, "Press SELECT to simulate", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, y + 18, "scanning a Beacon pairing QR", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            break;
        case SCAN_SUCCESS:
            tezos_draw_text(fb, d, 8, y, "PAIRED", &TEZOS_FONT_BODY, TEZOS_CYAN);
            y += 26;
            tezos_draw_text(fb, d, 8, y, st->peer.name, &TEZOS_FONT_SMALL, TEZOS_TEXT);
            y += 18;
            tezos_draw_text(fb, d, 8, y, st->peer.relay_server, &TEZOS_FONT_SMALL, TEZOS_MUTED);
            y += 18;
            {
                char pkpreview[24];
                snprintf(pkpreview, sizeof pkpreview, "%.16s...", st->peer.public_key);
                tezos_draw_text(fb, d, 8, y, pkpreview, &TEZOS_FONT_SMALL, TEZOS_MUTED);
            }
            break;
        case SCAN_FAILED:
            tezos_draw_text(fb, d, 8, y, "SCAN FAILED", &TEZOS_FONT_BODY, TEZOS_PINK);
            y += 26;
            tezos_draw_text(fb, d, 8, y, st->error_msg, &TEZOS_FONT_SMALL, TEZOS_MUTED);
            break;
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t scan_input(tezos_screen_t *self, tezos_input_event_t ev,
                                 tezos_screen_t **next, bool *needs_redraw) {
    scan_screen_state_t *st = (scan_screen_state_t *)self->app_state;
    (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        memset(&st->peer, 0, sizeof st->peer);
        st->error_msg[0] = '\0';
        bool ok = simulate_scan(&st->peer, st->error_msg, sizeof st->error_msg);
        st->state = ok ? SCAN_SUCCESS : SCAN_FAILED;
        *needs_redraw = true;
    }
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *scan_screen_get(void) {
    memset(&g_scan_state, 0, sizeof g_scan_state);
    g_scan_state.state = SCAN_IDLE;
    g_scan_screen = (tezos_screen_t){
        .render = scan_render, .handle_input = scan_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_scan_state, .debug_name = "camera_scan",
    };
    return &g_scan_screen;
}

/* ============================================================
   Capture Mode -- new. Genuinely separate code path from Scan mode,
   never touches zbar/QR decoding. Full pipeline (box-downsample +
   color-depth reduction) is real and verified
   (docs/build-notes/camera-app-build-notes.md section 2,
   camera_pipeline.c). Since there's no real camera sensor, a
   "simulate capture" trigger runs the real pipeline against a
   synthetic gradient+shape test frame standing in for a real photo --
   same pattern as Scan mode's simulate-scan.
   ============================================================ */

static const char *RES_NAMES[4] = { "320x480", "320x240", "800x480", "640x480" };
static const char *DEPTH_NAMES[4] = { "1-bit dither", "8-bit gray", "8-bit color", "16-bit color" };

typedef struct {
    tezos_cam_resolution_t resolution;
    tezos_cam_depth_t depth;
    tezos_cam_dither_size_t dither_size;
    bool have_result;
    tezos_rgb888_t *result_preview; /* always stored as RGB888 for on-screen preview,
        regardless of the mode's actual packed storage format -- see capture below */
    int result_w, result_h;
} capture_state_t;
static capture_state_t g_capture_state;
static tezos_screen_t g_capture_screen;
static tezos_screen_t g_capture_result_screen;

/* Synthetic "photo" standing in for a real sensor capture -- a
   gradient plus a shape, giving the dither/quantize steps something
   realistic to work with instead of flat test colors. Native sensor
   resolution stood in as 960x540 (a plausible CSI sensor downscaled
   preview size), matching the build notes' "~1080p native" note at a
   more test-friendly size. */
static tezos_rgb888_t *make_synthetic_frame(int w, int h) {
    tezos_rgb888_t *img = malloc((size_t)(w * h) * sizeof(tezos_rgb888_t));
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double gx = (double)x / w, gy = (double)y / h;
            uint8_t r = (uint8_t)(gx * 255);
            uint8_t g = (uint8_t)(gy * 255);
            uint8_t b = (uint8_t)(128 + 100 * sin(gx * 10) * cos(gy * 10));
            double dx = gx - 0.5, dy = gy - 0.5;
            if (dx*dx + dy*dy < 0.05) { r = 255; g = 240; b = 200; }
            img[y*w+x] = (tezos_rgb888_t){r, g, b};
        }
    }
    return img;
}

static void capture_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    capture_state_t *st = (capture_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "CAPTURE", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    int y = 44;
    tezos_draw_text(fb, d, 8, y, "Resolution (DOWN to cycle)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    y += 18;
    tezos_draw_text(fb, d, 8, y, RES_NAMES[st->resolution], &TEZOS_FONT_BODY, TEZOS_TEXT);
    y += 34;
    tezos_draw_text(fb, d, 8, y, "Color depth (UP to cycle)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    y += 18;
    tezos_draw_text(fb, d, 8, y, DEPTH_NAMES[st->depth], &TEZOS_FONT_BODY, TEZOS_TEXT);
    y += 34;

    bool dither_applies = (st->depth == TEZOS_CAM_DEPTH_1BIT_DITHER || st->depth == TEZOS_CAM_DEPTH_8BIT_COLOR);
    if (dither_applies) {
        tezos_draw_text(fb, d, 8, y, "Dither matrix (# to toggle)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
        y += 18;
        tezos_draw_text(fb, d, 8, y, st->dither_size == TEZOS_CAM_DITHER_4x4 ? "4x4 (chunkier)" : "8x8 (finer)",
                        &TEZOS_FONT_BODY, TEZOS_TEXT);
    } else {
        tezos_draw_text(fb, d, 8, y, "(dither matrix N/A for this mode)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "CAPTURE", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

/* Runs the REAL pipeline (box-downsample + chosen color-depth
   reduction) against the synthetic source frame, then converts
   whichever packed format the mode produced back to RGB888 purely for
   on-screen preview purposes -- what actually gets saved/minted would
   be the packed format itself (1-bit/8-bit/RGB565), not this
   preview-only RGB888 expansion. */
static void perform_capture(capture_state_t *st) {
    int src_w = 960, src_h = 540;
    tezos_rgb888_t *src = make_synthetic_frame(src_w, src_h);

    int w, h;
    tezos_cam_resolution_dims(st->resolution, &w, &h);
    tezos_rgb888_t *downsampled = malloc((size_t)(w * h) * sizeof(tezos_rgb888_t));
    tezos_cam_downsample_box(src, src_w, src_h, downsampled, w, h);
    free(src);

    free(st->result_preview);
    st->result_preview = malloc((size_t)(w * h) * sizeof(tezos_rgb888_t));
    st->result_w = w; st->result_h = h;

    switch (st->depth) {
        case TEZOS_CAM_DEPTH_1BIT_DITHER: {
            uint8_t *bits = malloc((size_t)((w*h+7)/8));
            tezos_cam_apply_1bit_dither(downsampled, w, h, st->dither_size, bits);
            for (int i = 0; i < w*h; i++) {
                bool on = (bits[i/8] & (0x80 >> (i%8))) != 0;
                st->result_preview[i] = on ? (tezos_rgb888_t){255,255,255} : (tezos_rgb888_t){0,0,0};
            }
            free(bits);
            break;
        }
        case TEZOS_CAM_DEPTH_8BIT_GRAY: {
            uint8_t *gray = malloc((size_t)(w*h));
            tezos_cam_apply_8bit_gray(downsampled, w, h, gray);
            for (int i = 0; i < w*h; i++) st->result_preview[i] = (tezos_rgb888_t){gray[i], gray[i], gray[i]};
            free(gray);
            break;
        }
        case TEZOS_CAM_DEPTH_8BIT_COLOR: {
            uint8_t *packed = malloc((size_t)(w*h));
            tezos_cam_apply_8bit_color(downsampled, w, h, st->dither_size, packed);
            for (int i = 0; i < w*h; i++) {
                uint8_t p = packed[i];
                uint8_t r3 = (p>>5)&0x7, g3 = (p>>2)&0x7, b2 = p&0x3;
                st->result_preview[i] = (tezos_rgb888_t){(uint8_t)(r3*255/7),(uint8_t)(g3*255/7),(uint8_t)(b2*255/3)};
            }
            free(packed);
            break;
        }
        case TEZOS_CAM_DEPTH_16BIT_COLOR: {
            uint16_t *c16 = malloc((size_t)(w*h) * sizeof(uint16_t));
            tezos_cam_apply_16bit_color(downsampled, w, h, c16);
            for (int i = 0; i < w*h; i++) {
                uint16_t px = c16[i];
                uint8_t r5=(px>>11)&0x1F, g6=(px>>5)&0x3F, b5=px&0x1F;
                st->result_preview[i] = (tezos_rgb888_t){(uint8_t)(r5*255/31),(uint8_t)(g6*255/63),(uint8_t)(b5*255/31)};
            }
            free(c16);
            break;
        }
    }
    free(downsampled);
    st->have_result = true;
}

static void capture_result_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    capture_state_t *st = (capture_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "CAPTURED", &TEZOS_FONT_HEADER, TEZOS_CYAN);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    /* Show as much of the result as fits the display, top-left
       anchored. Scrolling for presets larger than the display
       (800x480, 640x480) is real future UI work, not implemented here
       -- see docs/build-notes/camera-app-build-notes.md. This proves
       the pipeline output is real and correct; it doesn't yet build
       the scroll interaction. */
    int preview_y = 36;
    int avail_h = fb->height - preview_y - 34;
    for (int y = 0; y < fb->height - preview_y - 34 && y < st->result_h; y++) {
        for (int x = 0; x < fb->width && x < st->result_w; x++) {
            tezos_rgb888_t p = st->result_preview[y * st->result_w + x];
            tezos_draw_rect(fb, d, x, preview_y + y, 1, 1, (tezos_color_t){p.r, p.g, p.b});
        }
    }
    (void)avail_h;

    char info[64];
    snprintf(info, sizeof info, "%dx%d, %s", st->result_w, st->result_h, DEPTH_NAMES[st->depth]);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, info, &TEZOS_FONT_SMALL, TEZOS_MUTED);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t capture_result_input(tezos_screen_t *self, tezos_input_event_t ev,
                                           tezos_screen_t **next, bool *needs_redraw) {
    (void)self; (void)next; (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    return TEZOS_ACTION_NONE;
}

static tezos_action_t capture_input(tezos_screen_t *self, tezos_input_event_t ev,
                                    tezos_screen_t **next, bool *needs_redraw) {
    capture_state_t *st = (capture_state_t *)self->app_state;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_DOWN) {
        st->resolution = (tezos_cam_resolution_t)((st->resolution + 1) % 4);
        *needs_redraw = true;
        return TEZOS_ACTION_NONE;
    }
    if (ev.type == TEZOS_INPUT_UP) {
        st->depth = (tezos_cam_depth_t)((st->depth + 1) % 4);
        *needs_redraw = true;
        return TEZOS_ACTION_NONE;
    }
    if (ev.type == TEZOS_INPUT_HASH) {
        st->dither_size = (st->dither_size == TEZOS_CAM_DITHER_4x4) ? TEZOS_CAM_DITHER_8x8 : TEZOS_CAM_DITHER_4x4;
        *needs_redraw = true;
        return TEZOS_ACTION_NONE;
    }
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        perform_capture(st);
        g_capture_result_screen = (tezos_screen_t){
            .render = capture_result_render, .handle_input = capture_result_input,
            .on_resume = NULL, .on_tick = NULL,
            .app_state = st, .debug_name = "camera_capture_result",
        };
        *next = &g_capture_result_screen;
        return TEZOS_ACTION_PUSH;
    }
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *capture_settings_screen_get(void) {
    /* preserves the last-chosen settings across visits (doesn't reset
       resolution/depth/dither on re-entry) -- only the result buffer
       resets, since re-entering to change settings shouldn't discard
       anything the user hasn't explicitly retaken */
    g_capture_screen = (tezos_screen_t){
        .render = capture_render, .handle_input = capture_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_capture_state, .debug_name = "camera_capture_settings",
    };
    return &g_capture_screen;
}
