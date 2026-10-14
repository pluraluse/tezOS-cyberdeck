#include "discover.h"
#include "../wallet/wallet.h"
#include "../settings/settings.h"
#include "../camera/camera_pipeline.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../core/tezos_t9.h"
#include "../../core/tezos_image_decode.h"
#include "../../core/tezos_address_book.h"
#include "../../chain/tezos_tzkt.h"
#include "../../chain/tezos_http.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    DISC_HOME, DISC_ADDRESS_ENTRY, DISC_SITE,
    DISC_HOLDINGS_LIST, DISC_HOLDINGS_DETAIL, DISC_HOLDINGS_VIEWER,
} discover_phase_t;

#define DISC_MAX_BOOKMARKS TEZOS_ADDRESS_BOOK_MAX_CONTACTS

/* Real, small, deliberately short "Suggested" list -- NOT the same
   thing as the user's own real bookmarks (see discover.h's own
   header comment on why this project doesn't ship a large, vendor-
   curated address list by default). Each entry here was cross-
   verified against multiple independent real sources before being
   included, not taken from a single, unverifiable claim -- matching
   this project's own hard-learned lesson about exactly that kind of
   mistake:
   - Teia Marketplace: confirmed directly on Teia's own official
     "about" page (teia.art/about), Teia's own official GitHub smart-
     contracts repo, and tzkt.io's own indexer labeling -- 3+
     independent real sources agreeing, one of them the platform's own
     official site.
   - objkt.com Marketplace v2: confirmed by objkt's own official FAQ
     documentation, tzkt.io's own indexer labeling, and a third-party
     analytics dashboard -- 3 independent real sources agreeing.
   - Liquidity Baking (real, already extensively verified elsewhere in
     this project's own prior work, live-tested on both mainnet and
     Shadownet, not a new claim here).
   Visually and structurally distinct from real bookmarks in the UI
   (see render_home) -- never presented with the same confidence as
   the user's own, actually-verified-by-them list. */
#define DISC_SUGGESTED_COUNT 3
typedef struct { const char *label; const char *address; } discover_suggested_t;
static const discover_suggested_t DISC_SUGGESTED[DISC_SUGGESTED_COUNT] = {
    {"Teia Marketplace", "KT1PHubm9HtyQEJ4BBpMTVomq6mhbfNZ9z5w"},
    {"objkt.com Marketplace v2", "KT1WvzYHCNBvDSdwafTHv7nJ1dWmZ8GCYuuC"},
    {"Liquidity Baking", "KT1TxqZ8QtKvLu3V3JH7Gx58n7Co8pgtpQU5"},
};
#define DISC_MAX_HOLDINGS 12
#define DISC_THUMB_SIZE 120
#define DISC_PAN_STEP 24

typedef struct {
    discover_phase_t phase;

    /* real bookmark list -- the address book itself, exactly like a
       real browser's bookmarks: user-curated, not vendor-curated.
       +1 for "+ Visit New Address", + DISC_SUGGESTED_COUNT for the
       real, small, cross-verified Suggested entries appended after
       the user's own real bookmarks -- see DISC_SUGGESTED's own
       comment for why these are visually and structurally distinct,
       not presented with the same confidence as real bookmarks */
    tezos_list_item_t bookmark_items[DISC_MAX_BOOKMARKS + 1 + DISC_SUGGESTED_COUNT];
    char bookmark_labels[DISC_MAX_BOOKMARKS + 1 + DISC_SUGGESTED_COUNT][TEZOS_ADDRESS_BOOK_NAME_MAX];
    tezos_list_state_t bookmark_list;
    int real_bookmark_count; /* real count of address-book-backed rows
        (the "+ Visit New Address" row plus real bookmarks), so
        selection logic knows where the Suggested rows begin */

    tezos_t9_state_t address_entry_t9;

    /* the currently-visited address's real info */
    char visited_address[64];
    bool site_fetch_attempted;
    bool site_fetch_ok;
    tezos_tzkt_contract_info_t site_info;
    int entrypoint_count; /* -1 if not yet checked */
    bool is_bookmarked;

    /* real holdings at the visited address */
    bool holdings_fetch_attempted;
    bool holdings_fetch_ok;
    int holdings_count;
    tezos_tzkt_owned_token_t holdings[DISC_MAX_HOLDINGS];
    tezos_list_item_t holdings_items[DISC_MAX_HOLDINGS];
    char holdings_labels[DISC_MAX_HOLDINGS][128];
    tezos_list_state_t holdings_list;
    int selected_holding;

    /* real image state for the selected holding -- mirrors Gallery's
       own, already-proven pipeline. Real, disclosed duplication from
       gallery.c rather than a shared extraction in this pass -- see
       discover.h's own header comment. */
    bool image_attempted;
    bool image_ok;
    uint16_t thumb_pixels[DISC_THUMB_SIZE * DISC_THUMB_SIZE];
    uint16_t *full_pixels;
    int full_w, full_h;
    int pan_x, pan_y;
} discover_state_t;

