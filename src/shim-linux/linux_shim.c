/*
 * tezOS Linux platform shim — DRM/KMS display + evdev input.
 *
 * This is the ONE piece that's genuinely untestable in a dev sandbox
 * (no real DRM device, no real Pi hardware here) — everything else in
 * src/core/ was verified by actually running it. This file compiles
 * cleanly against real libdrm headers (checked: `apt install libdrm-dev`,
 * `gcc ... $(pkg-config --cflags --libs libdrm)`), but the runtime
 * behavior below needs to be verified on the actual Pi Zero 2 W.
 *
 * What to check first on real hardware, in order:
 *   1. `ls /dev/dri/` shows a card after the piscreen overlay loads
 *   2. This program's drm_init() successfully finds a connected
 *      connector and a dumb-buffer pixel format that matches
 *      DRM_FORMAT_RGB565 (see the format-negotiation note below —
 *      if the panel driver only supports XRGB8888 dumb buffers, a
 *      conversion step is needed before flip(), not yet implemented here)
 *   3. `cat /proc/bus/input/devices` shows the TCA8418 keypad as a
 *      standard evdev device once the device-tree overlay (see
 *      hardware/pinouts/tca8418-overlay-notes.md) is loaded
 *
 * Build (on the Pi):
 *   gcc -O2 -Wall -Wextra -std=c11 -o tezos_shim linux_shim.c \
 *       ../core/tezos_core.c ../core/tezos_gfx.c ../core/tezos_fonts.c \
 *       ../core/tezos_widgets.c -I../core $(pkg-config --cflags --libs libdrm)
 */
#define _GNU_SOURCE /* for O_CLOEXEC */
#include "../core/tezos_core.h"
#include "../core/tezos_gfx.h"
#include "../core/tezos_fonts.h"
#include "../core/tezos_widgets.h"
#include "../apps/wallet/wallet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>

/* ---------- DRM display backend ---------- */

typedef struct {
    int fd;
    drmModeConnector *conn;
    drmModeModeInfo mode;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
    uint8_t *map;
    drmModeCrtc *saved_crtc; /* restore on exit, so a crash doesn't leave the display in a weird state */
} drm_dev_t;

