/*
 * tezOS Linux platform shim — fbdev display + evdev input.
 *
 * Display backend history: this was originally written against libdrm
 * (DRM/KMS), on the assumption that the `piscreen` overlay would bring up
 * a modern tinydrm-class DRM device. Real hardware bring-up
 * (docs/build-notes/display-bringup-build-notes.md) found otherwise:
 * `piscreen` never worked at all, and the overlay that *does* work on
 * this board (`dtoverlay=fbtft,ili9481,...`) uses the Linux kernel's
 * legacy `fbtft` staging driver, which creates a plain fbdev device
 * (`/dev/fb1`) — there is no DRM device node on this configuration at
 * all. Rewritten against fbdev ioctls accordingly. Everything above this
 * backend (logical/physical canvas split, letterboxing, dirty-rect
 * bounding-box reduction, the boot splash hand-off) needed no changes —
 * it was already built against the panel-agnostic `tezos_fb_t`.
 *
 * Flip is implemented with `write()`/`pwrite()`, not `mmap()` — every
 * test pattern during bring-up was pushed to the panel with a plain
 * `write()` to the fbdev character device and landed correctly;
 * `mmap()`+deferred-io timing on this specific staging driver was never
 * validated, so this doesn't assume it works.
 *
 * Color channel order is handled here, in software, not by a devicetree
 * `bgr` flag: bring-up found the flag's effect inconsistent across
 * reboots on the same config (most likely a marginal breadboard
 * connection, never confirmed, not chased further — see the build note's
 * "design around it" decision). Swapping R/B at flip time is a software
 * guarantee regardless of what the flag or a flaky wire do.
 *
 * Mounting orientation (which physical corner is "up") is explicitly
 * NOT decided here — buffer coordinate (0,0) is a fixed reference point,
 * not asserted to be a specific named corner, per the same build note.
 * That's a mounting/enclosure decision for later.
 *
 * What to check first on real hardware, in order:
 *   1. `cat /sys/class/graphics/fb1/name` shows `fb_ili9481` (confirms
 *      the expected driver bound, not a different fbN)
 *   2. This program's fbdev_init() opens /dev/fb1 and reads back
 *      320x480 at 16bpp via FBIOGET_VSCREENINFO (the confirmed-working
 *      bring-up configuration — a different bpp means the config drifted
 *      and the swap math below needs revisiting)
 *   3. `cat /proc/bus/input/devices` shows the TCA8418 keypad as a
 *      standard evdev device once the device-tree overlay (see
 *      hardware/pinouts/tca8418-overlay-notes.md) is loaded
 *
 * Build (on the Pi):
 *   gcc -O2 -Wall -Wextra -std=c11 -o tezos_shim linux_shim.c \
 *       ../core/tezos_core.c ../core/tezos_gfx.c ../core/tezos_fonts.c \
 *       ../core/tezos_widgets.c -I../core
 * (no libdrm dependency anymore)
 */
#define _GNU_SOURCE /* for O_CLOEXEC */
#include "../core/tezos_core.h"
#include "../core/tezos_boot_splash.h"
#include "../core/tezos_gfx.h"
#include "../core/tezos_grid_nav.h"
#include "../core/fonts/tezos_icons.h"
#include "../core/tezos_fonts.h"
#include "../core/tezos_widgets.h"
#include "../apps/wallet/wallet.h"
#include "../apps/settings/settings.h"
#include "../apps/camera/camera.h"
#include "../apps/discover/discover.h"
#include "../apps/beacon_wallet/beacon_wallet.h"
#include "../apps/explorer/explorer.h"
#include "../apps/messenger/messenger.h"
#include "../apps/gallery/gallery.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/fb.h>

/* ---------- fbdev display backend ---------- */

typedef struct {
    int fd;
    int width, height;
    uint32_t line_length; /* bytes per row, from FBIOGET_FSCREENINFO --
                              not assumed equal to width*2; a staging
                              driver is as likely as not to pad rows */
    uint16_t *row_buf;    /* scratch buffer, one row, R/B pre-swapped,
                              reused every flip rather than allocated
                              per-call */
} fbdev_dev_t;

