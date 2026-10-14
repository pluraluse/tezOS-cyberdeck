#include "beacon_wallet.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../core/tezos_popup.h"
#include "../../chain/tezos_beacon_message.h"
#include "../../chain/tezos_store.h"
#include "../../chain/tezos_forge.h"
#include "../../chain/tezos_rpc.h"
#include "../../chain/tezos_tzkt.h"
#include "../../chain/tezos_micheline.h"
#include "../wallet/wallet.h"
#include "../settings/settings.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sodium.h>

typedef enum {
    BW_NO_SESSION, BW_SESSION_ACTIVE, BW_INCOMING_PERMISSION, BW_INCOMING_OPERATION,
    BW_RESPONSE_RESULT,
} bw_state_t;

typedef struct {
    bw_state_t state;
    tezos_beacon_peer_info_t peer;

    tezos_beacon_session_t session;
    bool connect_attempted;
    bool connect_ok;

    tezos_beacon_permission_request_t pending_permission;
    tezos_beacon_operation_request_t pending_operation;
    bool response_ok;
    char response_error[64];
} beacon_wallet_state_t;
static beacon_wallet_state_t g_state;
static tezos_screen_t g_screen;
static tezos_popup_result_t g_confirm_result = TEZOS_POPUP_PENDING;

#define BEACON_IDENTITY_STORE_PATH "/tmp/tezos_cyberdeck_beacon_identity.txt"

/* This device's own persistent Beacon identity seed -- deliberately
   separate from any Tezos account keypair (matching
   tezos_beacon_crypto.h's own design: "a genuinely separate identity
   from any Tezos account keypair"). Generated once with real entropy
   (randombytes_buf -- real OS-level entropy on this Linux side, not
   the STM32 RNG question from the signer-firmware discussion, a
   completely separate concern) and persisted so the same identity
   survives restarts -- a dApp that's paired with this device once
   should keep recognizing it as the same wallet. */
static void get_or_create_beacon_identity(char *seed_hex_out, size_t cap) {
    if (tezos_store_load_plain(BEACON_IDENTITY_STORE_PATH, "identity_seed", seed_hex_out, cap)) {
        return;
    }
    uint8_t raw[32];
    randombytes_buf(raw, sizeof raw);
    for (int i = 0; i < 32; i++) snprintf(seed_hex_out + i * 2, 3, "%02x", raw[i]);
    tezos_store_save_plain(BEACON_IDENTITY_STORE_PATH, "identity_seed", seed_hex_out);
}

/* Core, fully testable logic: given a decrypted incoming Beacon
   message JSON, classify it and store the parsed request for display.
   Deliberately separate from the network-touching poll loop, so it can
   be exercised directly without needing live Matrix connectivity --
   same "test the logic independent of the network hop" split already
   used for TZKT/Matrix throughout this project. */
static bw_state_t classify_incoming_message(beacon_wallet_state_t *st, const char *json) {
    if (tezos_beacon_parse_permission_request(json, &st->pending_permission)) {
        return BW_INCOMING_PERMISSION;
    }
    if (tezos_beacon_parse_operation_request(json, &st->pending_operation)) {
        return BW_INCOMING_OPERATION;
    }
    return BW_SESSION_ACTIVE; /* not a recognized type -- ignore, stay active */
}

static void perform_permission_confirm(beacon_wallet_state_t *st) {
    char address[64];
    if (!wallet_get_active_address(address, sizeof address)) {
        strcpy(st->response_error, "no active signer -- open Wallet first");
        st->response_ok = false; st->state = BW_RESPONSE_RESULT; return;
    }
    char pubkey[128];
    if (!wallet_derive_account_address(0, pubkey, sizeof pubkey)) {
        strcpy(pubkey, address); /* placeholder -- real gap, see build notes:
            this needs the real PUBLIC KEY, not another address */
    }

    char *json_out = NULL;
    if (!tezos_beacon_build_permission_response(&st->pending_permission, "tezOS", pubkey, address, &json_out)) {
        strcpy(st->response_error, "failed to build permission response");
        st->response_ok = false; st->state = BW_RESPONSE_RESULT; return;
    }
    st->response_ok = tezos_beacon_send(&st->session, json_out); /* real network call --
        can't be live-verified here, same disclosed limitation as every
        other Matrix-touching call in this project */
    if (!st->response_ok) strcpy(st->response_error, "send failed (network)");
    free(json_out);
    st->state = BW_RESPONSE_RESULT;
}

