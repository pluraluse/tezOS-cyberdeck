#include "explorer.h"
#include "../wallet/wallet.h"
#include "../settings/settings.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../core/tezos_t9.h"
#include "../../core/tezos_popup.h"
#include "../../chain/tezos_tzkt.h"
#include "../../chain/tezos_forge.h"
#include "../../chain/tezos_rpc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    EXP_ADDRESS_ENTRY, EXP_INFO, EXP_ENTRYPOINTS, EXP_HOLDINGS,
    EXP_CALL_LEAF, EXP_CALL_LIST_COUNT, EXP_CALL_RESULT,
} explorer_phase_t;

#define EXP_MAX_ENTRYPOINTS 32 /* real bound, matches tezos_tzkt_entrypoint_t
    arrays elsewhere in this project */
#define EXP_MAX_HOLDINGS 12    /* same display-limit reasoning as Gallery/Messenger --
    real pagination is a separate, unbuilt feature */

typedef struct {
    explorer_phase_t phase;
    tezos_t9_state_t t9;
    char address[40];

    bool info_fetch_ok;
    tezos_tzkt_contract_info_t info;

    bool entrypoints_fetch_ok;
    tezos_tzkt_entrypoint_t entrypoints[EXP_MAX_ENTRYPOINTS];
    int entrypoint_count;
    tezos_list_state_t entrypoints_list;
    tezos_list_item_t entrypoints_items[EXP_MAX_ENTRYPOINTS];
    char entrypoints_labels[EXP_MAX_ENTRYPOINTS][56]; /* name(32) + "  (type unknown)"(17) +
        NUL can reach 50 bytes -- sized with real headroom */

    bool holdings_fetch_ok;
    tezos_tzkt_owned_token_t holdings[EXP_MAX_HOLDINGS];
    int holdings_count;
    tezos_list_state_t holdings_list;
    tezos_list_item_t holdings_items[EXP_MAX_HOLDINGS];
    char holdings_labels[EXP_MAX_HOLDINGS][128]; /* same headroom fix as Gallery's
        equivalent buffer -- alias(31)+": "+name(63)+NUL can reach 97 bytes */

    tezos_list_state_t info_menu;
    tezos_list_item_t info_menu_items[2];

    /* real entrypoint-calling state -- see tezos_micheline_walker_t's
       own design note in tezos_micheline.h for why this is a pausable
       state machine rather than a single function call */
    int calling_entrypoint_index; /* which entry in `entrypoints[]` is being called */
    tezos_micheline_walker_t walker;
    tezos_t9_state_t call_t9;
    char call_error_msg[80];
    bool call_send_attempted;
    bool call_send_ok;
    char call_op_hash[80];
} explorer_state_t;

static explorer_state_t g_state;
static tezos_screen_t g_explorer_screen;
static tezos_popup_result_t g_confirm_result = TEZOS_POPUP_PENDING;

/* Real fetch of all three signals for the looked-up address. Failure
   of any ONE doesn't block the others -- a contract can have real
   entrypoints/holdings even if, say, its activity info is momentarily
   unreachable, and vice versa. Each screen reports its OWN fetch
   status honestly rather than one combined pass/fail. */
