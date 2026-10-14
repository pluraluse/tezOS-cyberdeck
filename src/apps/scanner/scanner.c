#include "scanner.h"
#include "../wallet/wallet.h"
#include "../settings/settings.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_popup.h"
#include "../../chain/tezos_forge.h"
#include "../../chain/tezos_micheline.h"
#include "../../chain/tezos_tzkt.h"
#include "../../chain/tezos_rpc.h"
#include <qrencode.h>
#include <zbar.h>
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* RPC node is now settings_get_rpc_node() -- a real, persisted setting,
   not a hardcoded placeholder (see wallet.c for the same change). */

typedef enum { SCAN_IDLE, SCAN_DECODED, SCAN_FAILED, SCAN_RESULT, SCAN_ENTRYPOINTS } scan_phase_t;

typedef struct {
    scan_phase_t phase;
    char destination[40];
    char entrypoint[32];
    char parameter_int[32];
    char error_msg[64];

    bool broadcast_attempted;
    bool broadcast_ok;
    bool self_verify_ok;
    char op_hash[80];
    char signed_hex_preview[80];

    /* real entrypoint discovery -- see tezos_tzkt.h's
       tezos_tzkt_get_contract_entrypoints() and its disclosed
       verification-gap note */
    char fetched_entrypoints[16][32];
    int fetched_entrypoint_count;
    bool entrypoints_fetch_attempted;
    bool entrypoints_fetch_ok;
} scanner_state_t;

static scanner_state_t g_state;
static tezos_screen_t g_scanner_screen;

/* Same rendering approach already verified in camera.c's simulate_scan
   -- render libqrencode's output to a grayscale bitmap, decode with
   real zbar. */
static unsigned char *render_qr_to_grayscale(QRcode *qr, int scale, int *out_w, int *out_h) {
    int quiet = 4 * scale;
    int img_size = qr->width * scale + quiet * 2;
    unsigned char *img = malloc((size_t)img_size * img_size);
    memset(img, 255, (size_t)img_size * img_size);
    for (int y = 0; y < qr->width; y++) {
        for (int x = 0; x < qr->width; x++) {
            unsigned char pixel = (qr->data[y * qr->width + x] & 0x1) ? 0 : 255;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    img[(quiet + y * scale + dy) * img_size + (quiet + x * scale + dx)] = pixel;
        }
    }
    *out_w = *out_h = img_size;
    return img;
}

/* Builds a realistic sample call-spec QR (standing in for what a
   dApp's website would show), real-encodes it, real-decodes it via
   zbar, and parses the resulting JSON -- the full real pipeline, same
   as camera.c's simulate_scan, just with Scanner's own simpler JSON
   schema instead of a Beacon pairing URI.

   CORRECTION, found by accident later in this project's history: the
   address below was picked as an arbitrary-looking placeholder and
   described (in earlier comments/docs, now fixed) as fictional --
   "not a claim this contract is real or deployed anywhere." That
   claim was wrong. KT1RJ6PbjHpwc3M5rw5s2Nbmefwbuwbdxton is the real,
   deployed Hic Et Nunc (HEN) NFT/FA2 token contract, confirmed via
   multiple independent sources (the platform's own official
   announcement, the Teia community's indexer documentation, and
   TzKT's own explorer). This was pure accident, not intentional
   reuse of a real address -- surfaced when checking whether Scanner
   could browse this exact contract's real entrypoints (see
   docs/build-notes/scanner-app-build-notes.md). The entrypoint name
   ("collect") and parameter shape below remain illustrative and are
   NOT confirmed to match this contract's actual real interface -- an
   FA2 token registry contract like this one more plausibly exposes
   entrypoints like "transfer"/"update_operators" per the FA2
   standard, not "collect" (which sounds more like a marketplace/swap
   action). Left as-is rather than swapped for a genuinely fictional
   address, since the address itself being real doesn't change
   anything about how this test data is used -- it's still just a
   string Scanner treats generically, not something that gets
   interpreted or trusted differently because it happens to be real. */
