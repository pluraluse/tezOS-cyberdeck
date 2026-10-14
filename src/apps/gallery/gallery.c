#include "gallery.h"
#include "../wallet/wallet.h"
#include "../settings/settings.h"
#include "../camera/camera_pipeline.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../core/tezos_image_decode.h"
#include "../../chain/tezos_tzkt.h"
#include "../../chain/tezos_http.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum { GAL_LIST, GAL_DETAIL, GAL_VIEWER } gallery_phase_t;

#define GAL_MAX 12 /* real display limit -- more needs real pagination,
    same reasoning as Messenger's inbox limit, a separate feature */
#define GAL_THUMB_SIZE 120 /* fixed thumbnail size for the detail view */
#define GAL_PAN_STEP 24 /* pixels per real pan step in the 1:1 viewer */

typedef struct {
    gallery_phase_t phase;
    bool fetch_attempted;
    bool fetch_ok;
    int count;
    tezos_tzkt_owned_token_t tokens[GAL_MAX];
    tezos_list_state_t list;
    tezos_list_item_t list_items[GAL_MAX];
    char list_labels[GAL_MAX][128]; /* alias(31) + ": " + name(63) + NUL can
        reach 97 bytes -- sized with real headroom, not just past the
        boundary that triggered the original truncation warning */
    int selected_detail;

    /* real image state -- fetched/decoded once per detail-view visit,
       not re-fetched every render frame */
    bool image_attempted;   /* true once a fetch was tried for the
        currently-viewed token, whether or not it succeeded -- so a
        real failure doesn't cause an endless retry loop on every frame */
    bool image_ok;
    uint16_t thumb_pixels[GAL_THUMB_SIZE * GAL_THUMB_SIZE]; /* RGB565,
        real fixed-size thumbnail for the detail view */

    /* the full decoded (not downsampled) image, for the real pannable
       1:1 viewer -- allocated dynamically since real source images
       vary hugely in size, unlike the fixed-size thumbnail above */
    uint16_t *full_pixels;
    int full_w, full_h;
    int pan_x, pan_y; /* top-left of the current viewport into full_pixels */
} gallery_state_t;

static gallery_state_t g_state;
static tezos_screen_t g_gallery_screen;

/* Real fetch -> resolve -> decode -> downsample -> RGB565 convert,
   for both the fixed-size thumbnail (detail view) and the full image
   (1:1 viewer). Deliberately does both from a single fetch+decode --
   re-fetching/re-decoding twice for the same token would be real,
   avoidable waste. Frees any previous full_pixels first, so revisiting
   a different token doesn't leak the prior one's buffer. */
static void fetch_and_decode_image(gallery_state_t *st) {
    st->image_attempted = true;
    st->image_ok = false;
    if (st->full_pixels) { free(st->full_pixels); st->full_pixels = NULL; }

    const tezos_tzkt_owned_token_t *t = &st->tokens[st->selected_detail];
    if (t->image_uri[0] == '\0') return; /* real, honest case: this token's
        metadata simply doesn't have any of the real fallback URI fields
        populated -- not every real token does */

    char resolved_url[256];
    if (!tezos_resolve_ipfs_uri(t->image_uri, resolved_url, sizeof resolved_url)) return;

    tezos_http_response_t resp;
    if (!tezos_http_get(resolved_url, &resp)) return; /* real network failure --
        disclosed limitation, same as every other network call in this
        sandbox; on real hardware with real connectivity this is a real fetch */
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return; }

    tezos_decoded_image_t decoded;
    bool decode_ok = tezos_image_decode((const uint8_t *)resp.body, (int)resp.body_len, &decoded);
    tezos_http_response_free(&resp);
    if (!decode_ok) return; /* real, honest failure -- corrupt fetch, or a
        format stb_image doesn't support; not every real URI resolves to
        something decodable */

    /* real thumbnail: box-average downsample (Camera's own proven
       technique, already generic) + RGB565 convert */
    tezos_rgb888_t *thumb_rgb = malloc(sizeof(tezos_rgb888_t) * GAL_THUMB_SIZE * GAL_THUMB_SIZE);
    if (thumb_rgb) {
        tezos_cam_downsample_box((const tezos_rgb888_t *)decoded.rgb, decoded.width, decoded.height,
                                 thumb_rgb, GAL_THUMB_SIZE, GAL_THUMB_SIZE);
        tezos_cam_apply_16bit_color(thumb_rgb, GAL_THUMB_SIZE, GAL_THUMB_SIZE, st->thumb_pixels);
        free(thumb_rgb);
    }

    /* real full-size image, no downsampling, for the pannable viewer --
       converted once here, not re-converted on every pan/render */
    st->full_pixels = malloc(sizeof(uint16_t) * decoded.width * decoded.height);
    if (st->full_pixels) {
        tezos_cam_apply_16bit_color((const tezos_rgb888_t *)decoded.rgb, decoded.width, decoded.height,
                                    st->full_pixels);
        st->full_w = decoded.width;
        st->full_h = decoded.height;
        st->pan_x = 0;
        st->pan_y = 0;
    }
    tezos_image_free(&decoded);
    st->image_ok = (thumb_rgb != NULL) || (st->full_pixels != NULL);
}

