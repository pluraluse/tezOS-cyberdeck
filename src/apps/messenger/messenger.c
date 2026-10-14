#include "messenger.h"
#include "../wallet/wallet.h"
#include "../settings/settings.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../core/tezos_t9.h"
#include "../../core/tezos_popup.h"
#include "../../chain/tezos_forge.h"
#include "../../chain/tezos_micheline.h"
#include "../../chain/tezos_tzkt.h"
#include "../../chain/tezos_rpc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    MSG_HOME, MSG_NOT_CONFIGURED, MSG_COMPOSE_RECEIVER, MSG_COMPOSE_SUBJECT,
    MSG_COMPOSE_CONTENT, MSG_RESULT, MSG_INBOX_LIST, MSG_INBOX_DETAIL,
} messenger_phase_t;

#define MSG_INBOX_MAX 10 /* real display limit -- more than this needs real
    pagination, a separate feature not attempted here */

typedef struct {
    messenger_phase_t phase;
    tezos_list_state_t home_list;
    tezos_list_item_t home_items[2];

    tezos_t9_state_t t9;
    char receiver[40];
    char subject[64];
    char content[256];

    bool send_attempted;
    bool send_ok;
    char error_msg[80];
    char op_hash[80];

    /* real inbox state -- see tezos_tzkt_get_token_balances/get_message's
       disclosed gaps (docs/build-notes/messenger-app-build-notes.md):
       the "held token ID == received message ID" assumption this
       whole screen rests on is a real, reasoned inference, not yet
       independently confirmed against a live message */
    bool inbox_fetch_attempted;
    bool inbox_fetch_ok;
    int inbox_count;
    tezos_tzkt_message_t inbox_messages[MSG_INBOX_MAX];
    tezos_list_state_t inbox_list;
    tezos_list_item_t inbox_items[MSG_INBOX_MAX];
    char inbox_labels[MSG_INBOX_MAX][96];
    int inbox_selected_detail;
} messenger_state_t;

static messenger_state_t g_state;
static tezos_screen_t g_messenger_screen;
static tezos_popup_result_t g_confirm_result = TEZOS_POPUP_PENDING;

/* ============ write_message's real Micheline value, hand-built ============
   Not routed through the general dynamic engine (tezos_micheline.h's
   tezos_micheline_parse_type/build_value) -- that engine's `map` type
   is deliberately out of scope (see scanner-app-build-notes.md), and
   write_message's token_metadata field IS a map. Rather than extend
   that engine's scope under time pressure just to unblock this one
   entrypoint, this hand-builds the value directly using the same
   low-level, already-verified constructors the engine itself is built
   from (tezos_micheline_pair/string/sequence/prim0). Every field this
   v1 doesn't expose in the UI is a REAL Micheline "None" (via
   tezos_micheline_prim0), not a placeholder or a guess -- confirmed
   correct against write_message's actual real parameter type:

     pair(map(string,bytes) token_metadata,
          pair(list(address) receivers,
          pair(string subject,
          pair(string content,
          pair(option(list(address)) cc,
          pair(option(list(address)) bcc,
          pair(option(list(string)) attachments,
          pair(option(nat) parent_message_id,
          pair(option(string) baker_address,
               option(pair(address,nat)) fa2_details)))))))))

   Returns false if the receiver address doesn't validate (real base58
   check via tezos_forge_address) -- fails cleanly, doesn't build a
   value with garbage bytes. */