static int drm_init(drm_dev_t *d, const char *node) {
    memset(d, 0, sizeof(*d));
    d->fd = open(node, O_RDWR | O_CLOEXEC);
    if (d->fd < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", node, strerror(errno));
        return -1;
    }

    drmModeRes *res = drmModeGetResources(d->fd);
    if (!res) {
        fprintf(stderr, "drmModeGetResources failed: %s\n", strerror(errno));
        return -1;
    }

    /* find the first connected connector — small panels driven by a
       single tinydrm-style overlay typically expose exactly one */
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *c = drmModeGetConnector(d->fd, res->connectors[i]);
        if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) {
            d->conn = c;
            break;
        }
        if (c) drmModeFreeConnector(c);
    }
    if (!d->conn) {
        fprintf(stderr, "no connected connector with a valid mode found\n");
        drmModeFreeResources(res);
        return -1;
    }
    d->mode = d->conn->modes[0]; /* first advertised mode; for a fixed
                                    panel this should be its native res */

    /* find an encoder/CRTC for this connector */
    drmModeEncoder *enc = NULL;
    if (d->conn->encoder_id) enc = drmModeGetEncoder(d->fd, d->conn->encoder_id);
    if (!enc) {
        for (int i = 0; i < d->conn->count_encoders; i++) {
            enc = drmModeGetEncoder(d->fd, d->conn->encoders[i]);
            if (enc) break;
        }
    }
    if (!enc) {
        fprintf(stderr, "no encoder found for connector\n");
        drmModeFreeResources(res);
        return -1;
    }
    d->crtc_id = enc->crtc_id;
    if (!d->crtc_id) {
        /* fall back to the first CRTC in the resource list */
        for (int i = 0; i < res->count_crtcs; i++) {
            if (enc->possible_crtcs & (1 << i)) { d->crtc_id = res->crtcs[i]; break; }
        }
    }
    drmModeFreeEncoder(enc);
    if (!d->crtc_id) {
        fprintf(stderr, "no usable CRTC found\n");
        drmModeFreeResources(res);
        return -1;
    }
    d->saved_crtc = drmModeGetCrtc(d->fd, d->crtc_id);
    drmModeFreeResources(res);

    /* Create a dumb buffer at RGB565 (16bpp) to match tezos_fb_t's
       native format exactly — no per-frame pixel conversion needed.
       NOTE: not all panel drivers support 16bpp dumb buffers; if
       drm_mode_create_dumb fails or drmModeAddFB2 rejects
       DRM_FORMAT_RGB565, the panel driver may only support XRGB8888 —
       check dmesg for the specific rejection reason on real hardware. */
    struct drm_mode_create_dumb creq = {0};
    creq.width = d->mode.hdisplay;
    creq.height = d->mode.vdisplay;
    creq.bpp = 16;
    if (drmIoctl(d->fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_CREATE_DUMB failed: %s\n", strerror(errno));
        return -1;
    }
    d->handle = creq.handle;
    d->pitch = creq.pitch;
    d->size = creq.size;

    if (drmModeAddFB2(d->fd, d->mode.hdisplay, d->mode.vdisplay, DRM_FORMAT_RGB565,
                       (uint32_t[4]){d->handle,0,0,0}, (uint32_t[4]){d->pitch,0,0,0},
                       (uint32_t[4]){0,0,0,0}, &d->fb_id, 0) < 0) {
        fprintf(stderr, "drmModeAddFB2 failed (panel may not support RGB565 dumb buffers): %s\n", strerror(errno));
        return -1;
    }

    struct drm_mode_map_dumb mreq = {0};
    mreq.handle = d->handle;
    if (drmIoctl(d->fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_MAP_DUMB failed: %s\n", strerror(errno));
        return -1;
    }
    d->map = mmap(0, d->size, PROT_READ | PROT_WRITE, MAP_SHARED, d->fd, mreq.offset);
    if (d->map == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return -1;
    }
    memset(d->map, 0, d->size);

    if (drmModeSetCrtc(d->fd, d->crtc_id, d->fb_id, 0, 0, &d->conn->connector_id, 1, &d->mode) < 0) {
        fprintf(stderr, "drmModeSetCrtc failed: %s\n", strerror(errno));
        return -1;
    }

    printf("DRM init OK: %dx%d, pitch=%u, size=%llu\n",
           d->mode.hdisplay, d->mode.vdisplay, d->pitch, (unsigned long long)d->size);
    return 0;
}

/* Copies the software tezos_fb_t buffer into the mapped DRM dumb buffer.
   Pitch may exceed width*2 bytes (row padding/alignment), so this
   copies row by row rather than assuming a flat memcpy — a common bug
   if the two happen to match on one panel but not another. */
static void drm_flip(drm_dev_t *d, const tezos_fb_t *fb, const tezos_dirty_t *dirty) {
    int y0 = 0, y1 = fb->height, x0 = 0, x1 = fb->width;
    if (dirty && !dirty->full_redraw && dirty->count > 0) {
        /* Simplification for v1: union of all dirty rects into one
           bounding box rather than copying each rect separately —
           correct, just not maximally bandwidth-optimal yet. Revisit
           if SPI throughput turns out to need per-rect precision. */
        y0 = fb->height; y1 = 0; x0 = fb->width; x1 = 0;
        for (int i = 0; i < dirty->count; i++) {
            tezos_rect_t r = dirty->rects[i];
            if (r.x < x0) x0 = r.x;
            if (r.y < y0) y0 = r.y;
            if (r.x + r.w > x1) x1 = r.x + r.w;
            if (r.y + r.h > y1) y1 = r.y + r.h;
        }
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > fb->width) x1 = fb->width;
        if (y1 > fb->height) y1 = fb->height;
    }
    for (int row = y0; row < y1; row++) {
        memcpy(d->map + row * d->pitch + x0 * 2,
               fb->pixels + row * fb->width + x0,
               (size_t)(x1 - x0) * 2);
    }
    /* Ask the driver to only push the changed region over SPI if it
       supports it — cheap to try, ignored if unsupported. This is the
       actual mitigation for the SPI-bandwidth constraint flagged in
       hardware-roadmap-build-notes.md. */
    drmModeClip clip = { .x1 = (unsigned short)x0, .y1 = (unsigned short)y0,
                          .x2 = (unsigned short)x1, .y2 = (unsigned short)y1 };
    drmModeDirtyFB(d->fd, d->fb_id, &clip, 1);
}