static bool simulate_scan(scanner_state_t *st) {
    const char *sample_json =
        "{\"destination\":\"KT1RJ6PbjHpwc3M5rw5s2Nbmefwbuwbdxton\","
        "\"entrypoint\":\"collect\",\"value\":{\"int\":\"4021\"}}";

    QRcode *qr = QRcode_encodeString(sample_json, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qr) { strcpy(st->error_msg, "QR encode failed"); return false; }

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
            cJSON *root = cJSON_Parse(decoded_text);
            if (root) {
                cJSON *dest = cJSON_GetObjectItemCaseSensitive(root, "destination");
                cJSON *entry = cJSON_GetObjectItemCaseSensitive(root, "entrypoint");
                cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
                cJSON *int_val = value ? cJSON_GetObjectItemCaseSensitive(value, "int") : NULL;
                if (cJSON_IsString(dest) && cJSON_IsString(entry) && cJSON_IsString(int_val)) {
                    snprintf(st->destination, sizeof st->destination, "%s", dest->valuestring);
                    snprintf(st->entrypoint, sizeof st->entrypoint, "%s", entry->valuestring);
                    snprintf(st->parameter_int, sizeof st->parameter_int, "%s", int_val->valuestring);
                    ok = true;
                } else {
                    strcpy(st->error_msg, "decoded QR but call spec is incomplete");
                }
                cJSON_Delete(root);
            } else {
                strcpy(st->error_msg, "decoded QR but JSON parse failed");
            }
        } else {
            strcpy(st->error_msg, "zbar found no symbol");
        }
    } else {
        strcpy(st->error_msg, "zbar scan found nothing");
    }

    zbar_image_destroy(zimg);
    zbar_image_scanner_destroy(scanner);
    QRcode_free(qr);
    free(img);
    return ok;
}

/* Real forge + sign + broadcast, exactly the pipeline verified
   throughout this project's chain layer -- entrypoint-call forging
   (tezos_micheline.h), the real account counter/branch (tezos_tzkt.h,
   same offline-fallback pattern as wallet.c), signing via Wallet's
   currently-active identity (wallet_sign_operation -- Scanner has no
   signer of its own), and real broadcast (tezos_rpc.h). */
static void perform_call(scanner_state_t *st) {
    char source[64];
    if (!wallet_get_active_address(source, sizeof source)) {
        strcpy(st->error_msg, "no active signer -- open Wallet first");
        st->phase = SCAN_FAILED;
        return;
    }

    tezos_transaction_params_t params = {0};
    strcpy(params.source, source);
    strcpy(params.fee, "500"); /* entrypoint calls typically cost a bit more gas than a plain transfer */
    strcpy(params.gas_limit, "2000");
    strcpy(params.storage_limit, "100");
    strcpy(params.amount, "0"); /* non-payable call -- the scanned spec doesn't include an amount field */
    snprintf(params.destination, sizeof params.destination, "%s", st->destination);
    params.entrypoint = st->entrypoint;

    tezos_micheline_node_t param_value = tezos_micheline_int(st->parameter_int);
    params.parameter_value = &param_value;

    char balance[32], real_counter[32];
    if (tezos_tzkt_get_account(settings_get_network(), source, balance, sizeof balance,
                               real_counter, sizeof real_counter)) {
        unsigned long long next_counter = strtoull(real_counter, NULL, 10) + 1;
        snprintf(params.counter, sizeof params.counter, "%llu", next_counter);
    } else {
        strcpy(params.counter, "1"); /* offline fallback, same as wallet.c */
    }
    char branch[128];
    const char *branch_to_use = tezos_tzkt_get_head_hash(settings_get_network(), branch, sizeof branch)
                               ? branch : "BLzyjjHKEKMULtvkpSHxuZxx6ei6fpntH2BTkYZiLgs8zLVstvX";

    tezos_bytes_t content; tezos_bytes_init(&content);
    if (!tezos_forge_transaction(&content, &params)) {
        /* real leak, same class as wallet.c's Send flow -- see that file's
           comment on tezos_forge_transaction's own partial-allocation-before-
           failure behavior */
        tezos_bytes_free(&content);
        strcpy(st->error_msg, "forge failed"); st->phase = SCAN_FAILED; return;
    }
    tezos_bytes_t full; tezos_bytes_init(&full);
    if (!tezos_forge_operation_group(&full, branch_to_use, content.data, content.len)) {
        strcpy(st->error_msg, "forge_operation_group failed");
        tezos_bytes_free(&content); st->phase = SCAN_FAILED; return;
    }
    tezos_bytes_free(&content);

    char sig[128];
    if (!wallet_sign_operation(&full, sig, sizeof sig)) {
        strcpy(st->error_msg, "sign failed"); tezos_bytes_free(&full); st->phase = SCAN_FAILED; return;
    }

    char hex[1024]; tezos_bytes_to_hex(&full, hex, sizeof hex);
    snprintf(st->signed_hex_preview, sizeof st->signed_hex_preview, "%.40s...", hex);
    st->broadcast_attempted = true;
    st->broadcast_ok = tezos_rpc_inject_operation(settings_get_rpc_node(), full.data, full.len, st->op_hash, sizeof st->op_hash);
    tezos_bytes_free(&full);
    st->phase = SCAN_RESULT;
}