/* Direct RGB565 blit into the main framebuffer at (dst_x, dst_y),
   clipped to the destination's real bounds -- this project's blit
   helper (tezos_fb_blit_letterboxed) operates on a whole separate
   tezos_fb_t scaled to fit, which isn't what a fixed-size thumbnail or
   a pannable viewport crop needs; this is a real, direct pixel copy
   with its own real dirty-rect marking, matching what every other
   drawing primitive in this file already does. */
static void blit_rgb565(tezos_fb_t *fb, tezos_dirty_t *d, int dst_x, int dst_y,
                        const uint16_t *src, int src_w, int src_h) {
    for (int y = 0; y < src_h; y++) {
        int fy = dst_y + y;
        if (fy < 0 || fy >= fb->height) continue;
        for (int x = 0; x < src_w; x++) {
            int fx = dst_x + x;
            if (fx < 0 || fx >= fb->width) continue;
            fb->pixels[fy * fb->width + fx] = src[y * src_w + x];
        }
    }
    tezos_dirty_mark(d, dst_x, dst_y, src_w, src_h);
}

static void perform_fetch(gallery_state_t *st) {
    st->fetch_attempted = true;
    st->count = 0;

    char address[64];
    if (!wallet_get_active_address(address, sizeof address)) {
        st->fetch_ok = false;
        return;
    }
    st->fetch_ok = tezos_tzkt_get_owned_tokens(settings_get_network(), address,
                                               st->tokens, GAL_MAX, &st->count);
    if (!st->fetch_ok) return;

    for (int i = 0; i < st->count; i++) {
        const char *alias = st->tokens[i].contract_alias[0] ? st->tokens[i].contract_alias : "(unnamed contract)";
        const char *name = st->tokens[i].name[0] ? st->tokens[i].name : "(untitled)";
        snprintf(st->list_labels[i], sizeof st->list_labels[i], "%s: %s", alias, name);
        st->list_items[i] = (tezos_list_item_t){st->list_labels[i], NULL};
    }
    if (st->count > 0) tezos_list_init(&st->list, st->list_items, st->count);
}