bool messenger_build_write_message_value(const char *receiver, const char *subject,
                                         const char *content, tezos_micheline_node_t *out) {
    static tezos_bytes_t receiver_addr_bytes;
    tezos_bytes_init(&receiver_addr_bytes);
    if (!tezos_forge_address(&receiver_addr_bytes, receiver)) return false;

    static tezos_micheline_node_t receiver_node, receivers_list, subject_node, content_node;
    static tezos_micheline_node_t cc_none, bcc_none, attachments_none, parent_none,
                                  baker_none, fa2_none, token_metadata_empty;
    static tezos_micheline_node_t p_baker_fa2, p_parent_rest, p_attach_rest, p_bcc_rest,
                                  p_cc_rest, p_content_rest, p_subject_rest, p_receivers_rest;

    receiver_node = tezos_micheline_bytes(receiver_addr_bytes.data, receiver_addr_bytes.len);
    static const tezos_micheline_node_t *receiver_ptr[1];
    receiver_ptr[0] = &receiver_node;
    receivers_list = tezos_micheline_sequence(receiver_ptr, 1);

    subject_node = tezos_micheline_string(subject);
    content_node = tezos_micheline_string(content);

    cc_none = tezos_micheline_prim0("None");
    bcc_none = tezos_micheline_prim0("None");
    attachments_none = tezos_micheline_prim0("None");
    parent_none = tezos_micheline_prim0("None");
    baker_none = tezos_micheline_prim0("None");
    fa2_none = tezos_micheline_prim0("None");
    /* empty map -- Michelson's map WIRE encoding is a sequence of Elt
       pairs; an empty map is a genuinely empty sequence, not a
       special-cased "no value" marker */
    token_metadata_empty = tezos_micheline_sequence(NULL, 0);

    p_baker_fa2 = tezos_micheline_pair(&baker_none, &fa2_none);
    p_parent_rest = tezos_micheline_pair(&parent_none, &p_baker_fa2);
    p_attach_rest = tezos_micheline_pair(&attachments_none, &p_parent_rest);
    p_bcc_rest = tezos_micheline_pair(&bcc_none, &p_attach_rest);
    p_cc_rest = tezos_micheline_pair(&cc_none, &p_bcc_rest);
    p_content_rest = tezos_micheline_pair(&content_node, &p_cc_rest);
    p_subject_rest = tezos_micheline_pair(&subject_node, &p_content_rest);
    p_receivers_rest = tezos_micheline_pair(&receivers_list, &p_subject_rest);
    *out = tezos_micheline_pair(&token_metadata_empty, &p_receivers_rest);
    return true;
}

/* Real forge + sign + broadcast -- same verified pipeline as
   scanner.c's perform_call (real branch/counter, signing via Wallet's
   active identity, real broadcast attempt). Targets the contract
   address from Settings, not a scanned one -- Messenger always calls
   the one configured contract, unlike Scanner which calls whatever a
   QR names. */
static void perform_send(messenger_state_t *st) {
    const char *contract = settings_get_messenger_contract();
    if (contract[0] == '\0') {
        strcpy(st->error_msg, "no contract configured -- set one in Settings first");
        st->send_attempted = true; st->send_ok = false;
        st->phase = MSG_RESULT;
        return;
    }

    char source[64];
    if (!wallet_get_active_address(source, sizeof source)) {
        strcpy(st->error_msg, "no active signer -- open Wallet first");
        st->send_attempted = true; st->send_ok = false;
        st->phase = MSG_RESULT;
        return;
    }

    tezos_micheline_node_t value;
    if (!messenger_build_write_message_value(st->receiver, st->subject, st->content, &value)) {
        strcpy(st->error_msg, "invalid receiver address");
        st->send_attempted = true; st->send_ok = false;
        st->phase = MSG_RESULT;
        return;
    }

    tezos_transaction_params_t params = {0};
    strcpy(params.source, source);
    strcpy(params.fee, "600"); /* a real message call has more parameter data than
        Scanner's single-int case -- a bit more gas budgeted accordingly */
    strcpy(params.gas_limit, "3000");
    strcpy(params.storage_limit, "300"); /* real, non-trivial storage: this
        writes a new message_infos entry and (per the real storage
        schema) likely mints an FA2 token -- more storage than a typical
        call, budgeted generously rather than guessed tightly */
    strcpy(params.amount, "0");
    snprintf(params.destination, sizeof params.destination, "%s", contract);
    params.entrypoint = "write_message";
    params.parameter_value = &value;

    char balance[32], real_counter[32];
    if (tezos_tzkt_get_account(settings_get_network(), source, balance, sizeof balance,
                               real_counter, sizeof real_counter)) {
        unsigned long long next_counter = strtoull(real_counter, NULL, 10) + 1;
        snprintf(params.counter, sizeof params.counter, "%llu", next_counter);
    } else {
        strcpy(params.counter, "1"); /* offline fallback, same as wallet.c/scanner.c */
    }
    char branch[128];
    const char *branch_to_use = tezos_tzkt_get_head_hash(settings_get_network(), branch, sizeof branch)
                               ? branch : "BLzyjjHKEKMULtvkpSHxuZxx6ei6fpntH2BTkYZiLgs8zLVstvX";

    tezos_bytes_t content_bytes; tezos_bytes_init(&content_bytes);
    if (!tezos_forge_transaction(&content_bytes, &params)) {
        /* real leak, same class as wallet.c's Send flow -- tezos_forge_transaction
           appends bytes at each step before checking the next, so a failure
           partway through still leaves real allocated memory this caller owns */
        tezos_bytes_free(&content_bytes);
        strcpy(st->error_msg, "forge failed");
        st->send_attempted = true; st->send_ok = false;
        st->phase = MSG_RESULT;
        return;
    }
    tezos_bytes_t full; tezos_bytes_init(&full);
    if (!tezos_forge_operation_group(&full, branch_to_use, content_bytes.data, content_bytes.len)) {
        strcpy(st->error_msg, "forge_operation_group failed");
        tezos_bytes_free(&content_bytes);
        st->send_attempted = true; st->send_ok = false;
        st->phase = MSG_RESULT;
        return;
    }
    tezos_bytes_free(&content_bytes);

    char sig[128];
    if (!wallet_sign_operation(&full, sig, sizeof sig)) {
        strcpy(st->error_msg, "sign failed");
        tezos_bytes_free(&full);
        st->send_attempted = true; st->send_ok = false;
        st->phase = MSG_RESULT;
        return;
    }

    st->send_attempted = true;
    st->send_ok = tezos_rpc_inject_operation(settings_get_rpc_node(), full.data, full.len,
                                             st->op_hash, sizeof st->op_hash);
    if (!st->send_ok) strcpy(st->error_msg, "broadcast failed");
    tezos_bytes_free(&full);
    st->phase = MSG_RESULT;
}