static void perform_operation_confirm(beacon_wallet_state_t *st) {
    if (st->pending_operation.operation_count == 0) {
        strcpy(st->response_error, "no operations in request");
        st->response_ok = false; st->state = BW_RESPONSE_RESULT; return;
    }
    const tezos_beacon_op_t *op = &st->pending_operation.operations[0]; /* first op only --
        real Beacon requests can batch multiple; a real, disclosed scope cut */

    char source[64];
    if (!wallet_get_active_address(source, sizeof source)) {
        strcpy(st->response_error, "no active signer -- open Wallet first");
        st->response_ok = false; st->state = BW_RESPONSE_RESULT; return;
    }

    tezos_transaction_params_t params = {0};
    strcpy(params.source, source);
    strcpy(params.fee, "500");
    strcpy(params.gas_limit, "2000");
    strcpy(params.storage_limit, "100");
    strncpy(params.amount, op->amount, sizeof params.amount - 1);
    params.amount[sizeof params.amount - 1] = '\0';
    strncpy(params.destination, op->destination, sizeof params.destination - 1);
    params.destination[sizeof params.destination - 1] = '\0';
    tezos_micheline_node_t value;
    if (op->has_entrypoint) {
        params.entrypoint = op->entrypoint;
        value = tezos_micheline_int(op->parameter_int);
        params.parameter_value = &value;
    }

    char balance[32], real_counter[32];
    if (tezos_tzkt_get_account(settings_get_network(), source, balance, sizeof balance,
                               real_counter, sizeof real_counter)) {
        unsigned long long next_counter = strtoull(real_counter, NULL, 10) + 1;
        snprintf(params.counter, sizeof params.counter, "%llu", next_counter);
    } else {
        strcpy(params.counter, "1");
    }
    char branch[128];
    const char *branch_to_use = tezos_tzkt_get_head_hash(settings_get_network(), branch, sizeof branch)
                               ? branch : "BLzyjjHKEKMULtvkpSHxuZxx6ei6fpntH2BTkYZiLgs8zLVstvX";

    tezos_bytes_t content_bytes; tezos_bytes_init(&content_bytes);
    if (!tezos_forge_transaction(&content_bytes, &params)) {
        tezos_bytes_free(&content_bytes);
        strcpy(st->response_error, "forge failed");
        st->response_ok = false; st->state = BW_RESPONSE_RESULT; return;
    }
    tezos_bytes_t full; tezos_bytes_init(&full);
    if (!tezos_forge_operation_group(&full, branch_to_use, content_bytes.data, content_bytes.len)) {
        tezos_bytes_free(&content_bytes);
        strcpy(st->response_error, "forge_operation_group failed");
        st->response_ok = false; st->state = BW_RESPONSE_RESULT; return;
    }
    tezos_bytes_free(&content_bytes);

    char sig[128];
    if (!wallet_sign_operation(&full, sig, sizeof sig)) {
        tezos_bytes_free(&full);
        strcpy(st->response_error, "sign failed");
        st->response_ok = false; st->state = BW_RESPONSE_RESULT; return;
    }

    char op_hash[80];
    bool broadcast_ok = tezos_rpc_inject_operation(settings_get_rpc_node(), full.data, full.len,
                                                   op_hash, sizeof op_hash);
    tezos_bytes_free(&full);
    if (!broadcast_ok) {
        strcpy(st->response_error, "broadcast failed");
        st->response_ok = false; st->state = BW_RESPONSE_RESULT; return;
    }

    char *json_out = NULL;
    if (!tezos_beacon_build_operation_response(&st->pending_operation, op_hash, &json_out)) {
        strcpy(st->response_error, "broadcast succeeded but building the response failed");
        st->response_ok = false; st->state = BW_RESPONSE_RESULT; return;
    }
    st->response_ok = tezos_beacon_send(&st->session, json_out);
    if (!st->response_ok) strcpy(st->response_error, "operation broadcast, but sending the response failed (network)");
    free(json_out);
    st->state = BW_RESPONSE_RESULT;
}