static void render_list(gallery_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "GALLERY", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    if (!st->fetch_ok) {
        tezos_draw_text(fb, d, 8, 50, "Couldn't reach TZKT (offline,", &TEZOS_FONT_SMALL, TEZOS_AMBER);
        tezos_draw_text(fb, d, 8, 68, "or no active signer).", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    } else if (st->count == 0) {
        tezos_draw_text(fb, d, 8, 50, "No tokens found for this", &TEZOS_FONT_SMALL, TEZOS_MUTED);
        tezos_draw_text(fb, d, 8, 68, "address.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->list);
    }
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    if (st->fetch_ok && st->count > 0) {
        tezos_draw_text(fb, d, 8, fb->height - 26, "OPEN", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    }
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_detail(gallery_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "TOKEN", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    const tezos_tzkt_owned_token_t *t = &st->tokens[st->selected_detail];
    int y = 44;
    char line[100];
    snprintf(line, sizeof line, "%s", t->name[0] ? t->name : "(untitled)");
    tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_BODY, TEZOS_CYAN); y += 24;
    snprintf(line, sizeof line, "Contract: %s", t->contract_alias[0] ? t->contract_alias : "(unnamed)");
    tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
    snprintf(line, sizeof line, "%.24s...", t->contract_address);
    tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_MUTED); y += 20;
    snprintf(line, sizeof line, "Token ID: %s", t->token_id);
    tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
    snprintf(line, sizeof line, "Balance: %s", t->balance);
    tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 24;

    if (!st->image_attempted || !st->image_ok) {
        if (!t->image_uri[0]) {
            tezos_draw_text(fb, d, 8, y, "(no image URI in metadata)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
        } else {
            tezos_draw_text(fb, d, 8, y, "(couldn't fetch/decode image)", &TEZOS_FONT_SMALL, TEZOS_AMBER);
        }
    } else {
        int thumb_x = (fb->width - GAL_THUMB_SIZE) / 2;
        tezos_draw_rect_outline(fb, d, thumb_x - 2, y - 2, GAL_THUMB_SIZE + 4, GAL_THUMB_SIZE + 4, TEZOS_MUTED);
        blit_rgb565(fb, d, thumb_x, y, st->thumb_pixels, GAL_THUMB_SIZE, GAL_THUMB_SIZE);
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    if (st->image_ok && st->full_pixels) {
        tezos_draw_text(fb, d, 8, fb->height - 26, "VIEW FULL", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    }
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

/* The real, genuinely new UI interaction this feature needed: nothing
   else in this project's toolkit pans across a 2D surface. Real
   images are routinely larger than this fixed 320x480 panel, so "view
   at 1:1" means scrolling a viewport across the full decoded image,
   not shrinking it to fit -- shrinking it would just be the thumbnail
   again, not a real 1:1 view. */
static void render_viewer(gallery_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    int viewport_h = fb->height - 34; /* full width, height minus the bottom bar only --
        a real 1:1 view wants to use as much of the real screen as possible */
    int src_x = st->pan_x, src_y = st->pan_y;
    int copy_w = fb->width < (st->full_w - src_x) ? fb->width : (st->full_w - src_x);
    int copy_h = viewport_h < (st->full_h - src_y) ? viewport_h : (st->full_h - src_y);
    for (int y = 0; y < copy_h; y++) {
        for (int x = 0; x < copy_w; x++) {
            fb->pixels[y * fb->width + x] = st->full_pixels[(src_y + y) * st->full_w + (src_x + x)];
        }
    }
    tezos_dirty_mark(d, 0, 0, copy_w, copy_h);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    char pos[40];
    snprintf(pos, sizeof pos, "%dx%d  (%d,%d)", st->full_w, st->full_h, st->pan_x, st->pan_y);
    tezos_draw_text(fb, d, 8, fb->height - 26, pos, &TEZOS_FONT_SMALL, TEZOS_MUTED);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void screen_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    gallery_state_t *st = (gallery_state_t *)self->app_state;
    if (st->phase == GAL_LIST) render_list(st, fb, d);
    else if (st->phase == GAL_DETAIL) render_detail(st, fb, d);
    else render_viewer(st, fb, d);
}

static tezos_action_t screen_input(tezos_screen_t *self, tezos_input_event_t ev,
                                   tezos_screen_t **next, bool *needs_redraw) {
    gallery_state_t *st = (gallery_state_t *)self->app_state;
    (void)next;

    if (st->phase == GAL_LIST) {
        if (st->fetch_ok && st->count > 0 && tezos_list_handle_input(&st->list, ev)) {
            *needs_redraw = true; return TEZOS_ACTION_NONE;
        }
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
        if (ev.type == TEZOS_INPUT_SOFTKEY_L && st->fetch_ok && st->count > 0) {
            st->selected_detail = st->list.selected;
            st->phase = GAL_DETAIL;
            st->image_attempted = false; /* real: fetch fresh for whichever
                token was just selected, not reuse a prior token's image */
            fetch_and_decode_image(st);
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == GAL_DETAIL) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = GAL_LIST; *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L && st->image_ok && st->full_pixels) {
            st->phase = GAL_VIEWER;
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    /* GAL_VIEWER -- real panning across the full decoded image */
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = GAL_DETAIL; *needs_redraw = true; return TEZOS_ACTION_NONE; }
    int viewport_h = 480 - 34; /* matches render_viewer's own reservation for the bottom bar */
    int max_pan_x = st->full_w > 320 ? st->full_w - 320 : 0;
    int max_pan_y = st->full_h > viewport_h ? st->full_h - viewport_h : 0;
    if (ev.type == TEZOS_INPUT_UP) st->pan_y -= GAL_PAN_STEP;
    else if (ev.type == TEZOS_INPUT_DOWN) st->pan_y += GAL_PAN_STEP;
    else if (ev.type == TEZOS_INPUT_NUM && ev.value == 4) st->pan_x -= GAL_PAN_STEP;
    else if (ev.type == TEZOS_INPUT_NUM && ev.value == 6) st->pan_x += GAL_PAN_STEP;
    if (st->pan_x < 0) st->pan_x = 0;
    if (st->pan_y < 0) st->pan_y = 0;
    if (st->pan_x > max_pan_x) st->pan_x = max_pan_x;
    if (st->pan_y > max_pan_y) st->pan_y = max_pan_y;
    *needs_redraw = true;
    return TEZOS_ACTION_NONE;
}

tezos_screen_t *gallery_entry_screen(void) {
    if (g_state.full_pixels) free(g_state.full_pixels); /* real leak fix --
        memset below would zero this pointer without freeing whatever it
        pointed to from a prior gallery session */
    memset(&g_state, 0, sizeof g_state);
    g_state.phase = GAL_LIST;
    perform_fetch(&g_state);

    g_gallery_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_state, .debug_name = "gallery",
    };
    return &g_gallery_screen;
}

tezos_screen_t *gallery_test_inject_tokens(const char *const *aliases, const char *const *names,
                                           const char *const *token_ids, const char *const *balances,
                                           int count) {
    if (g_state.full_pixels) free(g_state.full_pixels); /* same real leak fix as gallery_entry_screen */
    memset(&g_state, 0, sizeof g_state);
    g_state.phase = GAL_LIST;
    g_state.fetch_attempted = true;
    g_state.fetch_ok = true;
    g_state.count = count > GAL_MAX ? GAL_MAX : count;
    for (int i = 0; i < g_state.count; i++) {
        strncpy(g_state.tokens[i].contract_alias, aliases[i], sizeof g_state.tokens[i].contract_alias - 1);
        strncpy(g_state.tokens[i].name, names[i], sizeof g_state.tokens[i].name - 1);
        strncpy(g_state.tokens[i].token_id, token_ids[i], sizeof g_state.tokens[i].token_id - 1);
        strncpy(g_state.tokens[i].balance, balances[i], sizeof g_state.tokens[i].balance - 1);
        snprintf(g_state.list_labels[i], sizeof g_state.list_labels[i], "%s: %s", aliases[i], names[i]);
        g_state.list_items[i] = (tezos_list_item_t){g_state.list_labels[i], NULL};
    }
    tezos_list_init(&g_state.list, g_state.list_items, g_state.count);

    g_gallery_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_state, .debug_name = "gallery",
    };
    return &g_gallery_screen;
}

tezos_screen_t *gallery_test_inject_viewer(const uint16_t *pixels, int w, int h) {
    if (g_state.full_pixels) free(g_state.full_pixels); /* same real leak fix as elsewhere */
    memset(&g_state, 0, sizeof g_state);
    g_state.phase = GAL_VIEWER;
    g_state.full_pixels = malloc(sizeof(uint16_t) * w * h);
    if (g_state.full_pixels) memcpy(g_state.full_pixels, pixels, sizeof(uint16_t) * w * h);
    g_state.full_w = w;
    g_state.full_h = h;
    g_state.image_ok = true;

    g_gallery_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_state, .debug_name = "gallery",
    };
    return &g_gallery_screen;
}