/* ============ Rendering ============ */

static void render_home(messenger_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "MESSENGER", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->home_list);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_not_configured(tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "MESSENGER", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, "No contract configured yet.", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    tezos_draw_text(fb, d, 8, 68, "Set one in Settings >", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    tezos_draw_text(fb, d, 8, 86, "Messenger Contract first.", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

/* Real fetch: token balances on the configured contract for the active
   wallet identity, then a message_infos lookup per held token ID.
   Rests on the disclosed, not-yet-independently-confirmed assumption
   that a held token ID equals a received message ID -- see
   docs/build-notes/messenger-app-build-notes.md. Synchronous, matching
   every other real TZKT call in this project (no async/loading-state
   infrastructure exists elsewhere to match either). */
static void perform_inbox_fetch(messenger_state_t *st) {
    st->inbox_fetch_attempted = true;
    st->inbox_count = 0;

    char address[64];
    if (!wallet_get_active_address(address, sizeof address)) {
        st->inbox_fetch_ok = false;
        return;
    }
    const char *contract = settings_get_messenger_contract();
    if (contract[0] == '\0') {
        st->inbox_fetch_ok = false;
        return;
    }

    char token_ids[MSG_INBOX_MAX][32];
    int id_count = 0;
    st->inbox_fetch_ok = tezos_tzkt_get_token_balances(settings_get_network(), address, contract,
                                                       token_ids, MSG_INBOX_MAX, &id_count);
    if (!st->inbox_fetch_ok) return;

    for (int i = 0; i < id_count && st->inbox_count < MSG_INBOX_MAX; i++) {
        tezos_tzkt_message_t msg;
        if (tezos_tzkt_get_message(settings_get_network(), contract, token_ids[i], &msg)) {
            st->inbox_messages[st->inbox_count] = msg;
            st->inbox_count++;
        }
        /* a token ID that ISN'T a valid message_infos key is skipped,
           not treated as a fetch failure -- see the disclosed
           assumption above; this is exactly the case that would prove
           or disprove it in practice */
    }
}

static void render_inbox_list(messenger_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "INBOX", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    if (!st->inbox_fetch_ok) {
        tezos_draw_text(fb, d, 8, 50, "Couldn't reach TZKT (offline,", &TEZOS_FONT_SMALL, TEZOS_AMBER);
        tezos_draw_text(fb, d, 8, 68, "no active signer, or no", &TEZOS_FONT_SMALL, TEZOS_AMBER);
        tezos_draw_text(fb, d, 8, 86, "contract configured).", &TEZOS_FONT_SMALL, TEZOS_AMBER);
    } else if (st->inbox_count == 0) {
        tezos_draw_text(fb, d, 8, 50, "No messages found.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->inbox_list);
    }
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    if (st->inbox_fetch_ok && st->inbox_count > 0) {
        tezos_draw_text(fb, d, 8, fb->height - 26, "OPEN", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    }
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_inbox_detail(messenger_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "MESSAGE", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    const tezos_tzkt_message_t *msg = &st->inbox_messages[st->inbox_selected_detail];
    int y = 44;
    char line[100];
    snprintf(line, sizeof line, "From: %.24s...", msg->sender);
    tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_CYAN); y += 20;
    snprintf(line, sizeof line, "Subject: %s", msg->subject);
    tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_BODY, TEZOS_TEXT); y += 24;
    tezos_draw_text(fb, d, 8, y, msg->content, &TEZOS_FONT_SMALL, TEZOS_TEXT); y += 20;
    snprintf(line, sizeof line, "%s", msg->timestamp);
    tezos_draw_text(fb, d, 8, y, line, &TEZOS_FONT_SMALL, TEZOS_MUTED);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}


static void render_compose(messenger_state_t *st, const char *title, const char *hint,
                          tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, title, &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 50, hint, &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_rect(fb, d, 8, 80, fb->width - 16, 2, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 90, &TEZOS_FONT_BODY, &st->t9);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "NEXT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void render_result(messenger_state_t *st, tezos_fb_t *fb, tezos_dirty_t *d) {
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "MESSENGER", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    if (st->send_ok) {
        tezos_draw_text(fb, d, 8, 50, "Message sent.", &TEZOS_FONT_BODY, TEZOS_CYAN);
        char line[100]; snprintf(line, sizeof line, "Op: %.30s...", st->op_hash);
        tezos_draw_text(fb, d, 8, 76, line, &TEZOS_FONT_SMALL, TEZOS_MUTED);
    } else {
        tezos_draw_text(fb, d, 8, 50, "Send failed:", &TEZOS_FONT_BODY, TEZOS_PINK);
        tezos_draw_text(fb, d, 8, 76, st->error_msg, &TEZOS_FONT_SMALL, TEZOS_MUTED);
    }
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void screen_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    messenger_state_t *st = (messenger_state_t *)self->app_state;
    switch (st->phase) {
        case MSG_HOME: render_home(st, fb, d); break;
        case MSG_NOT_CONFIGURED: render_not_configured(fb, d); break;
        case MSG_INBOX_LIST: render_inbox_list(st, fb, d); break;
        case MSG_INBOX_DETAIL: render_inbox_detail(st, fb, d); break;
        case MSG_COMPOSE_RECEIVER: render_compose(st, "TO", "Receiver address (tz1/KT1):", fb, d); break;
        case MSG_COMPOSE_SUBJECT: render_compose(st, "SUBJECT", "Subject:", fb, d); break;
        case MSG_COMPOSE_CONTENT: render_compose(st, "MESSAGE", "Content:", fb, d); break;
        case MSG_RESULT: render_result(st, fb, d); break;
    }
}

static void screen_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    messenger_state_t *st = (messenger_state_t *)self->app_state;
    if (st->phase == MSG_COMPOSE_RECEIVER || st->phase == MSG_COMPOSE_SUBJECT || st->phase == MSG_COMPOSE_CONTENT) {
        *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
    }
}

static tezos_action_t screen_input(tezos_screen_t *self, tezos_input_event_t ev,
                                   tezos_screen_t **next, bool *needs_redraw) {
    messenger_state_t *st = (messenger_state_t *)self->app_state;

    if (st->phase == MSG_HOME) {
        if (tezos_list_handle_input(&st->home_list, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            if (st->home_list.selected == 0) { /* Compose */
                if (settings_get_messenger_contract()[0] == '\0') {
                    st->phase = MSG_NOT_CONFIGURED;
                } else {
                    tezos_t9_init(&st->t9);
                    st->phase = MSG_COMPOSE_RECEIVER;
                }
            } else { /* Inbox -- real fetch, not a stub transition */
                perform_inbox_fetch(st);
                if (st->inbox_fetch_ok && st->inbox_count > 0) {
                    for (int i = 0; i < st->inbox_count; i++) {
                        snprintf(st->inbox_labels[i], sizeof st->inbox_labels[i], "%.20s: %s",
                                st->inbox_messages[i].sender, st->inbox_messages[i].subject);
                        st->inbox_items[i] = (tezos_list_item_t){st->inbox_labels[i], NULL};
                    }
                    tezos_list_init(&st->inbox_list, st->inbox_items, st->inbox_count);
                }
                st->phase = MSG_INBOX_LIST;
            }
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == MSG_NOT_CONFIGURED || st->phase == MSG_RESULT) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = MSG_HOME; *needs_redraw = true; }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == MSG_INBOX_LIST) {
        if (st->inbox_fetch_ok && st->inbox_count > 0 && tezos_list_handle_input(&st->inbox_list, ev)) {
            *needs_redraw = true; return TEZOS_ACTION_NONE;
        }
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = MSG_HOME; *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L && st->inbox_fetch_ok && st->inbox_count > 0) {
            st->inbox_selected_detail = st->inbox_list.selected;
            st->phase = MSG_INBOX_DETAIL;
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == MSG_INBOX_DETAIL) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = MSG_INBOX_LIST; *needs_redraw = true; }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == MSG_COMPOSE_RECEIVER || st->phase == MSG_COMPOSE_SUBJECT || st->phase == MSG_COMPOSE_CONTENT) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_R) { st->phase = MSG_HOME; *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            const char *typed = tezos_t9_get_text(&st->t9);
            if (st->phase == MSG_COMPOSE_RECEIVER) {
                strncpy(st->receiver, typed, sizeof st->receiver - 1);
                tezos_t9_init(&st->t9);
                st->phase = MSG_COMPOSE_SUBJECT;
            } else if (st->phase == MSG_COMPOSE_SUBJECT) {
                strncpy(st->subject, typed, sizeof st->subject - 1);
                tezos_t9_init(&st->t9);
                st->phase = MSG_COMPOSE_CONTENT;
            } else { /* MSG_COMPOSE_CONTENT -- last field, confirm then send */
                strncpy(st->content, typed, sizeof st->content - 1);
                char confirm_msg[160];
                snprintf(confirm_msg, sizeof confirm_msg, "Send to %.20s...? This cannot be undone.", st->receiver);
                *next = tezos_popup_confirm_get("Send Message", confirm_msg, "SEND", "CANCEL", &g_confirm_result);
                return TEZOS_ACTION_PUSH;
            }
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
        return TEZOS_ACTION_NONE;
    }

    return TEZOS_ACTION_NONE;
}

