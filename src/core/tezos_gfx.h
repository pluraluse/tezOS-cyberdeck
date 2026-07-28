#pragma once
/*
 * tezOS render API — the primitives every screen/app draws with.
 *
 * Operates on a plain RGB565 pixel buffer. On real hardware this buffer
 * is a DRM dumb buffer (mapped via libdrm); in this dev/test harness it's
 * a malloc'd block written out to a PPM for visual verification. The API
 * is identical either way — that's the point: same code path for the SDL/
 * software dev loop and the real Pi Zero 2 W target (see ROADMAP.md M3).
 *
 * Dirty-rectangle tracking is built in from the start, not bolted on
 * later — SPI panel bandwidth is a real constraint (see
 * hardware-roadmap-build-notes.md), so full-frame redraws on every
 * change are not an option.
 */
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t *pixels;   /* RGB565, row-major, width*height */
    int width, height;
} tezos_fb_t;

typedef struct { uint8_t r, g, b; } tezos_color_t;

/* tezOS default palette — see docs/design-system/PALETTE.md.
   Defined here as the canonical values apps/core code should reference,
   rather than hardcoding hex elsewhere. */
extern const tezos_color_t TEZOS_BG;
extern const tezos_color_t TEZOS_PRIMARY;   /* Tezos Blue #0F61FF */
extern const tezos_color_t TEZOS_PINK;
extern const tezos_color_t TEZOS_CYAN;
extern const tezos_color_t TEZOS_AMBER;
extern const tezos_color_t TEZOS_TEXT;
extern const tezos_color_t TEZOS_MUTED;

/* Up to 8 dirty rects tracked per frame before we give up and mark the
   whole screen dirty — cheap for typical UI updates (a menu row
   highlight, a status icon change), falls back safely for big redraws. */
#define TEZOS_MAX_DIRTY_RECTS 8
typedef struct { int x, y, w, h; } tezos_rect_t;

typedef struct {
    tezos_rect_t rects[TEZOS_MAX_DIRTY_RECTS];
    int count;
    bool full_redraw; /* set when count would exceed the cap */
} tezos_dirty_t;

tezos_fb_t *tezos_fb_create(int width, int height);
void tezos_fb_destroy(tezos_fb_t *fb);

void tezos_dirty_reset(tezos_dirty_t *d);
void tezos_dirty_mark(tezos_dirty_t *d, int x, int y, int w, int h);

void tezos_draw_rect(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y, int w, int h, tezos_color_t color);
void tezos_draw_rect_outline(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y, int w, int h, tezos_color_t color);

/* Bitmap font glyph type is defined per-font-asset header
   (see src/core/fonts/tezos_*.h), all sharing this shape: */
struct tezos_glyph_generic { uint8_t w, h; int8_t xoff, yoff; uint8_t advance; const uint8_t *bitmap; };

typedef struct {
    const struct tezos_glyph_generic *glyphs; /* indexed by (code - first_char) */
    int first_char, last_char;
    int pixel_size, line_height;
} tezos_font_t;

/* Draws left-aligned text at (x,y) baseline-ish anchor (top-left of line).
   Returns total advance width in pixels (useful for centering). */
int tezos_draw_text(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y, const char *str,
                     const tezos_font_t *font, tezos_color_t color);
int tezos_text_width(const char *str, const tezos_font_t *font);

/* Icons share the same glyph-bitmap infrastructure as text fonts — an
   "icon font" is just a font asset where each codepoint's glyph is a
   small pictogram instead of a letter (see MEK-Dings, rasterized to
   src/core/fonts/tezos_mekdings_24.h). Which codepoint means "Wallet"
   vs "Settings" etc. is a design decision, not something this API
   assumes — see docs/design-system/ICONS.md for the mapping once
   chosen. */
typedef tezos_font_t tezos_icon_font_t;
void tezos_draw_icon(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y,
                      const tezos_icon_font_t *icon_font, char icon_code, tezos_color_t color);

/* Writes the current framebuffer to a PPM file — dev/test harness only,
   not part of the on-device build. Lets us visually verify the renderer
   without real DRM hardware. */
void tezos_fb_dump_ppm(const tezos_fb_t *fb, const char *path);