static discover_state_t g_state;
static tezos_screen_t g_screen;

static void refresh_bookmarks(discover_state_t *st) {
    strcpy(st->bookmark_labels[0], "+ Visit New Address");
    st->bookmark_items[0] = (tezos_list_item_t){st->bookmark_labels[0], NULL};
    int count = tezos_address_book_count();
    for (int i = 0; i < count && i < DISC_MAX_BOOKMARKS; i++) {
        tezos_address_book_entry_t entry;
        tezos_address_book_get(i, &entry);
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstringop-truncation"
        strncpy(st->bookmark_labels[i + 1], entry.name, sizeof st->bookmark_labels[i + 1] - 1);
        #pragma GCC diagnostic pop
        st->bookmark_labels[i + 1][sizeof st->bookmark_labels[i + 1] - 1] = '\0';
        st->bookmark_items[i + 1] = (tezos_list_item_t){st->bookmark_labels[i + 1], NULL};
    }
    st->real_bookmark_count = count + 1; /* "+ Visit New Address" plus real bookmarks --
        Suggested rows start right after this, real selection logic below depends on it */

    /* real, small Suggested list appended after real bookmarks --
       "★" prefix is a real, deliberate visual distinction so these
       are never mistaken for the user's own, actually-verified
       bookmarks (see DISC_SUGGESTED's own comment) */
    for (int i = 0; i < DISC_SUGGESTED_COUNT; i++) {
        int idx = st->real_bookmark_count + i;
        snprintf(st->bookmark_labels[idx], sizeof st->bookmark_labels[idx],
                "\xE2\x98\x85 %s", DISC_SUGGESTED[i].label); /* UTF-8 for a real ★ character */
        st->bookmark_items[idx] = (tezos_list_item_t){st->bookmark_labels[idx], NULL};
    }

    tezos_list_init(&st->bookmark_list, st->bookmark_items, st->real_bookmark_count + DISC_SUGGESTED_COUNT);
}