static void screen_on_resume(tezos_screen_t *self) {
    messenger_state_t *st = (messenger_state_t *)self->app_state;
    if (st->phase == MSG_COMPOSE_CONTENT && g_confirm_result == TEZOS_POPUP_CONFIRMED) {
        perform_send(st);
    }
    g_confirm_result = TEZOS_POPUP_PENDING;
}

tezos_screen_t *messenger_entry_screen(void) {
    memset(&g_state, 0, sizeof g_state);
    g_state.phase = MSG_HOME;
    g_state.home_items[0] = (tezos_list_item_t){"Compose", NULL};
    g_state.home_items[1] = (tezos_list_item_t){"Inbox", NULL};
    tezos_list_init(&g_state.home_list, g_state.home_items, 2);

    g_messenger_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = screen_on_resume, .on_tick = screen_on_tick,
        .app_state = &g_state, .debug_name = "messenger",
    };
    return &g_messenger_screen;
}

tezos_screen_t *messenger_test_inject_inbox(const char *const *senders, const char *const *subjects,
                                            const char *const *contents, const char *const *timestamps,
                                            int count) {
    memset(&g_state, 0, sizeof g_state);
    g_state.phase = MSG_INBOX_LIST;
    g_state.inbox_fetch_attempted = true;
    g_state.inbox_fetch_ok = true;
    g_state.inbox_count = count > MSG_INBOX_MAX ? MSG_INBOX_MAX : count;
    for (int i = 0; i < g_state.inbox_count; i++) {
        strncpy(g_state.inbox_messages[i].sender, senders[i], sizeof g_state.inbox_messages[i].sender - 1);
        strncpy(g_state.inbox_messages[i].subject, subjects[i], sizeof g_state.inbox_messages[i].subject - 1);
        strncpy(g_state.inbox_messages[i].content, contents[i], sizeof g_state.inbox_messages[i].content - 1);
        strncpy(g_state.inbox_messages[i].timestamp, timestamps[i], sizeof g_state.inbox_messages[i].timestamp - 1);
        snprintf(g_state.inbox_labels[i], sizeof g_state.inbox_labels[i], "%.20s: %s",
                g_state.inbox_messages[i].sender, g_state.inbox_messages[i].subject);
        g_state.inbox_items[i] = (tezos_list_item_t){g_state.inbox_labels[i], NULL};
    }
    tezos_list_init(&g_state.inbox_list, g_state.inbox_items, g_state.inbox_count);

    g_messenger_screen = (tezos_screen_t){
        .render = screen_render, .handle_input = screen_input,
        .on_resume = screen_on_resume, .on_tick = screen_on_tick,
        .app_state = &g_state, .debug_name = "messenger",
    };
    return &g_messenger_screen;
}