static void perform_lookup(explorer_state_t *st) {
    st->info_fetch_ok = tezos_tzkt_get_contract_info(TEZOS_NETWORK_MAINNET, st->address, &st->info);

    st->entrypoints_fetch_ok = tezos_tzkt_get_entrypoints_with_types(
        TEZOS_NETWORK_MAINNET, st->address, st->entrypoints, EXP_MAX_ENTRYPOINTS, &st->entrypoint_count);
    if (st->entrypoints_fetch_ok && st->entrypoint_count > 0) {
        for (int i = 0; i < st->entrypoint_count; i++) {
            /* GCC's -Wformat-truncation is a known false positive here at -O2
               (same class of issue tezos_popup.c's safe_copy() already
               documents and suppresses) -- it can't see that
               tezos_tzkt_entrypoint_t.name is a fixed 32-byte field,
               always null-terminated by tezos_tzkt.c's own parsing, and
               conservatively assumes an unbounded source string instead.
               The actual destination buffer size below is real and
               correctly sized; this is a compiler limitation, not a
               real truncation risk. */
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(st->entrypoints_labels[i], sizeof st->entrypoints_labels[i], "%s%s",
                    st->entrypoints[i].name,
                    st->entrypoints[i].type_root >= 0 ? "" : "  (type unknown)");
            #pragma GCC diagnostic pop
            st->entrypoints_items[i] = (tezos_list_item_t){st->entrypoints_labels[i], NULL};
        }
        tezos_list_init(&st->entrypoints_list, st->entrypoints_items, st->entrypoint_count);
    }

    st->holdings_fetch_ok = tezos_tzkt_get_owned_tokens(
        TEZOS_NETWORK_MAINNET, st->address, st->holdings, EXP_MAX_HOLDINGS, &st->holdings_count);
    if (st->holdings_fetch_ok && st->holdings_count > 0) {
        for (int i = 0; i < st->holdings_count; i++) {
            const char *alias = st->holdings[i].contract_alias[0] ? st->holdings[i].contract_alias : "(unnamed)";
            const char *name = st->holdings[i].name[0] ? st->holdings[i].name : "(untitled)";
            snprintf(st->holdings_labels[i], sizeof st->holdings_labels[i], "%s: %s", alias, name);
            st->holdings_items[i] = (tezos_list_item_t){st->holdings_labels[i], NULL};
        }
        tezos_list_init(&st->holdings_list, st->holdings_items, st->holdings_count);
    }

    st->info_menu_items[0] = (tezos_list_item_t){"Entrypoints", NULL};
    st->info_menu_items[1] = (tezos_list_item_t){"Holdings", NULL};
    tezos_list_init(&st->info_menu, st->info_menu_items, 2);
}

static const char *leaf_kind_label(tezos_micheline_type_kind_t kind) {
    switch (kind) {
        case TEZOS_MTYPE_INT: return "Enter a whole number (can be negative):";
        case TEZOS_MTYPE_NAT: return "Enter a whole number (0 or more):";
        case TEZOS_MTYPE_MUTEZ: return "Enter an amount in mutez:";
        case TEZOS_MTYPE_TIMESTAMP: return "Enter a Unix timestamp (seconds):";
        case TEZOS_MTYPE_KEY_HASH: return "Enter a baker/delegate address:";
        case TEZOS_MTYPE_STRING: return "Enter text:";
        case TEZOS_MTYPE_BYTES: return "Enter hex bytes:";
        case TEZOS_MTYPE_ADDRESS: return "Enter an address (tz1/KT1):";
        case TEZOS_MTYPE_BOOL: return "Enter true or false:";
        default: return "Enter a value:";
    }
}

static tezos_micheline_type_kind_t g_pending_leaf_kind; /* set by advance_walker,
    read by render_call_leaf -- module-static is fine here, matches this
    project's existing single-event-loop, non-reentrant assumption
    throughout (e.g. tezos_micheline.c's own build-time scratch buffers) */

/* Drives the walker forward to its next required action and sets up
   the corresponding UI phase for the two "need more input" cases.
   Returns the action so the caller (screen_input) can push a real
   confirmation popup when DONE, since only screen_input's return value
   can actually push a new screen. */
static tezos_walk_action_t advance_walker(explorer_state_t *st) {
    tezos_walk_action_t action = tezos_micheline_walker_next(&st->walker, &g_pending_leaf_kind);
    if (action == TEZOS_WALK_NEED_LEAF) {
        tezos_t9_init(&st->call_t9);
        st->phase = EXP_CALL_LEAF;
    } else if (action == TEZOS_WALK_NEED_LIST_COUNT) {
        tezos_t9_init(&st->call_t9);
        st->phase = EXP_CALL_LIST_COUNT;
    }
    return action;
}

/* Real forge + sign + broadcast, same verified pipeline as
   Scanner/Messenger's own perform_call/perform_send. */