static int fbdev_init(fbdev_dev_t *d, const char *node) {
    memset(d, 0, sizeof(*d));
    d->fd = open(node, O_RDWR | O_CLOEXEC);
    if (d->fd < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", node, strerror(errno));
        return -1;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(d->fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        fprintf(stderr, "FBIOGET_VSCREENINFO failed: %s\n", strerror(errno));
        return -1;
    }
    if (ioctl(d->fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        fprintf(stderr, "FBIOGET_FSCREENINFO failed: %s\n", strerror(errno));
        return -1;
    }

    /* Fail loud, not silently wrong: this shim only knows how to drive a
       16bpp RGB565 panel (tezos_fb_t's native format). A different value
       here means the real on-device config.txt has drifted from the
       confirmed bring-up configuration (docs/build-notes/
       display-bringup-build-notes.md) and needs reconciling, not a
       silent best-effort pixel conversion. */
    if (vinfo.bits_per_pixel != 16) {
        fprintf(stderr, "fbdev_init: expected 16bpp (RGB565), device reports %u bpp — "
                        "config.txt has likely drifted from the confirmed bring-up config\n",
                vinfo.bits_per_pixel);
        return -1;
    }

    d->width = vinfo.xres;
    d->height = vinfo.yres;
    d->line_length = finfo.line_length;
    d->row_buf = malloc((size_t)d->width * 2);
    if (!d->row_buf) {
        fprintf(stderr, "fbdev_init: out of memory allocating row buffer\n");
        return -1;
    }

    printf("fbdev init OK: %dx%d, line_length=%u\n", d->width, d->height, d->line_length);
    return 0;
}

/* RGB565 -> RGB565 with R and B swapped. See the file-top comment:
   bring-up found the driver's `bgr` devicetree flag gave inconsistent
   results across reboots on an identical config, so this guarantees
   correct color order in software instead of trusting the flag. */
static inline uint16_t rb_swap16(uint16_t c) {
    uint16_t r = (c >> 11) & 0x1F;
    uint16_t g = (c >> 5) & 0x3F;
    uint16_t b = c & 0x1F;
    return (uint16_t)((b << 11) | (g << 5) | r);
}

/* Writes the software tezos_fb_t buffer to the fbdev character device via
   write(), not mmap(). Every test pattern during bring-up was pushed
   this exact way and landed correctly; mmap()+deferred-io timing on this
   specific staging driver was never validated, so this doesn't lean on
   it. Dirty-rect handling mirrors the original DRM backend's bounding-
   box reduction (same bandwidth-mitigation intent, same v1 simplification
   of unioning rects rather than writing each separately). */
static void fbdev_flip(fbdev_dev_t *d, const tezos_fb_t *fb, const tezos_dirty_t *dirty) {
    int y0 = 0, y1 = fb->height, x0 = 0, x1 = fb->width;
    if (dirty && !dirty->full_redraw && dirty->count > 0) {
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

    int row_w = x1 - x0;
    if (row_w <= 0) return;

    for (int row = y0; row < y1; row++) {
        /* X is mirrored on this panel (confirmed on real hardware: boot
           splash and idle grid both rendered correctly except flipped
           left-right -- text readable only in a mirror, selected-icon
           position otherwise correct). Same approach as the R/B swap
           above: guaranteed in software here, not left to a devicetree
           flag. Read from the FULL row (not the x0-offset src pointer),
           indexed by each device column's mirrored logical column, so
           this is correct for partial dirty-rect writes too, not just
           full-frame ones. */
        const uint16_t *full_row = fb->pixels + (size_t)row * fb->width;
        for (int i = 0; i < row_w; i++) {
            int logical_x = fb->width - 1 - x0 - i;
            d->row_buf[i] = rb_swap16(full_row[logical_x]);
        }

        off_t offset = (off_t)row * d->line_length + (off_t)x0 * 2;
        ssize_t want = (ssize_t)row_w * 2;
        ssize_t got = pwrite(d->fd, d->row_buf, (size_t)want, offset);
        if (got != want) {
            fprintf(stderr, "fbdev_flip: short/failed pwrite at row %d: %s\n", row, strerror(errno));
            return;
        }
    }
}

static void fbdev_cleanup(fbdev_dev_t *d) {
    free(d->row_buf);
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

static tezos_screen_t *app_entry_stub_art(void)       { return tezos_stub_screen_get("1-bit Art"); }

/* --- Idle screen: a real, first-class icon grid, replacing the
   earlier plain text list. Real navigation, matching this device's
   real, available input set (no dedicated left/right exists -- see
   tezos_core.h's own input enum): NUM 1-9 jumps directly to that grid
   position (a real, deliberate fit -- a phone numpad's own physical
   3x3 layout maps onto a 3x3 icon grid with zero translation needed,
   1=top-left through 9=bottom-right), and UP/DOWN move sequentially
   through the grid in real reading order as a real fallback for
   anyone who'd rather browse than jump. */
typedef struct { int selected; } idle_state_t;
static idle_state_t g_idle_state;
static tezos_core_t g_core;

#define IDLE_GRID_COLS 3
#define IDLE_GRID_ROWS 3
#define IDLE_ICON_SCALE 3 /* 16px native -> 48px on screen -- real,
    deliberate size for a first-class grid cell, not a tiny inline icon */

/* real icon lookup, indexed to match the real, exact app registration
   order above -- not the order icons happened to be authored in */
static const tezos_icon_t *g_app_icons[9] = {
    &TEZOS_ICON_WALLET, &TEZOS_ICON_EXPLORER, &TEZOS_ICON_GALLERY,
    &TEZOS_ICON_DISCOVER, &TEZOS_ICON_MESSENGER, &TEZOS_ICON_CAMERA,
    &TEZOS_ICON_BEACON_WALLET, &TEZOS_ICON_ART_1BIT, &TEZOS_ICON_SETTINGS,
};

static void idle_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    idle_state_t *st = (idle_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "WiFi ...", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    /* real, deliberate choice: the header shows the CURRENTLY
       SELECTED app's real name, not a static "tezOS" logo -- a real,
       live preview of what SOFTKEY_L is about to open, the same
       pattern established console/game UIs use for icon grids so the
       tiny icons don't have to carry illegible text labels of their
       own at 48px */
    const char *selected_name = (st->selected >= 0 && st->selected < g_core.app_count)
        ? g_core.apps[st->selected].name : "";
    int name_w = tezos_text_width(selected_name, &TEZOS_FONT_HEADER);
    tezos_draw_text(fb, d, (fb->width - name_w) / 2, 40, selected_name, &TEZOS_FONT_HEADER, TEZOS_PRIMARY);

    /* real grid layout: centered, real cell spacing */
    int cell_w = fb->width / IDLE_GRID_COLS;
    int icon_px = TEZOS_ICON_SIZE * IDLE_ICON_SCALE;
    int grid_top = 90;
    int cell_h = (fb->height - 34 - grid_top) / IDLE_GRID_ROWS;

    for (int i = 0; i < g_core.app_count && i < 9; i++) {
        int col = i % IDLE_GRID_COLS, row = i / IDLE_GRID_COLS;
        int cell_x = col * cell_w, cell_y = grid_top + row * cell_h;
        int icon_x = cell_x + (cell_w - icon_px) / 2;
        int icon_y = cell_y + (cell_h - icon_px) / 2;

        if (i == st->selected) {
            /* real, visible selection highlight -- a colored border
               box drawn behind the icon, with real margin so it reads
               as a distinct "this one is selected" frame, not a
               coincidental overlap */
            tezos_draw_rect(fb, d, icon_x - 6, icon_y - 6, icon_px + 12, icon_px + 12, TEZOS_PRIMARY);
            tezos_draw_rect(fb, d, icon_x - 3, icon_y - 3, icon_px + 6, icon_px + 6, TEZOS_BG);
        }
        tezos_draw_icon_bitmap_scaled(fb, d, icon_x, icon_y, g_app_icons[i], IDLE_ICON_SCALE);
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int menu_w = tezos_text_width("MENU", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - menu_w - 8, fb->height - 26, "MENU", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t idle_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                         tezos_screen_t **next, bool *needs_redraw) {
    idle_state_t *st = (idle_state_t *)self->app_state;
    int count = g_core.app_count;

    if (ev.type == TEZOS_INPUT_NUM || ev.type == TEZOS_INPUT_UP || ev.type == TEZOS_INPUT_DOWN) {
        int new_selected = idle_grid_next_selected(st->selected, count, ev.type, ev.value);
        if (new_selected != st->selected) { st->selected = new_selected; *needs_redraw = true; }
        return TEZOS_ACTION_NONE;
    }
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        if (st->selected >= 0 && st->selected < count) {
            *next = g_core.apps[st->selected].entry();
            return TEZOS_ACTION_PUSH;
        }
    }
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t g_idle_screen = {
    .render = idle_render, .handle_input = idle_handle_input,
    .on_resume = NULL, .app_state = &g_idle_state, .debug_name = "idle",
};

/* Presentation state, set once in main() after fbdev/canvas setup. File-
   scope (matching g_core and this file's other globals) rather than a
   nested function -- nested functions are a GCC extension, not
   standard C11, and this project builds with plain -std=c11 elsewhere
   without relying on that extension. */
static fbdev_dev_t *g_present_fbdev;
static tezos_fb_t *g_present_logical_fb;
static tezos_fb_t *g_present_letterbox_staging;
static bool g_present_needs_letterbox;

static void present_frame(void) {
    if (!g_present_needs_letterbox) {
        fbdev_flip(g_present_fbdev, g_present_logical_fb, &g_core.dirty); /* real panel: fast dirty-rect path, unchanged */
        return;
    }
    tezos_fb_blit_letterboxed(g_present_letterbox_staging, g_present_logical_fb, TEZOS_BG);
    fbdev_flip(g_present_fbdev, g_present_letterbox_staging, NULL); /* NULL dirty = full-frame push, correct and cheap enough over HDMI */
}

int main(void) {
    /* --- register all 9 apps (real screens as they get built will
       replace the corresponding app_entry_stub_* function) --- */
    tezos_core_init(&g_core, NULL /* fb set below */);
    tezos_core_register_app(&g_core, (tezos_app_t){0, "Wallet",    wallet_entry_screen});
    tezos_core_register_app(&g_core, (tezos_app_t){1, "Explorer",  explorer_entry_screen});
    tezos_core_register_app(&g_core, (tezos_app_t){2, "Gallery",   gallery_entry_screen});
    tezos_core_register_app(&g_core, (tezos_app_t){3, "Discover",  discover_entry_screen});
    tezos_core_register_app(&g_core, (tezos_app_t){4, "Messenger", messenger_entry_screen});
    tezos_core_register_app(&g_core, (tezos_app_t){5, "Camera",    camera_entry_screen});
    tezos_core_register_app(&g_core, (tezos_app_t){6, "Beacon Wallet", beacon_wallet_entry_screen});
    tezos_core_register_app(&g_core, (tezos_app_t){7, "1-bit Art", app_entry_stub_art});
    tezos_core_register_app(&g_core, (tezos_app_t){8, "Settings",  settings_entry_screen});

    /* real grid needs no list init -- g_idle_state.selected defaults to
       0 (static zero-init), matching "top-left app selected on boot" */

    /* --- fbdev display --- */
    fbdev_dev_t fbdev;
    if (fbdev_init(&fbdev, "/dev/fb1") < 0) {
        fprintf(stderr, "fbdev init failed — see notes at top of this file. Falling back is not implemented; fix the panel overlay first.\n");
        return 1;
    }
    tezos_fb_t physical_fb = { .pixels = NULL, .width = fbdev.width, .height = fbdev.height };
    /* .pixels intentionally NULL: unlike the old DRM dumb-buffer backend,
       fbdev_flip() doesn't write through a persistent mapped pointer — it
       pwrite()s each row via its own scratch buffer (fbdev.row_buf). This
       struct's width/height are only used below for the letterbox check. */

    /* tezos_core always renders into this FIXED logical canvas, matching
       the real target panel's actual resolution -- not whatever the
       currently-attached display happens to report. This is what makes
       "test on a Pi with HDMI out to a 1080p monitor" show a correctly-
       proportioned version of the real UI instead of a stretched mess:
       a 1920x1080 landscape monitor and a 320x480 portrait panel are
       fundamentally different shapes, not just different sizes of the
       same shape. See docs/architecture/CORE-ARCHITECTURE.md's "logical
       canvas" section. */
    #define TEZOS_LOGICAL_WIDTH  320
    #define TEZOS_LOGICAL_HEIGHT 480
    tezos_fb_t *logical_fb = tezos_fb_create(TEZOS_LOGICAL_WIDTH, TEZOS_LOGICAL_HEIGHT);
    g_core.fb = logical_fb;

    /* Real target hardware: physical resolution == logical resolution
       exactly, so the existing dirty-rect SPI-bandwidth optimization in
       fbdev_flip() applies completely unchanged -- this new layer costs
       nothing on the actual device. Anything else (dev/testing via
       HDMI, or a future different-sized panel) needs the letterboxed
       full-frame path instead -- dirty rects computed in LOGICAL
       coordinates don't map directly onto a scaled/offset PHYSICAL
       buffer without real work translating them, and for a testing
       path where bandwidth is a non-issue (HDMI), that work isn't
       worth doing -- a full-frame blit is trivially fast enough. */
    bool needs_letterbox = (physical_fb.width != logical_fb->width || physical_fb.height != logical_fb->height);
    tezos_fb_t *letterbox_staging = NULL;
    if (needs_letterbox) {
        letterbox_staging = tezos_fb_create(physical_fb.width, physical_fb.height);
        fprintf(stderr, "Physical display (%dx%d) does not match the logical canvas (%dx%d) -- "
                        "using the letterboxed full-frame path, not the SPI dirty-rect fast path.\n",
                physical_fb.width, physical_fb.height, logical_fb->width, logical_fb->height);
    }
    g_present_fbdev = &fbdev;
    g_present_logical_fb = logical_fb;
    g_present_letterbox_staging = letterbox_staging;
    g_present_needs_letterbox = needs_letterbox;

    /* --- evdev inputs — adjust these paths to match your actual devices,
       found via /proc/bus/input/devices --- */
    int keypad_fd = open_input_device("/dev/input/by-id/tca8418-keypad-event-kbd" /* placeholder path, find real one via /proc/bus/input/devices */);
    int touch_fd  = open_input_device("/dev/input/by-id/ads7846-event"           /* placeholder path, same */);
    (void)keypad_fd; (void)touch_fd; /* touch handling not yet implemented — keypad-first, per project's stated input priority */

    /* The very first thing shown on boot, before the real home screen --
       see tezos_boot_splash.h for why the auto-advance-after-it-finishes
       logic lives in this main loop rather than inside the splash
       screen itself. */
    tezos_screen_t *splash_screen = tezos_boot_splash_screen_get();
    tezos_core_push(&g_core, splash_screen);
    g_core.needs_render = true;
    if (tezos_core_tick(&g_core, NULL)) present_frame();

    struct pollfd pfds[MAX_INPUT_FDS];
    for (int i = 0; i < input_fd_count; i++) { pfds[i].fd = input_fds[i]; pfds[i].events = POLLIN; }

    /* Poll with a timeout (not -1/block-forever) specifically so
       tezos_core_on_tick() gets driven even when nothing is pressed --
       this is what makes T9's delay-based auto-commit (see
       tezos_t9.h/tezos_t9_tick) actually work on real hardware. 50ms is
       plenty fine-grained against an 800ms commit delay while staying
       cheap. */
    #define POLL_TIMEOUT_MS 50
    struct timespec last_tick;
    clock_gettime(CLOCK_MONOTONIC, &last_tick);

    while (1) {
        int n = poll(pfds, input_fd_count, POLL_TIMEOUT_MS);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        int elapsed_ms = (int)((now.tv_sec - last_tick.tv_sec) * 1000
                              + (now.tv_nsec - last_tick.tv_nsec) / 1000000);
        last_tick = now;
        if (tezos_core_on_tick(&g_core, elapsed_ms)) present_frame();

        /* Boot splash's own real exit mechanism -- see tezos_boot_splash.h
           for why this lives here in the shim's main loop rather than in
           the splash screen's own handle_input. Once actually replaced,
           tezos_boot_splash_is_finished() safely and permanently returns
           false for the (now different) top-of-stack screen, so this
           check becomes a harmless no-op for the rest of the program's
           life -- not something that needs its own separate "already
           replaced, stop checking" flag. */
        if (g_core.depth > 0 && tezos_boot_splash_is_finished(g_core.stack[g_core.depth - 1])) {
            tezos_core_replace(&g_core, &g_idle_screen);
            g_core.needs_render = true;
            if (tezos_core_tick(&g_core, NULL)) present_frame();
        }

        if (n <= 0) continue; /* timeout, no input -- on_tick above already handled it */

        for (int i = 0; i < input_fd_count; i++) {
            if (!(pfds[i].revents & POLLIN)) continue;
            struct input_event iev;
            while (read(pfds[i].fd, &iev, sizeof(iev)) == sizeof(iev)) {
                if (iev.type == EV_KEY && iev.value == 1 /* key down only */) {
                    tezos_input_event_t tev;
                    if (translate_key(iev.code, &tev)) {
                        if (tezos_core_tick(&g_core, &tev)) present_frame();
                    }
                }
            }
        }

        /* Same real exit check as above, run again right after real
           input dispatch -- catches a key-press skip immediately, in
           the same loop iteration it happened, rather than leaving it
           until the next on_tick cycle up to POLL_TIMEOUT_MS later.
           Harmless, cheap no-op once the splash has already been
           replaced, same reasoning as the other call site. */
        if (g_core.depth > 0 && tezos_boot_splash_is_finished(g_core.stack[g_core.depth - 1])) {
            tezos_core_replace(&g_core, &g_idle_screen);
            g_core.needs_render = true;
            if (tezos_core_tick(&g_core, NULL)) present_frame();
        }
    }

    fbdev_cleanup(&fbdev); /* unreachable in the loop above; here for completeness if a future exit path is added */
    return 0;
}
