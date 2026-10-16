#include "wallet.h"
#include "../../core/tezos_popup.h"
#include "../../core/tezos_pin_entry.h"
#include "../settings/settings.h"
#include "../../chain/tezos_tzkt.h"
#include "../../chain/tezos_tez_domain.h"
#include "../../chain/tezos_rpc.h"
#include "../../chain/tezos_http.h"
#include "../../chain/tezos_store.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../core/tezos_t9.h"
#include "../../core/tezos_address_book.h"
#include "../../chain/tezos_forge.h"
#include "../../chain/tezos_signer.h"
#include "../../chain/tezos_base58.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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

/* Remembered for the CURRENT session only, once real seed material is
   available (after import or PIN-unlock) -- needed to re-derive a
   different account index on demand for account switching, without
   requiring another PIN entry mid-session. This is the same "signer
   holds real seed material in Linux-side memory" tradeoff already
   documented for the software mock signer generally
   (docs/architecture/SIGNER-PROTOCOL.md) -- not a new exposure this
   feature introduces, the same one that already exists. Cleared to
   zero whenever no real signer is active (dev test seed case). */
static char g_session_mnemonic[TEZOS_STORE_MAX_MNEMONIC_LEN];
static char g_session_passphrase[TEZOS_STORE_MAX_PASSPHRASE_LEN];
static int g_session_word_count;
static bool g_session_mnemonic_available = false;

static void ensure_signer(void) {
    if (!g_signer_ready) {
        tezos_mock_signer_init(&g_wallet_signer, DEV_TEST_SEED);
        g_signer_ready = true;
    }
}

void wallet_set_active_signer(const tezos_mock_signer_t *signer) {
    g_wallet_signer = *signer;
    g_signer_ready = true; /* prevents ensure_signer() from overwriting this
        with the dev test seed the next time a screen calls it */
}

bool wallet_get_active_address(char *out, size_t cap) {
    if (!g_signer_ready) return false;
    return tezos_mock_signer_address_b58(&g_wallet_signer, out, cap);
}

/* The real public key (edpk...), not the address (tz1...) -- these are
   not interchangeable. Added because beacon_wallet.c's permission
   response needs the real public key and was wrongly substituting
   wallet_derive_account_address()'s result (an address) for it,
   address-shaped data standing in for pubkey-shaped data. See
   docs/build-notes/beacon-wallet-app-build-notes.md. */
bool wallet_get_active_public_key(char *out, size_t cap) {
    if (!g_signer_ready) return false;
    return tezos_mock_signer_public_key_b58(&g_wallet_signer, out, cap);
}

bool wallet_sign_operation(tezos_bytes_t *forged_bytes, char *sig_out, size_t sig_cap) {
    if (!g_signer_ready) return false;
    return tezos_mock_signer_sign(&g_wallet_signer, forged_bytes, sig_out, sig_cap);
}

bool wallet_set_active_signer_from_mnemonic(const char *mnemonic, int word_count,
                                            const char *passphrase, uint32_t account) {
    tezos_mock_signer_t signer;
    if (!tezos_mock_signer_from_mnemonic(&signer, mnemonic, word_count, passphrase, account)) {
        return false;
    }
    g_wallet_signer = signer;
    g_signer_ready = true;

    strncpy(g_session_mnemonic, mnemonic, sizeof g_session_mnemonic - 1);
    g_session_mnemonic[sizeof g_session_mnemonic - 1] = '\0';
    strncpy(g_session_passphrase, passphrase ? passphrase : "", sizeof g_session_passphrase - 1);
    g_session_passphrase[sizeof g_session_passphrase - 1] = '\0';
    g_session_word_count = word_count;
    g_session_mnemonic_available = true;
    return true;
}

bool wallet_switch_account(uint32_t account) {
    if (!g_session_mnemonic_available) return false;
    tezos_mock_signer_t signer;
    if (!tezos_mock_signer_from_mnemonic(&signer, g_session_mnemonic, g_session_word_count,
                                         g_session_passphrase, account)) {
        return false;
    }
    g_wallet_signer = signer;
    g_signer_ready = true;
    return true;
}

bool wallet_has_session_mnemonic(void) {
    return g_session_mnemonic_available;
}

bool wallet_derive_account_address(uint32_t account, char *out, size_t cap) {
    if (!g_session_mnemonic_available) return false;
    tezos_mock_signer_t signer; /* local, doesn't touch g_wallet_signer -- read-only preview */
    if (!tezos_mock_signer_from_mnemonic(&signer, g_session_mnemonic, g_session_word_count,
                                         g_session_passphrase, account)) {
        return false;
    }
    return tezos_mock_signer_address_b58(&signer, out, cap);
}