static void render_idle_or_decoded(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    scanner_state_t *st = (scanner_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SCANNER", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    tezos_draw_rect(fb, d, 8, 40, fb->width - 16, 120, (tezos_color_t){0x06, 0x08, 0x18});
    tezos_draw_rect_outline(fb, d, 8, 40, fb->width - 16, 120, TEZOS_MUTED);
    const char *placeholder = "no real camera sensor here";
    int pw = tezos_text_width(placeholder, &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, (fb->width - pw) / 2, 95, placeholder, &TEZOS_FONT_SMALL, TEZOS_MUTED);

    int y = 176;
    if (st->phase == SCAN_IDLE) {
        tezos_draw_text(fb, d, 8, y, "Press SELECT to simulate", &TEZOS_FONT_SMALL, TEZOS_MUTED);
        tezos_draw_text(fb, d, 8, y + 18, "scanning a contract call QR", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else { /* SCAN_DECODED */
        tezos_draw_text(fb, d, 8, y, "CALL DECODED", &TEZOS_FONT_BODY, TEZOS_CYAN);
        y += 26;
        char line[64];
        snprintf(line, sizeof line, "To: %.20s...", st->destination);
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
        snprintf(line, sizeof line, "Entrypoint: %s", st->entrypoint);
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
        snprintf(line, sizeof line, "Param (int): %s", st->parameter_int);
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 26;
        tezos_draw_text(fb, d, 8, y, "UP: verify against the contract's", &TEZOS_FONT_SMALL, TEZOS_MUTED);
        y += 16;
        tezos_draw_text(fb, d, 8, y, "real entrypoints (via TZKT)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, st->phase == SCAN_IDLE ? "SELECT" : "SIGN & SEND", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_entrypoints(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    scanner_state_t *st = (scanner_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "CONTRACT ENTRYPOINTS", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    int y = 44;
    if (!st->entrypoints_fetch_attempted || !st->entrypoints_fetch_ok) {
        tezos_draw_text(fb, d, 8, y, "Couldn't reach TZKT (offline,", &TEZOS_FONT_SMALL, TEZOS_AMBER);
        y += 18;
        tezos_draw_text(fb, d, 8, y, "or this device has no network", &TEZOS_FONT_SMALL, TEZOS_AMBER);
        y += 18;
        tezos_draw_text(fb, d, 8, y, "configured yet).", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    } else if (st->fetched_entrypoint_count == 0) {
        tezos_draw_text(fb, d, 8, y, "No entrypoints found for this", &TEZOS_FONT_SMALL, TEZOS_MUTED);
        y += 18;
        tezos_draw_text(fb, d, 8, y, "contract address.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        bool claimed_entrypoint_matches = false;
        for (int i = 0; i < st->fetched_entrypoint_count; i++) {
            bool is_claimed = strcmp(st->fetched_entrypoints[i], st->entrypoint) == 0;
            if (is_claimed) claimed_entrypoint_matches = true;
            tezos_draw_text(fb, d, 8, y, st->fetched_entrypoints[i], &TEZOS_FONT_BODY,
                            is_claimed ? TEZOS_CYAN : TEZOS_TEXT);
            y += 20;
        }
        y += 10;
        if (claimed_entrypoint_matches) {
            tezos_draw_text(fb, d, 8, y, "The scanned entrypoint IS real.", &TEZOS_FONT_SMALL, TEZOS_CYAN);
        } else {
            tezos_draw_text(fb, d, 8, y, "WARNING: scanned entrypoint NOT", &TEZOS_FONT_SMALL, TEZOS_PINK);
            y += 18;
            tezos_draw_text(fb, d, 8, y, "found in this contract's real list.", &TEZOS_FONT_SMALL, TEZOS_PINK);
        }
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_failed(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    scanner_state_t *st = (scanner_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SCANNER", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "FAILED", &TEZOS_FONT_BODY, TEZOS_PINK);
    tezos_draw_text(fb, d, 8, 76, st->error_msg, &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_result(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    scanner_state_t *st = (scanner_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SCANNER", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    tezos_draw_text(fb, d, 8, 44, "SIGNED", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    tezos_draw_text(fb, d, 8, 62, st->signed_hex_preview, &TEZOS_FONT_SMALL, TEZOS_TEXT);

    int y = 90;
    if (!st->broadcast_attempted) {
        tezos_draw_text(fb, d, 8, y, "(not broadcast)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else if (st->broadcast_ok) {
        tezos_draw_text(fb, d, 8, y, "BROADCAST OK", &TEZOS_FONT_SMALL, TEZOS_CYAN);
        y += 18;
        char oph[64]; snprintf(oph, sizeof oph, "%.36s...", st->op_hash);
        tezos_draw_text(fb, d, 8, y, oph, &TEZOS_FONT_SMALL, TEZOS_TEXT);
    } else {
        tezos_draw_text(fb, d, 8, y, "broadcast failed (offline or", &TEZOS_FONT_SMALL, TEZOS_AMBER);
        y += 18;
        tezos_draw_text(fb, d, 8, y, "node rejected it)", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("DONE", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "DONE", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void screen_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    scanner_state_t *st = (scanner_state_t *)self->app_state;
    switch (st->phase) {
        case SCAN_IDLE: case SCAN_DECODED: render_idle_or_decoded(self, fb, d); break;
        case SCAN_FAILED: render_failed(self, fb, d); break;
        case SCAN_RESULT: render_result(self, fb, d); break;
        case SCAN_ENTRYPOINTS: render_entrypoints(self, fb, d); break;
    }
}

static tezos_popup_result_t g_confirm_result = TEZOS_POPUP_PENDING;

static tezos_action_t screen_input(tezos_screen_t *self, tezos_input_event_t ev,
                                   tezos_screen_t **next, bool *needs_redraw) {
    scanner_state_t *st = (scanner_state_t *)self->app_state;

    if (st->phase == SCAN_ENTRYPOINTS) {
        /* BACK here means "return to the decoded call", not "leave
           Scanner entirely" -- an in-screen phase transition, not a
           navigation pop. Must be checked BEFORE the general
           SOFTKEY_R->POP handling below. */
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) {
            st->phase = SCAN_DECODED;
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;

    if (ev.type == TEZOS_INPUT_UP && st->phase == SCAN_DECODED) {
        /* Real verification step: fetch the contract's ACTUAL
           entrypoints via TZKT and cross-check the scanned QR's
           claimed entrypoint against them, rather than blindly trusting
           what the QR said. See tezos_tzkt.h's disclosed verification-
           gap note on this specific endpoint's response shape. */
        st->entrypoints_fetch_attempted = true;
        st->entrypoints_fetch_ok = tezos_tzkt_get_contract_entrypoints(
            settings_get_network(), st->destination,
            st->fetched_entrypoints, 16, &st->fetched_entrypoint_count);
        st->phase = SCAN_ENTRYPOINTS;
        *needs_redraw = true;
        return TEZOS_ACTION_NONE;
    }

    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        if (st->phase == SCAN_IDLE) {
            bool ok = simulate_scan(st);
            st->phase = ok ? SCAN_DECODED : SCAN_FAILED;
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        if (st->phase == SCAN_DECODED) {
            /* real confirmation before an irreversible sign+broadcast --
               same popup-gating pattern as Wallet's Send flow */
            char confirm_msg[128];
            snprintf(confirm_msg, sizeof confirm_msg, "Call %s on %.16s...? This cannot be undone.",
                    st->entrypoint, st->destination);
            *next = tezos_popup_confirm_get("Confirm Call", confirm_msg, "SIGN & SEND", "CANCEL", &g_confirm_result);
            return TEZOS_ACTION_PUSH;
        }
    }
    return TEZOS_ACTION_NONE;
}

static void screen_on_resume(tezos_screen_t *self) {
    scanner_state_t *st = (scanner_state_t *)self->app_state;
    if (st->phase == SCAN_DECODED && g_confirm_result == TEZOS_POPUP_CONFIRMED) {
        perform_call(st);
    }
    g_confirm_result = TEZOS_POPUP_PENDING;
}

tezos_screen_t *scanner_entry_screen(void) {
    memset(&g_state, 0, sizeof g_state);
    g_state.phase = SCAN_IDLE;
    g_scanner_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = screen_on_resume, .on_tick = NULL,
        .app_state = &g_state, .debug_name = "scanner",
    };
    return &g_scanner_screen;
}