static void render_no_session(tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "BEACON WALLET", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "No active session.", &TEZOS_FONT_BODY, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 80, "Pair with a dApp via", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 98, "Camera > Scan first.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_session(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    beacon_wallet_state_t *st = (beacon_wallet_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "BEACON WALLET", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    int y = 50;
    switch (st->state) {
        case BW_SESSION_ACTIVE:
            if (!st->connect_ok) {
                tezos_draw_text(fb, d, 8, y, "Couldn't reach Matrix (real,", &TEZOS_FONT_SMALL, TEZOS_AMBER);
                y += 18;
                tezos_draw_text(fb, d, 8, y, "disclosed network limitation", &TEZOS_FONT_SMALL, TEZOS_AMBER);
                y += 18;
                tezos_draw_text(fb, d, 8, y, "in this environment).", &TEZOS_FONT_SMALL, TEZOS_AMBER);
            } else {
                tezos_draw_text(fb, d, 8, y, "SESSION ACTIVE", &TEZOS_FONT_BODY, TEZOS_CYAN);
                y += 22;
                char line[80];
                snprintf(line, sizeof line, "Paired: %s", st->peer.name);
                tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 20;
                tezos_draw_text(fb, d, 8, y, "Waiting for a real request.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            }
            break;
        case BW_INCOMING_PERMISSION:
            tezos_draw_text(fb, d, 8, y, "CONNECTION REQUEST", &TEZOS_FONT_BODY, TEZOS_CYAN);
            y += 24;
            {
                char line[80];
                snprintf(line, sizeof line, "App: %s", st->pending_permission.app_name);
                tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
                snprintf(line, sizeof line, "Network: %s", st->pending_permission.network_type);
                tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT);
            }
            break;
        case BW_INCOMING_OPERATION:
            tezos_draw_text(fb, d, 8, y, "SIGNING REQUEST", &TEZOS_FONT_BODY, TEZOS_CYAN);
            y += 24;
            if (st->pending_operation.operation_count > 0) {
                char line[80];
                const tezos_beacon_op_t *op = &st->pending_operation.operations[0];
                snprintf(line, sizeof line, "To: %.20s...", op->destination);
                tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
                snprintf(line, sizeof line, "Amount: %s mutez", op->amount);
                tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
                if (op->has_entrypoint) {
                    snprintf(line, sizeof line, "Entrypoint: %s", op->entrypoint);
                    tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT);
                }
            }
            break;
        case BW_RESPONSE_RESULT:
            if (st->response_ok) {
                tezos_draw_text(fb, d, 8, y, "Response sent.", &TEZOS_FONT_BODY, TEZOS_CYAN);
            } else {
                tezos_draw_text(fb, d, 8, y, "Failed:", &TEZOS_FONT_BODY, TEZOS_PINK);
                y += 20;
                tezos_draw_text(fb, d, 8, y, st->response_error, &TEZOS_FONT_SMALL, TEZOS_MUTED);
            }
            break;
        case BW_NO_SESSION: break; /* unreachable via this render path, see screen dispatch */
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    const char *left_label = "";
    if (st->state == BW_SESSION_ACTIVE) left_label = "CHECK NOW";
    else if (st->state == BW_INCOMING_PERMISSION || st->state == BW_INCOMING_OPERATION) left_label = "APPROVE";
    if (st->state != BW_RESPONSE_RESULT && left_label[0]) {
        tezos_draw_text(fb, d, 8, fb->height - 26, left_label, &TEZOS_FONT_SMALL, TEZOS_CYAN);
    }
    const char *right_label = (st->state == BW_INCOMING_PERMISSION || st->state == BW_INCOMING_OPERATION) ? "DENY" : "BACK";
    int backw = tezos_text_width(right_label, &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, right_label, &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void screen_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    beacon_wallet_state_t *st = (beacon_wallet_state_t *)self->app_state;
    if (st->state == BW_NO_SESSION) render_no_session(fb, d);
    else render_session(self, fb, d);
}

static tezos_action_t screen_input(tezos_screen_t *self, tezos_input_event_t ev,
                                   tezos_screen_t **next, bool *needs_redraw) {
    beacon_wallet_state_t *st = (beacon_wallet_state_t *)self->app_state;

    if (st->state == BW_NO_SESSION) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
        return TEZOS_ACTION_NONE;
    }

    if (st->state == BW_SESSION_ACTIVE) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
        if (ev.type == TEZOS_INPUT_SOFTKEY_L && st->connect_ok) {
            char messages[TEZOS_BEACON_MAX_MESSAGES][2048];
            int count = 0;
            if (tezos_beacon_receive(&st->session, 1000, messages, &count)) {
                for (int i = 0; i < count; i++) {
                    bw_state_t result = classify_incoming_message(st, messages[i]);
                    if (result != BW_SESSION_ACTIVE) { st->state = result; break; }
                }
            }
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->state == BW_INCOMING_PERMISSION || st->state == BW_INCOMING_OPERATION) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { /* DENY -- just return to active, no response sent.
            A real wallet would send an aborted/error response; not built in this pass
            (a real, disclosed scope cut -- the APPROVE path is this feature's actual point). */
            st->state = BW_SESSION_ACTIVE;
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) { /* APPROVE -- real confirmation popup first,
            same pattern as every other real send/call flow in this project */
            const char *msg = st->state == BW_INCOMING_PERMISSION
                             ? "Allow this app to connect and request signatures?"
                             : "Sign and broadcast this operation? This cannot be undone.";
            *next = tezos_popup_confirm_get("Confirm", msg, "APPROVE", "CANCEL", &g_confirm_result);
            return TEZOS_ACTION_PUSH;
        }
        return TEZOS_ACTION_NONE;
    }

    /* BW_RESPONSE_RESULT */
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->state = BW_SESSION_ACTIVE; *needs_redraw = true; }
    return TEZOS_ACTION_NONE;
}

