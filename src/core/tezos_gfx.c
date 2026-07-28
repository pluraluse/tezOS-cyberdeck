#include "tezos_gfx.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const tezos_color_t TEZOS_BG      = {0x10, 0x14, 0x3A};
const tezos_color_t TEZOS_PRIMARY = {0x0F, 0x61, 0xFF};
const tezos_color_t TEZOS_PINK    = {0xFF, 0x3D, 0x8A};
const tezos_color_t TEZOS_CYAN    = {0x2F, 0xE6, 0xE6};
const tezos_color_t TEZOS_AMBER   = {0xFF, 0xA5, 0x3D};
const tezos_color_t TEZOS_TEXT    = {0xE4, 0xEA, 0xFB};
const tezos_color_t TEZOS_MUTED   = {0x88, 0x94, 0xC4};

static inline uint16_t rgb565(tezos_color_t c) {
    return (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
}

tezos_fb_t *tezos_fb_create(int width, int height) {
    tezos_fb_t *fb = malloc(sizeof(tezos_fb_t));
    fb->width = width;
    fb->height = height;
    fb->pixels = calloc((size_t)width * height, sizeof(uint16_t));
    return fb;
}

void tezos_fb_destroy(tezos_fb_t *fb) {
    if (!fb) return;
    free(fb->pixels);
    free(fb);
}

void tezos_dirty_reset(tezos_dirty_t *d) {
    d->count = 0;
    d->full_redraw = false;
}

void tezos_dirty_mark(tezos_dirty_t *d, int x, int y, int w, int h) {
    if (d->full_redraw) return;
    if (d->count >= TEZOS_MAX_DIRTY_RECTS) {
        /* Too many small updates this frame — cheaper to just redraw
           everything than track dozens of tiny rects individually. */
        d->full_redraw = true;
        return;
    }
    d->rects[d->count++] = (tezos_rect_t){x, y, w, h};
}

void tezos_draw_rect(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y, int w, int h, tezos_color_t color) {
    uint16_t px = rgb565(color);
    for (int row = y; row < y + h; row++) {
        if (row < 0 || row >= fb->height) continue;
        for (int col = x; col < x + w; col++) {
            if (col < 0 || col >= fb->width) continue;
            fb->pixels[row * fb->width + col] = px;
        }
    }
    if (d) tezos_dirty_mark(d, x, y, w, h);
}

void tezos_draw_rect_outline(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y, int w, int h, tezos_color_t color) {
    tezos_draw_rect(fb, d, x, y, w, 1, color);
    tezos_draw_rect(fb, d, x, y + h - 1, w, 1, color);
    tezos_draw_rect(fb, d, x, y, 1, h, color);
    tezos_draw_rect(fb, d, x + w - 1, y, 1, h, color);
}

static void blit_glyph(tezos_fb_t *fb, int x, int y, const struct tezos_glyph_generic *g, tezos_color_t color) {
    if (g->w == 0 || g->bitmap == NULL) return;
    int gx = x + g->xoff;
    int gy = y + g->yoff;
    for (int row = 0; row < g->h; row++) {
        int py = gy + row;
        if (py < 0 || py >= fb->height) continue;
        for (int col = 0; col < g->w; col++) {
            int px = gx + col;
            if (px < 0 || px >= fb->width) continue;
            uint8_t alpha = g->bitmap[row * g->w + col];
            if (alpha == 0) continue;
            /* Simple alpha blend against current background pixel so
               glyph edges anti-alias against whatever's underneath
               (usually TEZOS_BG, but not assumed — popups/highlights
               draw text over non-background fills too). */
            uint16_t bg565 = fb->pixels[py * fb->width + px];
            uint8_t bg_r = (bg565 >> 8) & 0xF8, bg_g = (bg565 >> 3) & 0xFC, bg_b = (bg565 << 3) & 0xF8;
            uint8_t out_r = (uint8_t)((color.r * alpha + bg_r * (255 - alpha)) / 255);
            uint8_t out_g = (uint8_t)((color.g * alpha + bg_g * (255 - alpha)) / 255);
            uint8_t out_b = (uint8_t)((color.b * alpha + bg_b * (255 - alpha)) / 255);
            fb->pixels[py * fb->width + px] = rgb565((tezos_color_t){out_r, out_g, out_b});
        }
    }
}

static const struct tezos_glyph_generic *glyph_for(const tezos_font_t *font, char c) {
    if (c < font->first_char || c > font->last_char) return NULL;
    return &font->glyphs[c - font->first_char];
}

int tezos_draw_text(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y, const char *str,
                     const tezos_font_t *font, tezos_color_t color) {
    int cursor = x;
    int start_x = x, min_y = y;
    for (const char *p = str; *p; p++) {
        const struct tezos_glyph_generic *g = glyph_for(font, *p);
        if (!g) continue;
        blit_glyph(fb, cursor, y, g, color);
        cursor += g->advance;
    }
    if (d) tezos_dirty_mark(d, start_x, min_y, cursor - start_x, font->line_height);
    return cursor - x;
}

int tezos_text_width(const char *str, const tezos_font_t *font) {
    int w = 0;
    for (const char *p = str; *p; p++) {
        const struct tezos_glyph_generic *g = glyph_for(font, *p);
        if (g) w += g->advance;
    }
    return w;
}

void tezos_draw_icon(tezos_fb_t *fb, tezos_dirty_t *d, int x, int y,
                      const tezos_icon_font_t *icon_font, char icon_code, tezos_color_t color) {
    const struct tezos_glyph_generic *g = glyph_for(icon_font, icon_code);
    if (!g) return;
    blit_glyph(fb, x, y, g, color);
    if (d) tezos_dirty_mark(d, x + g->xoff, y + g->yoff, g->w, g->h);
}

void tezos_fb_dump_ppm(const tezos_fb_t *fb, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", fb->width, fb->height);
    for (int i = 0; i < fb->width * fb->height; i++) {
        uint16_t px = fb->pixels[i];
        uint8_t r = (px >> 8) & 0xF8;
        uint8_t g = (px >> 3) & 0xFC;
        uint8_t b = (px << 3) & 0xF8;
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
}