static void fetch_and_decode_image(discover_state_t *st, const char *image_uri) {
    st->image_attempted = true;
    st->image_ok = false;
    if (st->full_pixels) { free(st->full_pixels); st->full_pixels = NULL; }
    if (!image_uri || !image_uri[0]) return;

    char resolved_url[256];
    if (!tezos_resolve_ipfs_uri(image_uri, resolved_url, sizeof resolved_url)) return;

    tezos_http_response_t resp;
    if (!tezos_http_get(resolved_url, &resp)) return;
    if (resp.status_code != 200) { tezos_http_response_free(&resp); return; }

    tezos_decoded_image_t decoded;
    bool decode_ok = tezos_image_decode((const uint8_t *)resp.body, (int)resp.body_len, &decoded);
    tezos_http_response_free(&resp);
    if (!decode_ok) return;

    tezos_rgb888_t *thumb_rgb = malloc(sizeof(tezos_rgb888_t) * DISC_THUMB_SIZE * DISC_THUMB_SIZE);
    if (thumb_rgb) {
        tezos_cam_downsample_box((const tezos_rgb888_t *)decoded.rgb, decoded.width, decoded.height,
                                 thumb_rgb, DISC_THUMB_SIZE, DISC_THUMB_SIZE);
        tezos_cam_apply_16bit_color(thumb_rgb, DISC_THUMB_SIZE, DISC_THUMB_SIZE, st->thumb_pixels);
        free(thumb_rgb);
    }

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

/* Real fetch for a newly-visited address: real contract info + real
   entrypoint count (Explorer's own real functions, unchanged) + real
   holdings (Gallery's own real function, unchanged -- already
   generic, takes an explicit address). This is the actual "load the
   page" moment. */
static void visit_address(discover_state_t *st, const char *address) {
    strncpy(st->visited_address, address, sizeof st->visited_address - 1);
    st->visited_address[sizeof st->visited_address - 1] = '\0';

    st->site_fetch_attempted = true;
    st->site_fetch_ok = tezos_tzkt_get_contract_info(settings_get_network(), address, &st->site_info);

    tezos_tzkt_entrypoint_t entrypoints[32];
    int ep_count = 0;
    if (tezos_tzkt_get_entrypoints_with_types(settings_get_network(), address, entrypoints, 32, &ep_count)) {
        st->entrypoint_count = ep_count;
    } else {
        st->entrypoint_count = -1;
    }

    st->holdings_fetch_attempted = true;
    st->holdings_fetch_ok = tezos_tzkt_get_owned_tokens(settings_get_network(), address,
                                                        st->holdings, DISC_MAX_HOLDINGS, &st->holdings_count);
    if (st->holdings_fetch_ok) {
        for (int i = 0; i < st->holdings_count; i++) {
            snprintf(st->holdings_labels[i], sizeof st->holdings_labels[i], "%s",
                    st->holdings[i].name[0] ? st->holdings[i].name : st->holdings[i].token_id);
            st->holdings_items[i] = (tezos_list_item_t){st->holdings_labels[i], NULL};
        }
        tezos_list_init(&st->holdings_list, st->holdings_items, st->holdings_count);
    }

    /* real check: is this address already saved? -- controls whether
       the SITE screen offers "BOOKMARK" or shows it's already saved */
    st->is_bookmarked = false;
    int bcount = tezos_address_book_count();
    for (int i = 0; i < bcount; i++) {
        tezos_address_book_entry_t entry;
        tezos_address_book_get(i, &entry);
        if (strcmp(entry.address, address) == 0) { st->is_bookmarked = true; break; }
    }

    st->phase = DISC_SITE;
}

static void render_home(discover_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "DISCOVER", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->bookmark_list);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "VISIT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_address_entry(discover_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "VISIT ADDRESS", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "Address (tz1/KT1):", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 70, &TEZOS_FONT_BODY, &st->address_entry_t9);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "GO", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_site(discover_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SITE", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    int y = 44;
    char line[100];
    snprintf(line, sizeof line, "%.30s...", st->visited_address);
    tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_MUTED); y += 24;

    if (!st->site_fetch_ok) {
        tezos_draw_text(fb, d, 8, y, "(couldn't fetch real info)", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    } else {
        snprintf(line, sizeof line, "%s", st->site_info.alias[0] ? st->site_info.alias : "(no alias)");
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_BODY, TEZOS_CYAN); y += 24;
        snprintf(line, sizeof line, "Balance: %s mutez", st->site_info.balance);
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
        snprintf(line, sizeof line, "Transactions: %d", st->site_info.num_transactions);
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
        if (st->entrypoint_count >= 0) {
            snprintf(line, sizeof line, "Entrypoints: %d", st->entrypoint_count);
            tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
        }
        snprintf(line, sizeof line, "Holdings: %s", st->holdings_fetch_ok ?
                (st->holdings_count > 0 ? "real items found" : "none") : "(couldn't fetch)");
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
    }

    y += 8;
    tezos_draw_text(fb, d, 8, y, st->is_bookmarked ? "(bookmarked)" : "STAR: bookmark this address",
                    &TEZOS_FONT_SMALL, TEZOS_MUTED);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    if (st->holdings_fetch_ok && st->holdings_count > 0) {
        tezos_draw_text(fb, d, 8, fb->height - 26, "BROWSE", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    }
    int backw = tezos_text_width("HOME", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "HOME", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_holdings_list(discover_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "HOLDINGS", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->holdings_list);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_holdings_detail(discover_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "ITEM", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    const tezos_tzkt_owned_token_t *t = &st->holdings[st->selected_holding];
    int y = 44;
    tezos_draw_text(fb, d, 8, y, t->name[0] ? t->name : "(untitled)", &TEZOS_FONT_BODY, TEZOS_CYAN);
    y += 28;

    if (!st->image_attempted || !st->image_ok) {
        tezos_draw_text(fb, d, 8, y, t->image_uri[0] ? "(couldn't fetch/decode image)" : "(no image)",
                        &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        int thumb_x = (fb->width - DISC_THUMB_SIZE) / 2;
        tezos_draw_rect_outline(fb, d, thumb_x - 2, y - 2, DISC_THUMB_SIZE + 4, DISC_THUMB_SIZE + 4, TEZOS_MUTED);
        blit_rgb565(fb, d, thumb_x, y, st->thumb_pixels, DISC_THUMB_SIZE, DISC_THUMB_SIZE);
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    if (st->image_ok && st->full_pixels) {
        tezos_draw_text(fb, d, 8, fb->height - 26, "VIEW FULL", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    }
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_holdings_viewer(discover_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    int viewport_h = fb->height - 34;
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
    discover_state_t *st = (discover_state_t *)self->app_state;
    switch (st->phase) {
        case DISC_HOME: render_home(st, fb, d); break;
        case DISC_ADDRESS_ENTRY: render_address_entry(st, fb, d); break;
        case DISC_SITE: render_site(st, fb, d); break;
        case DISC_HOLDINGS_LIST: render_holdings_list(st, fb, d); break;
        case DISC_HOLDINGS_DETAIL: render_holdings_detail(st, fb, d); break;
        case DISC_HOLDINGS_VIEWER: render_holdings_viewer(st, fb, d); break;
    }
}

static tezos_action_t screen_input(tezos_screen_t *self, tezos_input_event_t ev,
                                   tezos_screen_t **next, bool *needs_redraw) {
    (void)next;
    discover_state_t *st = (discover_state_t *)self->app_state;

    if (st->phase == DISC_HOME) {
        if (tezos_list_handle_input(&st->bookmark_list, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            int sel = st->bookmark_list.selected;
            if (sel == 0) {
                tezos_t9_init(&st->address_entry_t9);
                st->phase = DISC_ADDRESS_ENTRY;
            } else if (sel < st->real_bookmark_count) {
                /* a real, user-saved bookmark */
                tezos_address_book_entry_t entry;
                if (tezos_address_book_get(sel - 1, &entry)) {
                    visit_address(st, entry.address);
                }
            } else {
                /* a real Suggested entry -- same real visit_address() flow,
                   just sourced from the small, cross-verified static list
                   instead of the address book */
                int suggested_idx = sel - st->real_bookmark_count;
                if (suggested_idx >= 0 && suggested_idx < DISC_SUGGESTED_COUNT) {
                    visit_address(st, DISC_SUGGESTED[suggested_idx].address);
                }
            }
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == DISC_ADDRESS_ENTRY) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = DISC_HOME; *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            const char *typed = tezos_t9_get_text(&st->address_entry_t9);
            if (typed[0]) visit_address(st, typed);
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        *needs_redraw = tezos_t9_handle_input(&st->address_entry_t9, ev);
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == DISC_SITE) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) {
            st->phase = DISC_HOME;
            refresh_bookmarks(st); /* real refresh -- a bookmark may have just been added */
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        /* real, deliberate design fix: browsing and bookmarking are two
           independent real actions, not one gated behind the other --
           a real browser lets you browse a page without bookmarking it
           first. STAR bookmarks (a real, intuitive mapping); SOFTKEY_L
           always browses when there's something to browse, regardless
           of bookmark status. */
        if (ev.type == TEZOS_INPUT_STAR && !st->is_bookmarked) {
            char default_name[TEZOS_ADDRESS_BOOK_NAME_MAX];
            /* real, intentional truncation if the alias/address is
               longer than a bookmark name's real 32-char limit --
               snprintf always null-terminates safely within the
               given size (unlike strncpy), so this is a deliberate
               length constraint, not a safety bug */
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(default_name, sizeof default_name, "%s",
                    st->site_info.alias[0] ? st->site_info.alias : st->visited_address);
            #pragma GCC diagnostic pop
            if (tezos_address_book_add(default_name, st->visited_address)) st->is_bookmarked = true;
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L && st->holdings_fetch_ok && st->holdings_count > 0) {
            st->phase = DISC_HOLDINGS_LIST;
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == DISC_HOLDINGS_LIST) {
        if (tezos_list_handle_input(&st->holdings_list, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = DISC_SITE; *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            st->selected_holding = st->holdings_list.selected;
            st->image_attempted = false;
            fetch_and_decode_image(st, st->holdings[st->selected_holding].image_uri);
            st->phase = DISC_HOLDINGS_DETAIL;
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == DISC_HOLDINGS_DETAIL) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = DISC_HOLDINGS_LIST; *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L && st->image_ok && st->full_pixels) {
            st->phase = DISC_HOLDINGS_VIEWER;
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    /* DISC_HOLDINGS_VIEWER */
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = DISC_HOLDINGS_DETAIL; *needs_redraw = true; return TEZOS_ACTION_NONE; }
    int viewport_h = 480 - 34;
    int max_pan_x = st->full_w > 320 ? st->full_w - 320 : 0;
    int max_pan_y = st->full_h > viewport_h ? st->full_h - viewport_h : 0;
    if (ev.type == TEZOS_INPUT_UP) st->pan_y -= DISC_PAN_STEP;
    else if (ev.type == TEZOS_INPUT_DOWN) st->pan_y += DISC_PAN_STEP;
    else if (ev.type == TEZOS_INPUT_NUM && ev.value == 4) st->pan_x -= DISC_PAN_STEP;
    else if (ev.type == TEZOS_INPUT_NUM && ev.value == 6) st->pan_x += DISC_PAN_STEP;
    if (st->pan_x < 0) st->pan_x = 0;
    if (st->pan_y < 0) st->pan_y = 0;
    if (st->pan_x > max_pan_x) st->pan_x = max_pan_x;
    if (st->pan_y > max_pan_y) st->pan_y = max_pan_y;
    *needs_redraw = true;
    return TEZOS_ACTION_NONE;
}

tezos_screen_t *discover_entry_screen(void) {
    if (g_state.full_pixels) free(g_state.full_pixels);
    memset(&g_state, 0, sizeof g_state);
    g_state.phase = DISC_HOME;
    g_state.entrypoint_count = -1;
    refresh_bookmarks(&g_state);
    g_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_state, .debug_name = "discover",
    };
    return &g_screen;
}

tezos_screen_t *discover_test_inject_site(const char *address, const char *alias,
                                          const char *balance, const char *tx_count,
                                          int entrypoint_count,
                                          const char *holding_name, const char *holding_image_uri) {
    if (g_state.full_pixels) free(g_state.full_pixels);
    memset(&g_state, 0, sizeof g_state);
    strncpy(g_state.visited_address, address, sizeof g_state.visited_address - 1);
    g_state.site_fetch_attempted = true;
    g_state.site_fetch_ok = true;
    strncpy(g_state.site_info.alias, alias, sizeof g_state.site_info.alias - 1);
    strncpy(g_state.site_info.balance, balance, sizeof g_state.site_info.balance - 1);
    g_state.site_info.num_transactions = atoi(tx_count);
    g_state.entrypoint_count = entrypoint_count;

    if (holding_name) {
        g_state.holdings_fetch_attempted = true;
        g_state.holdings_fetch_ok = true;
        g_state.holdings_count = 1;
        strncpy(g_state.holdings[0].name, holding_name, sizeof g_state.holdings[0].name - 1);
        if (holding_image_uri) {
            strncpy(g_state.holdings[0].image_uri, holding_image_uri, sizeof g_state.holdings[0].image_uri - 1);
        }
        g_state.holdings_items[0] = (tezos_list_item_t){g_state.holdings_labels[0], NULL};
        snprintf(g_state.holdings_labels[0], sizeof g_state.holdings_labels[0], "%s", holding_name);
        tezos_list_init(&g_state.holdings_list, g_state.holdings_items, 1);
    }

    g_state.phase = DISC_SITE;
    g_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_state, .debug_name = "discover",
    };
    return &g_screen;
}