static void perform_call(explorer_state_t *st) {
    const tezos_tzkt_entrypoint_t *ep = &st->entrypoints[st->calling_entrypoint_index];

    tezos_micheline_node_t values[TEZOS_MTYPE_MAX_NODES];
    int val_root = tezos_micheline_build_value(&ep->type_pool, ep->type_root,
                                               st->walker.leaf_ptrs, st->walker.leaf_count,
                                               st->walker.list_item_counts, st->walker.list_item_counts_len,
                                               values, TEZOS_MTYPE_MAX_NODES);
    if (val_root < 0) {
        strcpy(st->call_error_msg, "building the call value failed -- a leaf may not have validated");
        st->call_send_attempted = true; st->call_send_ok = false;
        st->phase = EXP_CALL_RESULT;
        return;
    }

    char source[64];
    if (!wallet_get_active_address(source, sizeof source)) {
        strcpy(st->call_error_msg, "no active signer -- open Wallet first");
        st->call_send_attempted = true; st->call_send_ok = false;
        st->phase = EXP_CALL_RESULT;
        return;
    }

    tezos_transaction_params_t params = {0};
    strcpy(params.source, source);
    strcpy(params.fee, "600");
    strcpy(params.gas_limit, "3000");
    strcpy(params.storage_limit, "300");
    strcpy(params.amount, "0");
    snprintf(params.destination, sizeof params.destination, "%s", st->address);
    params.entrypoint = ep->name;
    params.parameter_value = &values[val_root];

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
        /* real leak, same class as wallet.c's Send flow -- see that file's
           comment on tezos_forge_transaction's own partial-allocation-before-
           failure behavior */
        tezos_bytes_free(&content_bytes);
        strcpy(st->call_error_msg, "forge failed");
        st->call_send_attempted = true; st->call_send_ok = false;
        st->phase = EXP_CALL_RESULT;
        return;
    }
    tezos_bytes_t full; tezos_bytes_init(&full);
    if (!tezos_forge_operation_group(&full, branch_to_use, content_bytes.data, content_bytes.len)) {
        strcpy(st->call_error_msg, "forge_operation_group failed");
        tezos_bytes_free(&content_bytes);
        st->call_send_attempted = true; st->call_send_ok = false;
        st->phase = EXP_CALL_RESULT;
        return;
    }
    tezos_bytes_free(&content_bytes);

    char sig[128];
    if (!wallet_sign_operation(&full, sig, sizeof sig)) {
        strcpy(st->call_error_msg, "sign failed");
        tezos_bytes_free(&full);
        st->call_send_attempted = true; st->call_send_ok = false;
        st->phase = EXP_CALL_RESULT;
        return;
    }

    st->call_send_attempted = true;
    st->call_send_ok = tezos_rpc_inject_operation(settings_get_rpc_node(), full.data, full.len,
                                                  st->call_op_hash, sizeof st->call_op_hash);
    if (!st->call_send_ok) strcpy(st->call_error_msg, "broadcast failed");
    tezos_bytes_free(&full);
    st->phase = EXP_CALL_RESULT;
}