/* ---------- Shared "operation result" display, used by both the Send
   confirm screen and the Staking page's real delegate action ---------- */
typedef struct {
    bool done;
    bool self_verify_ok;
    char signed_hex_preview[80];
    char error[64];
    bool broadcast_attempted;
    bool broadcast_ok;
    char op_hash[80];
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
    if (!r->broadcast_attempted) {
        tezos_draw_text(fb, d, 8, y + 54, "(not broadcast -- no RPC node configured)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else if (r->broadcast_ok) {
        tezos_draw_text(fb, d, 8, y + 54, "BROADCAST OK", &TEZOS_FONT_SMALL, TEZOS_CYAN);
        char oph[80]; snprintf(oph, sizeof oph, "%.36s...", r->op_hash);
        tezos_draw_text(fb, d, 8, y + 72, oph, &TEZOS_FONT_SMALL, TEZOS_TEXT);
    } else {
        tezos_draw_text(fb, d, 8, y + 54, "broadcast failed (offline or", &TEZOS_FONT_SMALL, TEZOS_AMBER);
        tezos_draw_text(fb, d, 8, y + 72, "node rejected it)", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    }
}

/* ================= Send confirm screen (pushed from Balances page) ================= */

static op_result_t g_send_result;
static char g_send_recipient[64] = "";  /* filled by wallet_send_recipient_screen, real T9 entry --
    was a hardcoded DEV_RECIPIENT constant until now */
static char g_send_amount_mutez[24] = ""; /* filled by wallet_send_amount_screen (converted from
    a real tez-decimal T9 entry) -- was a hardcoded DEV_AMOUNT_MUTEZ constant until now */
static char g_send_amount_display[24] = ""; /* the tez-decimal string as entered, for display */
static const char *DEV_PLACEHOLDER_BRANCH = "BLzyjjHKEKMULtvkpSHxuZxx6ei6fpntH2BTkYZiLgs8zLVstvX";
static const char *DEV_PLACEHOLDER_BAKER = "tz1gvF4cD2dDtqitL3ZTraggSR1Mju2BKFEM"; /* real, separate
    gap from Send's recipient -- Delegate has no real baker-selection UI yet, not touched
    by this pass, which is specifically about Send's recipient/amount */
static tezos_popup_result_t g_send_popup_result = TEZOS_POPUP_PENDING;

/* Converts a real, user-typed tez-decimal string ("1.5", "0.25", "10")
   into a mutez integer string ("1500000", "250000", "10000000") --
   tez's real precision is exactly 6 decimal places, matching mutez
   being 10^-6 tez. Fails cleanly (returns false) rather than silently
   rounding or truncating on: more than one decimal point, more than 6
   fractional digits (real precision loss, not a rounding convenience),
   non-digit characters, or an empty string -- a bad amount should stop
   the send flow here, not become a wrong on-chain amount. */
bool wallet_tez_to_mutez(const char *tez_str, char *mutez_out, size_t mutez_cap) {
    if (tez_str[0] == '\0') return false;
    const char *dot = strchr(tez_str, '.');
    char int_part[16] = "0", frac_part[8] = "000000";
    if (dot) {
        size_t int_len = (size_t)(dot - tez_str);
        size_t frac_len = strlen(dot + 1);
        if (int_len == 0 || int_len >= sizeof int_part) return false;
        if (frac_len > 6) return false; /* more precision than tez actually has -- real
            rejection, not silent truncation of the user's intended amount */
        strncpy(int_part, tez_str, int_len);
        int_part[int_len] = '\0';
        /* GCC's -Wstringop-truncation is a known false positive here at
           -O2 (same class already documented and suppressed in
           tezos_popup.c's safe_copy() and explorer.c's entrypoint-label
           formatting) -- frac_len is already validated <= 6 above
           (frac_part is 8 bytes, comfortable real headroom), but GCC's
           static analysis can't connect that earlier bounds check to
           this specific strncpy call. Real bound, not a real risk. */
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstringop-truncation"
        strncpy(frac_part, dot + 1, frac_len);
        #pragma GCC diagnostic pop
        for (size_t i = frac_len; i < 6; i++) frac_part[i] = '0'; /* right-pad to 6 digits */
        frac_part[6] = '\0';
    } else {
        if (strlen(tez_str) >= sizeof int_part) return false;
        strcpy(int_part, tez_str);
    }
    for (const char *p = int_part; *p; p++) if (!isdigit((unsigned char)*p)) return false;
    for (const char *p = frac_part; *p; p++) if (!isdigit((unsigned char)*p)) return false;

    unsigned long long int_val = strtoull(int_part, NULL, 10);
    unsigned long long frac_val = strtoull(frac_part, NULL, 10);
    unsigned long long total_mutez = int_val * 1000000ULL + frac_val;
    snprintf(mutez_out, mutez_cap, "%llu", total_mutez);
    return true;
}

/* RPC node is now a real, persisted, user-editable Settings field
   (settings_get_rpc_node()) instead of a hardcoded placeholder --
   Settings' RPC Node row is real now (was still a stub when this
   comment was first written). Both Wallet and Scanner read the same
   one setting instead of each hardcoding their own copy of the value. */

static void perform_send(void) {
    memset(&g_send_result, 0, sizeof g_send_result);
    char source[64];
    tezos_mock_signer_address_b58(&g_wallet_signer, source, sizeof source);

    tezos_transaction_params_t params = {0}; /* zero-init matters now: entrypoint/
        parameter_value must default to NULL (plain transfer), not leftover stack
        garbage -- this was a real bug, caught by a segfault, not a hypothetical */
    strcpy(params.source, source);
    strcpy(params.fee, "374");

    /* Real counter fetch, falling back to the old placeholder if the
       network isn't reachable -- this device may genuinely be offline
       (no WiFi configured yet, no signal), so this can't be an
       assumed-available dependency. */
    char balance[32], real_counter[32];
    if (tezos_tzkt_get_account(settings_get_network(), source, balance, sizeof balance,
                               real_counter, sizeof real_counter)) {
        unsigned long long next_counter = strtoull(real_counter, NULL, 10) + 1;
        snprintf(params.counter, sizeof params.counter, "%llu", next_counter);
    } else {
        strcpy(params.counter, "1"); /* offline fallback -- would be rejected by a
            real node (wrong counter), but keeps the forge/sign pipeline itself
            testable without live network, same as everywhere else in this project */
    }

    /* Real branch (current head hash) fetch, same offline-fallback
       pattern as the counter above -- tezos_tzkt_get_head_hash existed
       and was verified, but nothing actually called it until now. */
    char branch[128];
    bool have_real_branch = tezos_tzkt_get_head_hash(settings_get_network(), branch, sizeof branch);
    const char *branch_to_use = have_real_branch ? branch : DEV_PLACEHOLDER_BRANCH;

    strcpy(params.gas_limit, "1000");
    strcpy(params.storage_limit, "0");
    strcpy(params.amount, g_send_amount_mutez);
    strcpy(params.destination, g_send_recipient);

    tezos_bytes_t content; tezos_bytes_init(&content);
    if (!tezos_forge_transaction(&content, &params)) {
        /* real leak fixed here: tezos_forge_transaction appends bytes at
           each step before checking the next (tag, then manager fields,
           then amount, then destination) -- a failure partway through
           (e.g. an invalid destination address, exactly what a real
           malformed recipient produces) still leaves real allocated
           memory in `content` that this caller owns and must free,
           same as the operation_group-failure path below already does.
           Caught by LeakSanitizer while testing Send's new real
           recipient/amount entry -- this path was unreachable with the
           old hardcoded, always-valid DEV_RECIPIENT constant, which is
           exactly why a real gap like this can hide until real input
           actually exercises it. */
        tezos_bytes_free(&content);
        strcpy(g_send_result.error, "forge failed"); return;
    }
    tezos_bytes_t full; tezos_bytes_init(&full);
    if (!tezos_forge_operation_group(&full, branch_to_use, content.data, content.len)) {
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

    /* Real broadcast attempt -- only reached after a self-verified real
       signature exists. Failure here (offline, node down, node
       rejects the placeholder branch/counter) doesn't unwind the
       SIGNED state above; the operation was still genuinely forged and
       signed correctly regardless of whether it made it onto the
       chain. */
    g_send_result.broadcast_attempted = true;
    g_send_result.broadcast_ok = tezos_rpc_inject_operation(settings_get_rpc_node(), full.data, full.len,
                                                            g_send_result.op_hash, sizeof g_send_result.op_hash);
    tezos_bytes_free(&full);
}

static void send_confirm_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    (void)self;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SEND", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 44, "To:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 62, g_send_recipient, &TEZOS_FONT_SMALL, TEZOS_TEXT);
    tezos_draw_text(fb, d, 8, 90, "Amount:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    char amount_line[40];
    snprintf(amount_line, sizeof amount_line, "%s tez", g_send_amount_display);
    tezos_draw_text(fb, d, 8, 108, amount_line, &TEZOS_FONT_BODY, TEZOS_TEXT);
    if (!g_send_result.done && g_send_result.error[0] == '\0') {
        tezos_draw_text(fb, d, 8, 160, "Press SIGN to review & confirm", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        draw_op_result(fb, d, 160, &g_send_result);
    }
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SIGN", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void send_confirm_on_resume(tezos_screen_t *self) {
    (void)self;
    if (g_send_popup_result == TEZOS_POPUP_CONFIRMED) perform_send();
    g_send_popup_result = TEZOS_POPUP_PENDING; /* reset so a stale result can't leak into a later visit */
}

static tezos_action_t send_confirm_input(tezos_screen_t *self, tezos_input_event_t ev,
                                         tezos_screen_t **next, bool *needs_redraw) {
    (void)self; (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L && !g_send_result.done) {
        /* This used to sign immediately on one button press -- now it
           requires an actual confirmation step first, closing exactly
           the gap popups were built for. */
        char confirm_msg[160];
        snprintf(confirm_msg, sizeof confirm_msg, "Send %s tez to %.20s...? This cannot be undone.",
                g_send_amount_display, g_send_recipient);
        *next = tezos_popup_confirm_get("Confirm Send", confirm_msg,
            "SEND", "CANCEL", &g_send_popup_result);
        return TEZOS_ACTION_PUSH;
    }
    return TEZOS_ACTION_NONE;
}

/* ================= Send: real recipient + amount entry, chained before
   confirm. Was hardcoded until this pass -- the T9 widget existed and
   was verified elsewhere (Settings, Messenger, Explorer) well before
   Wallet's own Send flow, its most central real flow, ever used it. ================= */

typedef struct { tezos_t9_state_t t9; char error[64]; } send_recipient_state_t;
static send_recipient_state_t g_send_recipient_state;
static tezos_screen_t g_send_recipient_screen;

static void send_recipient_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    send_recipient_state_t *st = (send_recipient_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SEND", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "Recipient (tz1/KT1 or .tez):", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 70, &TEZOS_FONT_BODY, &st->t9);
    if (st->error[0]) {
        tezos_draw_text(fb, d, 8, 120, st->error, &TEZOS_FONT_SMALL, TEZOS_AMBER);
    }
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "NEXT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void send_recipient_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    send_recipient_state_t *st = (send_recipient_state_t *)self->app_state;
    *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
}

static tezos_screen_t *wallet_send_amount_screen(void); /* forward decl -- recipient chains to amount */
static tezos_screen_t *wallet_send_confirm_screen(void); /* forward decl -- amount chains to confirm */

static tezos_action_t send_recipient_input(tezos_screen_t *self, tezos_input_event_t ev,
                                           tezos_screen_t **next, bool *needs_redraw) {
    send_recipient_state_t *st = (send_recipient_state_t *)self->app_state;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        const char *typed = tezos_t9_get_text(&st->t9);
        if (typed[0] == '\0') return TEZOS_ACTION_NONE; /* a real send needs a real recipient --
            don't silently advance on empty input */

        /* real .tez resolution -- if this looks like a domain name
           rather than a raw address, resolve it first, and use the
           real resolved address, not the typed name itself. A raw
           tz1/KT1 address (not ending in .tez) passes through
           unchanged, same as before this change. */
        size_t typed_len = strlen(typed);
        bool looks_like_domain = typed_len > 4 && strcmp(typed + typed_len - 4, ".tez") == 0;
        if (looks_like_domain) {
            const char *registry = settings_get_name_registry_contract();
            char resolved[64];
            if (registry[0] == '\0') {
                snprintf(st->error, sizeof st->error, "Set Name Registry in Settings first");
                *needs_redraw = true;
                return TEZOS_ACTION_NONE;
            }
            if (!tezos_tez_domain_resolve(settings_get_network(), registry, typed, resolved, sizeof resolved)) {
                snprintf(st->error, sizeof st->error, "Couldn't resolve %.20s", typed);
                *needs_redraw = true;
                return TEZOS_ACTION_NONE;
            }
            /* real, safe pattern: null-terminated explicitly right
               after this branch merges back with the else branch below
               -- GCC's static analysis can't see across the branch, but
               this is the same strncpy+manual-null-terminate pattern
               used throughout this codebase */
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wstringop-truncation"
            strncpy(g_send_recipient, resolved, sizeof g_send_recipient - 1);
            #pragma GCC diagnostic pop
        } else {
            strncpy(g_send_recipient, typed, sizeof g_send_recipient - 1);
        }
        g_send_recipient[sizeof g_send_recipient - 1] = '\0';
        *next = wallet_send_amount_screen();
        return TEZOS_ACTION_REPLACE; /* replaces itself, not pushed on top -- recipient entry
            isn't meaningful to come back to once submitted; amount entry's own CANCEL/BACK
            returns to wherever Send was originally entered from, same as before this change */
    }
    *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *wallet_send_recipient_screen(void) {
    tezos_t9_init(&g_send_recipient_state.t9);
    g_send_recipient_state.error[0] = '\0';
    g_send_recipient_screen = (tezos_screen_t){
        .render = send_recipient_render, .handle_input = send_recipient_input,
        .on_resume = NULL, .on_tick = send_recipient_on_tick,
        .app_state = &g_send_recipient_state, .debug_name = "wallet_send_recipient",
    };
    return &g_send_recipient_screen;
}

tezos_screen_t *wallet_test_inject_recipient_text(const char *text) {
    tezos_t9_init(&g_send_recipient_state.t9);
    g_send_recipient_state.error[0] = '\0';
    strncpy(g_send_recipient_state.t9.buffer, text, sizeof(g_send_recipient_state.t9.buffer) - 1);
    g_send_recipient_state.t9.buffer[sizeof(g_send_recipient_state.t9.buffer) - 1] = '\0';
    g_send_recipient_state.t9.length = (int)strlen(g_send_recipient_state.t9.buffer);
    g_send_recipient_screen = (tezos_screen_t){
        .render = send_recipient_render, .handle_input = send_recipient_input,
        .on_resume = NULL, .on_tick = send_recipient_on_tick,
        .app_state = &g_send_recipient_state, .debug_name = "wallet_send_recipient",
    };
    return &g_send_recipient_screen;
}

typedef struct { tezos_t9_state_t t9; char error[48]; } send_amount_state_t;
static send_amount_state_t g_send_amount_state;
static tezos_screen_t g_send_amount_screen;

static void send_amount_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    send_amount_state_t *st = (send_amount_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SEND", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "Amount in tez (e.g. 1.5):", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 70, &TEZOS_FONT_BODY, &st->t9);
    if (st->error[0]) tezos_draw_text(fb, d, 8, 130, st->error, &TEZOS_FONT_SMALL, TEZOS_PINK);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "NEXT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void send_amount_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    send_amount_state_t *st = (send_amount_state_t *)self->app_state;
    *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
}

static tezos_action_t send_amount_input(tezos_screen_t *self, tezos_input_event_t ev,
                                        tezos_screen_t **next, bool *needs_redraw) {
    send_amount_state_t *st = (send_amount_state_t *)self->app_state;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        const char *typed = tezos_t9_get_text(&st->t9);
        if (!wallet_tez_to_mutez(typed, g_send_amount_mutez, sizeof g_send_amount_mutez)) {
            strcpy(st->error, "invalid amount (max 6 decimal places)");
            *needs_redraw = true;
            return TEZOS_ACTION_NONE; /* real validation failure -- stay here, don't
                silently advance with a wrong/unset amount */
        }
        strncpy(g_send_amount_display, typed, sizeof g_send_amount_display - 1);
        g_send_amount_display[sizeof g_send_amount_display - 1] = '\0';
        *next = wallet_send_confirm_screen();
        return TEZOS_ACTION_PUSH; /* pushed, not replaced -- BACK from confirm should
            return here to re-enter the amount, matching every other real confirm flow
            in this project (Messenger, Explorer) */
    }
    st->error[0] = '\0';
    *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *wallet_send_amount_screen(void) {
    memset(&g_send_amount_state, 0, sizeof g_send_amount_state);
    tezos_t9_init(&g_send_amount_state.t9);
    g_send_amount_screen = (tezos_screen_t){
        .render = send_amount_render, .handle_input = send_amount_input,
        .on_resume = NULL, .on_tick = send_amount_on_tick,
        .app_state = &g_send_amount_state, .debug_name = "wallet_send_amount",
    };
    return &g_send_amount_screen;
}

static tezos_screen_t g_send_confirm_screen;
static tezos_screen_t *wallet_send_confirm_screen(void) {
    memset(&g_send_result, 0, sizeof g_send_result);
    g_send_confirm_screen = (tezos_screen_t){
        .render = send_confirm_render, .handle_input = send_confirm_input,
        .on_resume = send_confirm_on_resume, .app_state = NULL, .debug_name = "wallet_send_confirm",
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
/* real contacts now come from the address book, populated by
   refresh_contacts_list() -- see wallet_pages_state_t */

typedef struct {
    tezos_paged_view_state_t pager;
    bool in_sublist; /* true = Up/Down scroll the current page's list instead of flipping pages */
    tezos_list_state_t tx_list;
    tezos_list_state_t contacts_list;
    /* real address-book-backed contact list -- +1 slot for the always-
       present "+ Add Contact" entry at index 0 */
    tezos_list_item_t contact_items[TEZOS_ADDRESS_BOOK_MAX_CONTACTS + 1];
    char contact_labels[TEZOS_ADDRESS_BOOK_MAX_CONTACTS + 1][TEZOS_ADDRESS_BOOK_NAME_MAX];
    op_result_t delegate_result;
    char address[64];
} wallet_pages_state_t;

static wallet_pages_state_t g_pages_state;

/* Real refresh from the address book -- called on entry to the
   Contacts sublist and after a real add, so the list always reflects
   what's actually saved, not a stale snapshot. */
static void refresh_contacts_list(wallet_pages_state_t *st) {
    strcpy(st->contact_labels[0], "+ Add Contact");
    st->contact_items[0] = (tezos_list_item_t){st->contact_labels[0], NULL};
    int count = tezos_address_book_count();
    for (int i = 0; i < count; i++) {
        tezos_address_book_entry_t entry;
        tezos_address_book_get(i, &entry);
        /* GCC's -Wstringop-truncation is a known false positive here at
           -O2 (same class already documented and suppressed in
           tezos_popup.c's safe_copy(), explorer.c's entrypoint labels,
           and wallet.c's own tez_to_mutez) -- explicit null-termination
           follows immediately below, this is a real, safe bound. */
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstringop-truncation"
        strncpy(st->contact_labels[i + 1], entry.name, sizeof st->contact_labels[i + 1] - 1);
        #pragma GCC diagnostic pop
        st->contact_labels[i + 1][sizeof st->contact_labels[i + 1] - 1] = '\0';
        st->contact_items[i + 1] = (tezos_list_item_t){st->contact_labels[i + 1], NULL};
    }
    tezos_list_init(&st->contacts_list, st->contact_items, count + 1);
}


/* ================= Add Contact: real T9 name entry, then real T9
   address entry, real validation via tezos_address_book_add (which
   itself validates through tezos_forge_address) -- see
   docs/CONCEPTS.md's address-book concept for the full reasoning:
   this is the "save a name once, reuse it" shortcut in front of raw
   T9 entry, not a replacement for it. ================= */

static char g_new_contact_name[TEZOS_ADDRESS_BOOK_NAME_MAX] = "";

typedef struct { tezos_t9_state_t t9; } contact_name_state_t;
static contact_name_state_t g_contact_name_state;
static tezos_screen_t g_contact_name_screen;

static void contact_name_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    contact_name_state_t *st = (contact_name_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "ADD CONTACT", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "Name:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 70, &TEZOS_FONT_BODY, &st->t9);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "NEXT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void contact_name_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    contact_name_state_t *st = (contact_name_state_t *)self->app_state;
    *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
}

static tezos_screen_t *contact_address_screen(void); /* forward decl -- name chains to address */

static tezos_action_t contact_name_input(tezos_screen_t *self, tezos_input_event_t ev,
                                         tezos_screen_t **next, bool *needs_redraw) {
    contact_name_state_t *st = (contact_name_state_t *)self->app_state;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        const char *typed = tezos_t9_get_text(&st->t9);
        if (typed[0] == '\0') return TEZOS_ACTION_NONE; /* a contact needs a real name */
        strncpy(g_new_contact_name, typed, sizeof g_new_contact_name - 1);
        g_new_contact_name[sizeof g_new_contact_name - 1] = '\0';
        *next = contact_address_screen();
        return TEZOS_ACTION_REPLACE; /* replaces itself, matching Send's own
            recipient->amount chaining pattern -- not meaningful to come back to */
    }
    *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *contact_name_screen_get(void) {
    tezos_t9_init(&g_contact_name_state.t9);
    g_contact_name_screen = (tezos_screen_t){
        .render = contact_name_render, .handle_input = contact_name_input,
        .on_resume = NULL, .on_tick = contact_name_on_tick,
        .app_state = &g_contact_name_state, .debug_name = "wallet_contact_name",
    };
    return &g_contact_name_screen;
}

typedef struct { tezos_t9_state_t t9; char error[48]; } contact_address_state_t;
static contact_address_state_t g_contact_address_state;
static tezos_screen_t g_contact_address_screen;

static void contact_address_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    contact_address_state_t *st = (contact_address_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "ADD CONTACT", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "Address (tz1/KT1):", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 70, &TEZOS_FONT_BODY, &st->t9);
    if (st->error[0]) tezos_draw_text(fb, d, 8, 130, st->error, &TEZOS_FONT_SMALL, TEZOS_PINK);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SAVE", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void contact_address_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    contact_address_state_t *st = (contact_address_state_t *)self->app_state;
    *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
}

static tezos_action_t contact_address_input(tezos_screen_t *self, tezos_input_event_t ev,
                                            tezos_screen_t **next, bool *needs_redraw) {
    contact_address_state_t *st = (contact_address_state_t *)self->app_state;
    (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        const char *typed = tezos_t9_get_text(&st->t9);
        if (!tezos_address_book_add(g_new_contact_name, typed)) {
            /* real validation failure -- tezos_address_book_add itself checks via
               tezos_forge_address, an empty name, or a full book; any of those
               should keep the user here to fix it, not silently lose their input */
            strcpy(st->error, "invalid address, or book is full");
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        refresh_contacts_list(&g_pages_state); /* real, immediate reflect --
            so the very next screen (Contacts) already shows the new entry */
        return TEZOS_ACTION_POP;
    }
    st->error[0] = '\0';
    *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *contact_address_screen(void) {
    memset(&g_contact_address_state, 0, sizeof g_contact_address_state);
    tezos_t9_init(&g_contact_address_state.t9);
    g_contact_address_screen = (tezos_screen_t){
        .render = contact_address_render, .handle_input = contact_address_input,
        .on_resume = NULL, .on_tick = contact_address_on_tick,
        .app_state = &g_contact_address_state, .debug_name = "wallet_contact_address",
    };
    return &g_contact_address_screen;
}
static void perform_delegate(void) {
    op_result_t *r = &g_pages_state.delegate_result;
    memset(r, 0, sizeof *r);
    char source[64];
    tezos_mock_signer_address_b58(&g_wallet_signer, source, sizeof source);

    tezos_delegation_params_t params;
    strcpy(params.source, source);
    strcpy(params.fee, "374");

    char balance[32], real_counter[32];
    if (tezos_tzkt_get_account(settings_get_network(), source, balance, sizeof balance,
                               real_counter, sizeof real_counter)) {
        unsigned long long next_counter = strtoull(real_counter, NULL, 10) + 1;
        snprintf(params.counter, sizeof params.counter, "%llu", next_counter);
    } else {
        strcpy(params.counter, "1"); /* offline fallback, same as perform_send */
    }
    char branch[128];
    bool have_real_branch = tezos_tzkt_get_head_hash(settings_get_network(), branch, sizeof branch);
    const char *branch_to_use = have_real_branch ? branch : DEV_PLACEHOLDER_BRANCH;

    strcpy(params.gas_limit, "1000");
    strcpy(params.storage_limit, "0");
    strcpy(params.delegate, DEV_PLACEHOLDER_BAKER); /* stand-in "baker" address — no real baker registry yet, a real, separate gap from Send's recipient (see build notes) */

    tezos_bytes_t content; tezos_bytes_init(&content);
    if (!tezos_forge_delegation(&content, &params)) { strcpy(r->error, "forge_delegation failed"); return; }
    tezos_bytes_t full; tezos_bytes_init(&full);
    if (!tezos_forge_operation_group(&full, branch_to_use, content.data, content.len)) {
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

    r->broadcast_attempted = true;
    r->broadcast_ok = tezos_rpc_inject_operation(settings_get_rpc_node(), full.data, full.len, r->op_hash, sizeof r->op_hash);
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
            int selected = st->contacts_list.selected;
            if (selected == 0) {
                *next = contact_name_screen_get(); /* "+ Add Contact" */
                return TEZOS_ACTION_PUSH;
            }
            /* a real saved contact -- use its real saved address directly,
               skipping recipient entry entirely (that's the actual point
               of an address book: pick from saved, don't retype it) */
            tezos_address_book_entry_t entry;
            if (tezos_address_book_get(selected - 1, &entry)) {
                strncpy(g_send_recipient, entry.address, sizeof g_send_recipient - 1);
                g_send_recipient[sizeof g_send_recipient - 1] = '\0';
                *next = wallet_send_amount_screen();
                return TEZOS_ACTION_PUSH;
            }
            return TEZOS_ACTION_NONE;
        }
        return TEZOS_ACTION_NONE;
    }

    if (tezos_paged_view_handle_input(&st->pager, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP; /* MENU -> back to idle */
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        switch (page) {
            case PAGE_BALANCES:
                *next = wallet_send_recipient_screen();
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

/* ============================================================
   Startup unlock gate -- shown instead of the real wallet if a saved
   signer store exists on disk (from a previous session's recovery-
   phrase import, see import_recovery.c) and nothing has been unlocked
   yet THIS session. Without this, an imported phrase would silently
   never be used again after a restart -- exactly the gap
   tezos_store.h closes. */
typedef struct {
    tezos_pin_result_t pin_result;
    char pin_buffer[TEZOS_PIN_MAX_LEN + 1];
    bool unlock_failed; /* shows an error after a wrong PIN, cleared on retry */
} locked_state_t;
static locked_state_t g_locked_state;
static tezos_screen_t g_locked_screen;

static void locked_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    locked_state_t *st = (locked_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "WALLET LOCKED", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 44, "A saved recovery phrase exists.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 62, "Enter your PIN to unlock it.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    if (st->unlock_failed) {
        tezos_draw_text(fb, d, 8, 90, "Wrong PIN, or corrupted data.", &TEZOS_FONT_SMALL, TEZOS_PINK);
    }
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "UNLOCK", &TEZOS_FONT_SMALL, TEZOS_CYAN);
}

static tezos_action_t locked_input(tezos_screen_t *self, tezos_input_event_t ev,
                                   tezos_screen_t **next, bool *needs_redraw) {
    locked_state_t *st = (locked_state_t *)self->app_state;
    (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        *next = tezos_pin_entry_get("ENTER PIN", st->pin_buffer, &st->pin_result);
        return TEZOS_ACTION_PUSH;
    }
    return TEZOS_ACTION_NONE;
}

static void locked_on_resume(tezos_screen_t *self) {
    locked_state_t *st = (locked_state_t *)self->app_state;
    if (st->pin_result != TEZOS_PIN_CONFIRMED) return; /* cancelled -- stay locked, no change */

    tezos_store_signer_data_t data;
    if (!tezos_store_load_signer(TEZOS_STORE_DEFAULT_SIGNER_PATH, st->pin_buffer, &data)) {
        st->unlock_failed = true;
        return;
    }
    /* wallet_set_active_signer_from_mnemonic (not the raw derive-then-set
       pattern this used to use) so account switching works for the rest
       of this session without asking for the PIN again. */
    if (!wallet_set_active_signer_from_mnemonic(data.mnemonic, data.word_count,
                                                data.passphrase, data.account)) {
        st->unlock_failed = true; /* store decrypted fine but the saved mnemonic itself
            doesn't validate -- shouldn't happen given save-time validation, but don't assume */
        return;
    }
    /* Actual screen swap happens back in wallet_entry_screen()'s caller
       via the app registry re-invoking entry -- this screen doesn't
       self-replace since tezos_core's REPLACE semantics here would
       need the caller (linux_shim's app dispatch) to re-enter Wallet,
       which already happens naturally the next time Wallet is opened.
       Simplest correct behavior for now: pop back to Idle and let the
       user re-open Wallet, which will find g_signer_ready already true
       and skip the lock gate. A same-screen in-place transition to the
       real wallet pages would be smoother UX -- a real, small gap, not
       a silent one. */
}

static tezos_screen_t *locked_screen_get(void) {
    memset(&g_locked_state, 0, sizeof g_locked_state);
    g_locked_screen = (tezos_screen_t){
        .render = locked_render, .handle_input = locked_input,
        .on_resume = locked_on_resume, .on_tick = NULL,
        .app_state = &g_locked_state, .debug_name = "wallet_locked",
    };
    return &g_locked_screen;
}

tezos_screen_t *wallet_entry_screen(void) {
    static bool http_initialized = false;
    if (!http_initialized) { tezos_http_init(); http_initialized = true; }

    if (!g_signer_ready && tezos_store_exists(TEZOS_STORE_DEFAULT_SIGNER_PATH)) {
        return locked_screen_get();
    }

    ensure_signer();
    memset(&g_pages_state, 0, sizeof g_pages_state);
    tezos_mock_signer_address_b58(&g_wallet_signer, g_pages_state.address, sizeof g_pages_state.address);
    tezos_paged_view_init(&g_pages_state.pager, PAGE_COUNT);
    tezos_list_init(&g_pages_state.tx_list, g_tx_items, 4);
    refresh_contacts_list(&g_pages_state); /* real, from the address book */
    return &g_wallet_pages_screen;
}
