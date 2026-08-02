#include "wallet.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../chain/tezos_forge.h"
#include "../../chain/tezos_signer.h"
#include "../../chain/tezos_base58.h"
#include <stdio.h>
#include <string.h>

/*
 * *** DEV/TEST IDENTITY — NOT A REAL WALLET, DO NOT FUND THIS ADDRESS ***
 * Taquito's own public test vector, reused deliberately so it's
 * unambiguous this is a known, public test key. Stands in for the real
 * signer hardware (STM32/Pico) that doesn't exist yet — see
 * docs/build-notes/wallet-app-build-notes.md.
 */
static const char *DEV_TEST_SEED = "edsk4TjJWEszkHKono7XMnepVqwi37FrpbVt1KCsifJeAGimxheShG";
static tezos_mock_signer_t g_wallet_signer;
static bool g_signer_ready = false;

static void ensure_signer(void) {
    if (!g_signer_ready) {
        tezos_mock_signer_init(&g_wallet_signer, DEV_TEST_SEED);
        g_signer_ready = true;
    }
}

/* ---------- Shared "operation result" display, used by both the Send
   confirm screen and the Staking page's real delegate action ---------- */
typedef struct {
    bool done;
    bool self_verify_ok;
    char signed_hex_preview[80];
    char error[64];
} op_result_t;