static void render_address_entry(explorer_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "EXPLORER", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "Address to look up:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 70, &TEZOS_FONT_BODY, &st->t9);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "LOOKUP", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_info(explorer_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "EXPLORER", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    int y = 42;
    if (!st->info_fetch_ok) {
        tezos_draw_text(fb, d, 8, y, "Couldn't reach TZKT, or this", &TEZOS_FONT_SMALL, TEZOS_AMBER); y += 18;
        tezos_draw_text(fb, d, 8, y, "isn't a real contract address.", &TEZOS_FONT_SMALL, TEZOS_AMBER); y += 22;
    } else {
        char line[100];
        snprintf(line, sizeof line, "%s", st->info.alias[0] ? st->info.alias : "(no alias)");
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_BODY, TEZOS_CYAN); y += 22;
        snprintf(line, sizeof line, "Balance: %s mutez", st->info.balance);
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
        snprintf(line, sizeof line, "Transactions: %d", st->info.num_transactions);
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 18;
        snprintf(line, sizeof line, "Last active: %.10s", st->info.last_activity_time[0] ? st->info.last_activity_time : "(unknown)");
        tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_MUTED); y += 22;
    }
    tezos_list_render(fb, d, 8, y, fb->width - 16, fb->height - y - 40, &TEZOS_FONT_BODY, &st->info_menu);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "OPEN", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_entrypoints(explorer_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "ENTRYPOINTS", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    if (!st->entrypoints_fetch_ok) {
        tezos_draw_text(fb, d, 8, 50, "Couldn't reach TZKT.", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    } else if (st->entrypoint_count == 0) {
        tezos_draw_text(fb, d, 8, 50, "No entrypoints found.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->entrypoints_list);
    }
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26,
                    "Calling an entrypoint isn't built yet", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_holdings(explorer_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "HOLDINGS", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    if (!st->holdings_fetch_ok) {
        tezos_draw_text(fb, d, 8, 50, "Couldn't reach TZKT.", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    } else if (st->holdings_count == 0) {
        tezos_draw_text(fb, d, 8, 50, "No token holdings found.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->holdings_list);
    }
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_call_leaf(explorer_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    char title[64];
    snprintf(title, sizeof title, "CALL: %s", st->entrypoints[st->calling_entrypoint_index].name);
    tezos_draw_text(fb, d, 8, 6, title, &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, leaf_kind_label(g_pending_leaf_kind), &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_rect(fb, d, 8, 76, fb->width - 16, 2, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 86, &TEZOS_FONT_BODY, &st->call_t9);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "NEXT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_call_list_count(explorer_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    char title[64];
    snprintf(title, sizeof title, "CALL: %s", st->entrypoints[st->calling_entrypoint_index].name);
    tezos_draw_text(fb, d, 8, 6, title, &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "How many items in this list?", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_rect(fb, d, 8, 76, fb->width - 16, 2, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 86, &TEZOS_FONT_BODY, &st->call_t9);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "NEXT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_call_result(explorer_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "CALL RESULT", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    if (st->call_send_ok) {
        tezos_draw_text(fb, d, 8, 50, "Call sent.", &TEZOS_FONT_BODY, TEZOS_CYAN);
        char line[100]; snprintf(line, sizeof line, "Op: %.30s...", st->call_op_hash);
        tezos_draw_text(fb, d, 8, 76, line, &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        tezos_draw_text(fb, d, 8, 50, "Call failed:", &TEZOS_FONT_BODY, TEZOS_PINK);
        tezos_draw_text(fb, d, 8, 76, st->call_error_msg, &TEZOS_FONT_SMALL, TEZOS_MUTED);
    }
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void screen_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    explorer_state_t *st = (explorer_state_t *)self->app_state;
    switch (st->phase) {
        case EXP_ADDRESS_ENTRY: render_address_entry(st, fb, d); break;
        case EXP_INFO: render_info(st, fb, d); break;
        case EXP_ENTRYPOINTS: render_entrypoints(st, fb, d); break;
        case EXP_HOLDINGS: render_holdings(st, fb, d); break;
        case EXP_CALL_LEAF: render_call_leaf(st, fb, d); break;
        case EXP_CALL_LIST_COUNT: render_call_list_count(st, fb, d); break;
        case EXP_CALL_RESULT: render_call_result(st, fb, d); break;
    }
}

static void screen_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    explorer_state_t *st = (explorer_state_t *)self->app_state;
    if (st->phase == EXP_ADDRESS_ENTRY) *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
    else if (st->phase == EXP_CALL_LEAF || st->phase == EXP_CALL_LIST_COUNT) {
        *needs_redraw = tezos_t9_tick(&st->call_t9, elapsed_ms);
    }
}

static tezos_action_t screen_input(tezos_screen_t *self, tezos_input_event_t ev,
                                   tezos_screen_t **next, bool *needs_redraw) {
    explorer_state_t *st = (explorer_state_t *)self->app_state;
    (void)next;

    if (st->phase == EXP_ADDRESS_ENTRY) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            const char *typed = tezos_t9_get_text(&st->t9);
            if (typed[0] == '\0') return TEZOS_ACTION_NONE;
            strncpy(st->address, typed, sizeof st->address - 1);
            perform_lookup(st);
            st->phase = EXP_INFO;
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == EXP_INFO) {
        if (tezos_list_handle_input(&st->info_menu, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = EXP_ADDRESS_ENTRY; *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            st->phase = st->info_menu.selected == 0 ? EXP_ENTRYPOINTS : EXP_HOLDINGS;
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == EXP_ENTRYPOINTS) {
        if (st->entrypoints_fetch_ok && st->entrypoint_count > 0 && tezos_list_handle_input(&st->entrypoints_list, ev)) {
            *needs_redraw = true; return TEZOS_ACTION_NONE;
        }
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = EXP_INFO; *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L && st->entrypoints_fetch_ok && st->entrypoint_count > 0) {
            int idx = st->entrypoints_list.selected;
            if (st->entrypoints[idx].type_root < 0) return TEZOS_ACTION_NONE; /* type unknown -- can't build a call for it, already labeled as such in the list */
            st->calling_entrypoint_index = idx;
            tezos_micheline_walker_init(&st->walker, &st->entrypoints[idx].type_pool, st->entrypoints[idx].type_root);
            tezos_walk_action_t action = advance_walker(st);
            if (action == TEZOS_WALK_DONE) {
                /* a real, if unusual, case: an entrypoint whose type needs
                   zero leaves at all (e.g. a bare unit) -- go straight to
                   confirmation, nothing to prompt for */
                char confirm_msg[128];
                snprintf(confirm_msg, sizeof confirm_msg, "Call %s on %.16s...? This cannot be undone.",
                        st->entrypoints[idx].name, st->address);
                *next = tezos_popup_confirm_get("Confirm Call", confirm_msg, "CALL", "CANCEL", &g_confirm_result);
                return TEZOS_ACTION_PUSH;
            }
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == EXP_HOLDINGS) {
        if (st->holdings_fetch_ok && st->holdings_count > 0 && tezos_list_handle_input(&st->holdings_list, ev)) {
            *needs_redraw = true; return TEZOS_ACTION_NONE;
        }
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = EXP_INFO; *needs_redraw = true; }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == EXP_CALL_LEAF || st->phase == EXP_CALL_LIST_COUNT) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = EXP_ENTRYPOINTS; *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            const char *typed = tezos_t9_get_text(&st->call_t9);
            if (typed[0] == '\0') return TEZOS_ACTION_NONE; /* real fields need a real value -- don't silently advance on empty input */
            bool submitted = st->phase == EXP_CALL_LEAF
                            ? tezos_micheline_walker_submit_leaf(&st->walker, typed)
                            : tezos_micheline_walker_submit_list_count(&st->walker, atoi(typed));
            if (!submitted) return TEZOS_ACTION_NONE; /* real validation failure (e.g. a non-numeric
                list count) -- stay on this screen rather than silently corrupt the walk */
            tezos_walk_action_t action = advance_walker(st);
            if (action == TEZOS_WALK_DONE) {
                char confirm_msg[128];
                snprintf(confirm_msg, sizeof confirm_msg, "Call %s on %.16s...? This cannot be undone.",
                        st->entrypoints[st->calling_entrypoint_index].name, st->address);
                *next = tezos_popup_confirm_get("Confirm Call", confirm_msg, "CALL", "CANCEL", &g_confirm_result);
                return TEZOS_ACTION_PUSH;
            }
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        *needs_redraw = tezos_t9_handle_input(&st->call_t9, ev);
        return TEZOS_ACTION_NONE;
    }

    /* EXP_CALL_RESULT */
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = EXP_ENTRYPOINTS; *needs_redraw = true; }
    return TEZOS_ACTION_NONE;
}

static void screen_on_resume(tezos_screen_t *self) {
    explorer_state_t *st = (explorer_state_t *)self->app_state;
    if ((st->phase == EXP_CALL_LEAF || st->phase == EXP_CALL_LIST_COUNT)
        && g_confirm_result == TEZOS_POPUP_CONFIRMED) {
        perform_call(st);
    }
    g_confirm_result = TEZOS_POPUP_PENDING;
}

tezos_screen_t *explorer_entry_screen(void) {
    memset(&g_state, 0, sizeof g_state);
    g_state.phase = EXP_ADDRESS_ENTRY;
    tezos_t9_init(&g_state.t9);

    g_explorer_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = screen_on_resume, .on_tick = screen_on_tick,
        .app_state = &g_state, .debug_name = "explorer",
    };
    return &g_explorer_screen;
}

tezos_screen_t *explorer_test_inject_entrypoint(const char *address, const char *entrypoint_name,
                                                const char *type_json) {
    memset(&g_state, 0, sizeof g_state);
    strncpy(g_state.address, address, sizeof g_state.address - 1);

    tezos_tzkt_entrypoint_t *ep = &g_state.entrypoints[0];
    strncpy(ep->name, entrypoint_name, sizeof ep->name - 1);
    ep->type_root = tezos_micheline_parse_type(type_json, &ep->type_pool);
    g_state.entrypoint_count = 1;
    g_state.entrypoints_fetch_ok = true;
    snprintf(g_state.entrypoints_labels[0], sizeof g_state.entrypoints_labels[0], "%s", ep->name);
    g_state.entrypoints_items[0] = (tezos_list_item_t){g_state.entrypoints_labels[0], NULL};
    tezos_list_init(&g_state.entrypoints_list, g_state.entrypoints_items, 1);

    g_state.phase = EXP_ENTRYPOINTS;

    g_explorer_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = screen_on_resume, .on_tick = screen_on_tick,
        .app_state = &g_state, .debug_name = "explorer",
    };
    return &g_explorer_screen;
}