static void drm_cleanup(drm_dev_t *d) {
    if (d->saved_crtc) {
        drmModeSetCrtc(d->fd, d->saved_crtc->crtc_id, d->saved_crtc->buffer_id,
                       d->saved_crtc->x, d->saved_crtc->y, &d->conn->connector_id, 1, &d->saved_crtc->mode);
        drmModeFreeCrtc(d->saved_crtc);
    }
    if (d->map) munmap(d->map, d->size);
    if (d->conn) drmModeFreeConnector(d->conn);
    if (d->fd >= 0) close(d->fd);
}

/* ---------- evdev input backend ---------- */

/* Maps standard Linux keycodes (set via the TCA8418 device-tree overlay's
   linux,keymap) to tezOS input events. See
   hardware/pinouts/tca8418-overlay-notes.md for the actual keymap this
   assumes — change both together if you change one. */
static bool translate_key(int code, tezos_input_event_t *out) {
    switch (code) {
        case KEY_UP:     *out = (tezos_input_event_t){TEZOS_INPUT_UP, 0}; return true;
        case KEY_DOWN:   *out = (tezos_input_event_t){TEZOS_INPUT_DOWN, 0}; return true;
        case KEY_ENTER:  *out = (tezos_input_event_t){TEZOS_INPUT_SOFTKEY_L, 0}; return true;
        case KEY_ESC:    *out = (tezos_input_event_t){TEZOS_INPUT_SOFTKEY_R, 0}; return true;
        case KEY_KPASTERISK: *out = (tezos_input_event_t){TEZOS_INPUT_STAR, 0}; return true;
        case KEY_BACKSLASH:  *out = (tezos_input_event_t){TEZOS_INPUT_HASH, 0}; return true; /* '#' key, keymap-dependent */
        default:
            if (code >= KEY_1 && code <= KEY_9) {
                *out = (tezos_input_event_t){TEZOS_INPUT_NUM, code - KEY_1 + 1}; return true;
            }
            if (code == KEY_0) { *out = (tezos_input_event_t){TEZOS_INPUT_NUM, 0}; return true; }
            return false;
    }
}

#define MAX_INPUT_FDS 4
static int input_fds[MAX_INPUT_FDS];
static int input_fd_count = 0;

/* Pass explicit device paths rather than scanning /dev/input/star -- on a
   purpose-built device there are exactly two input sources (keypad,
   touch), known ahead of time, not something to auto-detect at runtime. */
static int open_input_device(const char *path) {
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "warning: could not open input device %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (input_fd_count < MAX_INPUT_FDS) input_fds[input_fd_count++] = fd;
    return fd;
}

/* ---------- app registry — wires idle's numbered list to real/stub screens ---------- */

static tezos_screen_t *app_entry_stub_explorer(void)  { return tezos_stub_screen_get("Explorer"); }
static tezos_screen_t *app_entry_stub_scanner(void)   { return tezos_stub_screen_get("Scanner"); }
static tezos_screen_t *app_entry_stub_gallery(void)   { return tezos_stub_screen_get("Gallery"); }
static tezos_screen_t *app_entry_stub_discover(void)  { return tezos_stub_screen_get("Discover"); }
static tezos_screen_t *app_entry_stub_messenger(void) { return tezos_stub_screen_get("Messenger"); }
static tezos_screen_t *app_entry_stub_camera(void)    { return tezos_stub_screen_get("Camera"); }
static tezos_screen_t *app_entry_stub_art(void)       { return tezos_stub_screen_get("1-bit Art"); }
static tezos_screen_t *app_entry_stub_settings(void)  { return tezos_stub_screen_get("Settings"); }

/* --- Idle screen (same logic as demo_idle_screen.c, but pushing via the
   app registry instead of a single hardcoded stub call, since this
   binary actually has all 9 apps registered) --- */
typedef struct { tezos_list_state_t menu; } idle_state_t;
static idle_state_t g_idle_state;
static tezos_core_t g_core;