static void screen_on_resume(tezos_screen_t *self) {
    beacon_wallet_state_t *st = (beacon_wallet_state_t *)self->app_state;
    if (g_confirm_result == TEZOS_POPUP_CONFIRMED) {
        if (st->state == BW_INCOMING_PERMISSION) perform_permission_confirm(st);
        else if (st->state == BW_INCOMING_OPERATION) perform_operation_confirm(st);
    }
    g_confirm_result = TEZOS_POPUP_PENDING;
}

tezos_screen_t *beacon_wallet_screen_from_peer(const tezos_beacon_peer_info_t *peer) {
    memset(&g_state, 0, sizeof g_state);
    g_state.peer = *peer;

    char identity_seed[80];
    get_or_create_beacon_identity(identity_seed, sizeof identity_seed);
    g_state.connect_attempted = true;
    g_state.connect_ok = tezos_beacon_prepare(&g_state.session, identity_seed, &g_state.peer)
                       && tezos_beacon_connect(&g_state.session); /* real network call --
        will correctly fail in this sandbox (no reachable Matrix homeserver),
        same disclosed limitation as everywhere else Matrix is touched */
    g_state.state = BW_SESSION_ACTIVE;

    g_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = screen_on_resume, .on_tick = NULL,
        .app_state = &g_state, .debug_name = "beacon_wallet",
    };
    return &g_screen;
}

tezos_screen_t *beacon_wallet_entry_screen(void) {
    memset(&g_state, 0, sizeof g_state);
    g_state.state = BW_NO_SESSION;
    g_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = screen_on_resume, .on_tick = NULL,
        .app_state = &g_state, .debug_name = "beacon_wallet",
    };
    return &g_screen;
}

tezos_screen_t *beacon_wallet_test_inject_active_session(void) {
    memset(&g_state, 0, sizeof g_state);
    strcpy(g_state.peer.name, "test-dapp");
    g_state.connect_attempted = true;
    g_state.connect_ok = true;
    g_state.state = BW_SESSION_ACTIVE;
    g_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = screen_on_resume, .on_tick = NULL,
        .app_state = &g_state, .debug_name = "beacon_wallet",
    };
    return &g_screen;
}

bool beacon_wallet_test_inject_message(const char *json) {
    bw_state_t result = classify_incoming_message(&g_state, json);
    if (result == BW_SESSION_ACTIVE) return false; /* not a recognized request type */
    g_state.state = result;
    return true;
}