static void draw_op_result(tezos_fb_t *fb, tezos_dirty_t *d, int y, const op_result_t *r) {
    if (!r->done && r->error[0] == '\0') return;
    if (r->error[0] != '\0') {
        tezos_draw_text(fb, d, 8, y, "ERROR:", &TEZOS_FONT_SMALL, TEZOS_PINK);
        tezos_draw_text(fb, d, 8, y + 18, r->error, &TEZOS_FONT_SMALL, TEZOS_PINK);
        return;
    }
    tezos_draw_text(fb, d, 8, y, "SIGNED", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    tezos_draw_text(fb, d, 8, y + 18, r->signed_hex_preview, &TEZOS_FONT_SMALL, TEZOS_TEXT);
    tezos_draw_text(fb, d, 8, y + 36, r->self_verify_ok ? "self-verify: OK" : "self-verify: FAILED",
                    &TEZOS_FONT_SMALL, r->self_verify_ok ? TEZOS_CYAN : TEZOS_PINK);
    tezos_draw_text(fb, d, 8, y + 54, "(not broadcast - no network layer yet)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
}

/* ================= Send confirm screen (pushed from Balances page) ================= */

static op_result_t g_send_result;
static const char *DEV_RECIPIENT = "tz1gvF4cD2dDtqitL3ZTraggSR1Mju2BKFEM";
static const char *DEV_AMOUNT_MUTEZ = "1000000"; /* 1 tez */
static const char *DEV_PLACEHOLDER_BRANCH = "BLzyjjHKEKMULtvkpSHxuZxx6ei6fpntH2BTkYZiLgs8zLVstvX";

static void perform_send(void) {
    memset(&g_send_result, 0, sizeof g_send_result);
    char source[64];
    tezos_mock_signer_address_b58(&g_wallet_signer, source, sizeof source);

    tezos_transaction_params_t params;
    strcpy(params.source, source);
    strcpy(params.fee, "374");
    strcpy(params.counter, "1"); /* placeholder — needs real account counter from a node */
    strcpy(params.gas_limit, "1000");
    strcpy(params.storage_limit, "0");
    strcpy(params.amount, DEV_AMOUNT_MUTEZ);
    strcpy(params.destination, DEV_RECIPIENT);

    tezos_bytes_t content; tezos_bytes_init(&content);
    if (!tezos_forge_transaction(&content, &params)) { strcpy(g_send_result.error, "forge failed"); return; }
    tezos_bytes_t full; tezos_bytes_init(&full);
    if (!tezos_forge_operation_group(&full, DEV_PLACEHOLDER_BRANCH, content.data, content.len)) {
        strcpy(g_send_result.error, "forge_operation_group failed"); tezos_bytes_free(&content); return;
    }
    tezos_bytes_free(&content);

    char sig_b58[TEZOS_B58_MAX_STRLEN];
    if (!tezos_mock_signer_sign(&g_wallet_signer, &full, sig_b58, sizeof sig_b58)) {
        strcpy(g_send_result.error, "sign failed"); tezos_bytes_free(&full); return;
    }
    g_send_result.self_verify_ok = tezos_mock_signer_self_verify(&g_wallet_signer, full.data, full.len);
    char hex[1024]; tezos_bytes_to_hex(&full, hex, sizeof hex);
    snprintf(g_send_result.signed_hex_preview, sizeof g_send_result.signed_hex_preview, "%.40s...", hex);
    g_send_result.done = true;
    tezos_bytes_free(&full);
}

static void send_confirm_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    (void)self;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SEND", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 44, "To:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 62, DEV_RECIPIENT, &TEZOS_FONT_SMALL, TEZOS_TEXT);
    tezos_draw_text(fb, d, 8, 90, "Amount:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 108, "1.000000 tez", &TEZOS_FONT_BODY, TEZOS_TEXT);
    if (!g_send_result.done && g_send_result.error[0] == '\0') {
        tezos_draw_text(fb, d, 8, 160, "Press SIGN to forge + sign", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        draw_op_result(fb, d, 160, &g_send_result);
    }
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SIGN", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t send_confirm_input(tezos_screen_t *self, tezos_input_event_t ev,
                                         tezos_screen_t **next, bool *needs_redraw) {
    (void)self; (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) { perform_send(); *needs_redraw = true; }
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t g_send_confirm_screen;
static tezos_screen_t *wallet_send_confirm_screen(void) {
    memset(&g_send_result, 0, sizeof g_send_result);
    g_send_confirm_screen = (tezos_screen_t){
        .render = send_confirm_render, .handle_input = send_confirm_input,
        .on_resume = NULL, .app_state = NULL, .debug_name = "wallet_send_confirm",
    };
    return &g_send_confirm_screen;
}

/* ================= Paged Wallet home: Balances / Transactions /
   Contacts / Domain / Staking-Delegating / TZKT Info ================= */

typedef enum {
    PAGE_BALANCES, PAGE_TRANSACTIONS, PAGE_CONTACTS,
    PAGE_DOMAIN, PAGE_STAKING, PAGE_TZKT_INFO, PAGE_COUNT
} wallet_page_t;

static const char *PAGE_NAMES[PAGE_COUNT] = {
    "BALANCES", "TRANSACTIONS", "CONTACTS", "DOMAIN", "STAKING / DELEGATING", "TZKT INFO"
};

/* Mock content — real Transactions/Contacts/Domain/TZKT-Info data all
   need the network layer, which doesn't exist yet (see
   docs/build-notes/wallet-app-build-notes.md). Staking's delegate
   action is genuinely real, same forge/sign pipeline as Send. */
static const tezos_list_item_t g_tx_items[] = {
    {"Sent 1.0 tez", "to alice.tez"},
    {"Received 5.0 tez", "from bob.tez"},
    {"Delegated", "to baker.tz"},
    {"Sent 0.25 tez", "to tz1boK...9pQ2"},
};
static const tezos_list_item_t g_contact_items[] = {
    {"alice.tez", NULL},
    {"bob.tez", NULL},
    {"baker.tz", NULL},
};

typedef struct {
    tezos_paged_view_state_t pager;
    bool in_sublist; /* true = Up/Down scroll the current page's list instead of flipping pages */
    tezos_list_state_t tx_list;
    tezos_list_state_t contacts_list;
    op_result_t delegate_result;
    char address[64];
} wallet_pages_state_t;

static wallet_pages_state_t g_pages_state;

static void perform_delegate(void) {
    op_result_t *r = &g_pages_state.delegate_result;
    memset(r, 0, sizeof *r);
    char source[64];
    tezos_mock_signer_address_b58(&g_wallet_signer, source, sizeof source);

    tezos_delegation_params_t params;
    strcpy(params.source, source);
    strcpy(params.fee, "374");
    strcpy(params.counter, "2"); /* placeholder — needs real account counter from a node */
    strcpy(params.gas_limit, "1000");
    strcpy(params.storage_limit, "0");
    strcpy(params.delegate, DEV_RECIPIENT); /* stand-in "baker" address — no real baker registry yet */

    tezos_bytes_t content; tezos_bytes_init(&content);
    if (!tezos_forge_delegation(&content, &params)) { strcpy(r->error, "forge_delegation failed"); return; }
    tezos_bytes_t full; tezos_bytes_init(&full);
    if (!tezos_forge_operation_group(&full, DEV_PLACEHOLDER_BRANCH, content.data, content.len)) {
        strcpy(r->error, "forge_operation_group failed"); tezos_bytes_free(&content); return;
    }
    tezos_bytes_free(&content);

    char sig_b58[TEZOS_B58_MAX_STRLEN];
    if (!tezos_mock_signer_sign(&g_wallet_signer, &full, sig_b58, sizeof sig_b58)) {
        strcpy(r->error, "sign failed"); tezos_bytes_free(&full); return;
    }
    r->self_verify_ok = tezos_mock_signer_self_verify(&g_wallet_signer, full.data, full.len);
    char hex[1024]; tezos_bytes_to_hex(&full, hex, sizeof hex);
    snprintf(r->signed_hex_preview, sizeof r->signed_hex_preview, "%.40s...", hex);
    r->done = true;
    tezos_bytes_free(&full);
}

static void draw_page_chrome(tezos_fb_t *fb, tezos_dirty_t *d, wallet_pages_state_t *st) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, PAGE_NAMES[st->pager.current_page], &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    int pw = tezos_text_width("9/9", &TEZOS_FONT_SMALL);
    tezos_paged_view_draw_indicator(fb, d, fb->width - pw - 8, 8, &TEZOS_FONT_SMALL, &st->pager);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
}

static void draw_page_softkeys(tezos_fb_t *fb, tezos_dirty_t *d, const char *left, const char *right) {
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, left, &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int rw = tezos_text_width(right, &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - rw - 8, fb->height - 26, right, &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void wallet_pages_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    wallet_pages_state_t *st = (wallet_pages_state_t *)self->app_state;
    draw_page_chrome(fb, d, st);

    switch (st->pager.current_page) {
        case PAGE_BALANCES:
            tezos_draw_text(fb, d, 8, 44, "BALANCE (mock - no network layer yet)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, 64, "1,204.50 tez", &TEZOS_FONT_HERO, TEZOS_TEXT);
            tezos_draw_text(fb, d, 8, 110, "YOUR ADDRESS (real, from mock signer)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, 128, st->address, &TEZOS_FONT_SMALL, TEZOS_CYAN);
            tezos_draw_text(fb, d, 8, 160, "Press SELECT to Send", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            draw_page_softkeys(fb, d, "SEND", "MENU");
            break;

        case PAGE_TRANSACTIONS:
            tezos_draw_text(fb, d, 8, 44, "recent (mock - no network layer yet)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_list_render(fb, d, 8, 66, fb->width - 16, fb->height - 106, &TEZOS_FONT_BODY, &st->tx_list);
            draw_page_softkeys(fb, d, st->in_sublist ? "VIEW" : "SELECT", "MENU");
            break;

        case PAGE_CONTACTS:
            tezos_draw_text(fb, d, 8, 44, "saved addresses (local, real)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_list_render(fb, d, 8, 66, fb->width - 16, fb->height - 106, &TEZOS_FONT_BODY, &st->contacts_list);
            draw_page_softkeys(fb, d, st->in_sublist ? "SEND" : "SELECT", "MENU");
            break;

        case PAGE_DOMAIN:
            tezos_draw_text(fb, d, 8, 44, "your .tez domain", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, 66, "none registered", &TEZOS_FONT_BODY, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, 110, "Registration needs Scanner + a network", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, 128, "layer, neither exist yet.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            draw_page_softkeys(fb, d, "-", "MENU");
            break;

        case PAGE_STAKING:
            tezos_draw_text(fb, d, 8, 44, "current delegate (mock)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, 64, "not delegated", &TEZOS_FONT_BODY, TEZOS_MUTED);
            if (!st->delegate_result.done && st->delegate_result.error[0] == '\0') {
                tezos_draw_text(fb, d, 8, 110, "Press SELECT to delegate", &TEZOS_FONT_SMALL, TEZOS_MUTED);
                tezos_draw_text(fb, d, 8, 128, "(real crypto, mock dev signer)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            } else {
                draw_op_result(fb, d, 110, &st->delegate_result);
            }
            draw_page_softkeys(fb, d, "DELEGATE", "MENU");
            break;

        case PAGE_TZKT_INFO:
            tezos_draw_text(fb, d, 8, 44, "account info (mock - no network layer yet)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, 68, "balance:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 120, 68, "1204500000", &TEZOS_FONT_SMALL, TEZOS_TEXT);
            tezos_draw_text(fb, d, 8, 88, "counter:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 120, 88, "0", &TEZOS_FONT_SMALL, TEZOS_TEXT);
            tezos_draw_text(fb, d, 8, 108, "revealed:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 120, 108, "false", &TEZOS_FONT_SMALL, TEZOS_TEXT);
            tezos_draw_text(fb, d, 8, 128, "delegate:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 120, 128, "null", &TEZOS_FONT_SMALL, TEZOS_TEXT);
            draw_page_softkeys(fb, d, "-", "MENU");
            break;

        default: break;
    }
}

static tezos_action_t wallet_pages_input(tezos_screen_t *self, tezos_input_event_t ev,
                                         tezos_screen_t **next, bool *needs_redraw) {
    wallet_pages_state_t *st = (wallet_pages_state_t *)self->app_state;
    wallet_page_t page = st->pager.current_page;

    /* Sub-list mode: Transactions/Contacts pages let SELECT "enter" the
       list, at which point Up/Down scroll the list instead of flipping
       pages, until BACK exits back to paging mode. */
    if (st->in_sublist) {
        tezos_list_state_t *list = (page == PAGE_TRANSACTIONS) ? &st->tx_list : &st->contacts_list;
        if (tezos_list_handle_input(list, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->in_sublist = false; *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L && page == PAGE_CONTACTS) {
            *next = wallet_send_confirm_screen(); /* "send to this contact" — recipient still
                hardcoded for now, same T9-widget gap as Balances' Send (see build notes) */
            return TEZOS_ACTION_PUSH;
        }
        return TEZOS_ACTION_NONE;
    }

    if (tezos_paged_view_handle_input(&st->pager, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP; /* MENU -> back to idle */
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        switch (page) {
            case PAGE_BALANCES:
                *next = wallet_send_confirm_screen();
                return TEZOS_ACTION_PUSH;
            case PAGE_TRANSACTIONS:
            case PAGE_CONTACTS:
                st->in_sublist = true;
                *needs_redraw = true;
                return TEZOS_ACTION_NONE;
            case PAGE_STAKING:
                perform_delegate();
                *needs_redraw = true;
                return TEZOS_ACTION_NONE;
            default:
                return TEZOS_ACTION_NONE; /* Domain/TZKT Info: no action yet, placeholder pages */
        }
    }
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t g_wallet_pages_screen = {
    .render = wallet_pages_render, .handle_input = wallet_pages_input,
    .on_resume = NULL, .app_state = &g_pages_state, .debug_name = "wallet_pages",
};

tezos_screen_t *wallet_entry_screen(void) {
    ensure_signer();
    memset(&g_pages_state, 0, sizeof g_pages_state);
    tezos_mock_signer_address_b58(&g_wallet_signer, g_pages_state.address, sizeof g_pages_state.address);
    tezos_paged_view_init(&g_pages_state.pager, PAGE_COUNT);
    tezos_list_init(&g_pages_state.tx_list, g_tx_items, 4);
    tezos_list_init(&g_pages_state.contacts_list, g_contact_items, 3);
    return &g_wallet_pages_screen;
}