static void idle_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    idle_state_t *st = (idle_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "WiFi ...", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    int logo_w = tezos_text_width("tezOS", &TEZOS_FONT_HEADER);
    tezos_draw_text(fb, d, (fb->width - logo_w) / 2, 60, "tezOS", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_list_render(fb, d, 8, 120, fb->width - 16, fb->height - 160, &TEZOS_FONT_BODY, &st->menu);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int menu_w = tezos_text_width("MENU", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - menu_w - 8, fb->height - 26, "MENU", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t idle_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                         tezos_screen_t **next, bool *needs_redraw) {
    idle_state_t *st = (idle_state_t *)self->app_state;
    if (tezos_list_handle_input(&st->menu, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        int idx = st->menu.selected;
        if (idx >= 0 && idx < g_core.app_count) {
            *next = g_core.apps[idx].entry();
            return TEZOS_ACTION_PUSH;
        }
    }
    return TEZOS_ACTION_NONE;
}

static tezos_list_item_t g_idle_menu_items[9];
static tezos_screen_t g_idle_screen = {
    .render = idle_render, .handle_input = idle_handle_input,
    .on_resume = NULL, .app_state = &g_idle_state, .debug_name = "idle",
};

int main(void) {
    /* --- register all 9 apps (real screens as they get built will
       replace the corresponding app_entry_stub_* function) --- */
    tezos_core_init(&g_core, NULL /* fb set below */);
    tezos_core_register_app(&g_core, (tezos_app_t){0, "Wallet",    wallet_entry_screen});
    tezos_core_register_app(&g_core, (tezos_app_t){1, "Explorer",  app_entry_stub_explorer});
    tezos_core_register_app(&g_core, (tezos_app_t){2, "Scanner",   app_entry_stub_scanner});
    tezos_core_register_app(&g_core, (tezos_app_t){3, "Gallery",   app_entry_stub_gallery});
    tezos_core_register_app(&g_core, (tezos_app_t){4, "Discover",  app_entry_stub_discover});
    tezos_core_register_app(&g_core, (tezos_app_t){5, "Messenger", app_entry_stub_messenger});
    tezos_core_register_app(&g_core, (tezos_app_t){6, "Camera",    app_entry_stub_camera});
    tezos_core_register_app(&g_core, (tezos_app_t){7, "1-bit Art", app_entry_stub_art});
    tezos_core_register_app(&g_core, (tezos_app_t){8, "Settings",  app_entry_stub_settings});

    for (int i = 0; i < g_core.app_count; i++) g_idle_menu_items[i] = (tezos_list_item_t){g_core.apps[i].name, NULL};
    tezos_list_init(&g_idle_state.menu, g_idle_menu_items, g_core.app_count);

    /* --- DRM display --- */
    drm_dev_t drm;
    if (drm_init(&drm, "/dev/dri/card0") < 0) {
        fprintf(stderr, "DRM init failed — see notes at top of this file. Falling back is not implemented; fix the panel overlay first.\n");
        return 1;
    }
    tezos_fb_t fb = { .pixels = (uint16_t *)drm.map, .width = drm.mode.hdisplay, .height = drm.mode.vdisplay };
    g_core.fb = &fb;

    /* --- evdev inputs — adjust these paths to match your actual devices,
       found via /proc/bus/input/devices --- */
    int keypad_fd = open_input_device("/dev/input/by-id/tca8418-keypad-event-kbd" /* placeholder path, find real one via /proc/bus/input/devices */);
    int touch_fd  = open_input_device("/dev/input/by-id/ads7846-event"           /* placeholder path, same */);
    (void)keypad_fd; (void)touch_fd; /* touch handling not yet implemented — keypad-first, per project's stated input priority */

    tezos_core_push(&g_core, &g_idle_screen);
    g_core.needs_render = true;
    tezos_core_tick(&g_core, NULL);
    drm_flip(&drm, &fb, &g_core.dirty);

    struct pollfd pfds[MAX_INPUT_FDS];
    for (int i = 0; i < input_fd_count; i++) { pfds[i].fd = input_fds[i]; pfds[i].events = POLLIN; }

    while (1) {
        int n = poll(pfds, input_fd_count, -1);
        if (n <= 0) continue;
        for (int i = 0; i < input_fd_count; i++) {
            if (!(pfds[i].revents & POLLIN)) continue;
            struct input_event iev;
            while (read(pfds[i].fd, &iev, sizeof(iev)) == sizeof(iev)) {
                if (iev.type == EV_KEY && iev.value == 1 /* key down only */) {
                    tezos_input_event_t tev;
                    if (translate_key(iev.code, &tev)) {
                        tezos_core_tick(&g_core, &tev);
                        if (g_core.needs_render) drm_flip(&drm, &fb, &g_core.dirty);
                    }
                }
            }
        }
    }

    drm_cleanup(&drm); /* unreachable in the loop above; here for completeness if a future exit path is added */
    return 0;
}
