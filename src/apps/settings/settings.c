#include "settings.h"
#include "import_recovery.h"
#include "../snake/snake.h"
#include "../wallet/wallet.h"
#include "../../core/tezos_gfx.h"
#include "../../core/tezos_fonts.h"
#include "../../core/tezos_widgets.h"
#include "../../core/tezos_t9.h"
#include "../../core/tezos_pin_entry.h"
#include "../../chain/tezos_store.h"
#include "../../chain/tezos_wifi.h"
#include "../../chain/tezos_tzkt.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Device name now persists via the plain (non-encrypted -- a device
   nickname isn't sensitive, gating it behind a PIN would be poor UX
   for no real security benefit) store in tezos_store.h. Falls back to
   the hardcoded default if nothing's been saved yet (fresh
   install/first boot) or if the save file genuinely can't be read. */
#define DEVICE_NAME_DEFAULT "tezOS-Device"
#define DEVICE_NAME_STORE_PATH "/tmp/tezos_cyberdeck_settings.txt"
static char g_device_name[TEZOS_T9_MAX_LEN + 1] = DEVICE_NAME_DEFAULT;

/* RPC node URL -- was duplicated as a hardcoded DEV_RPC_NODE constant
   in both wallet.c and scanner.c (the same value, but two separate
   constants that could silently drift). Now one real, persisted,
   user-editable setting, in the same plain (non-sensitive) store as
   device name. */
#define RPC_NODE_DEFAULT "https://rpc.tzbeta.net"
static char g_rpc_node[TEZOS_T9_MAX_LEN + 1] = RPC_NODE_DEFAULT;

/* Confirmed by the user directly against TZKT
   (https://api.tzkt.io/v1/contracts/KT19VtkiKbg69tQW2iVkpRLc9h3mkS8y9MMw/storage/schema)
   as the actual source of the write_message/message_infos/teiacafe_fee
   data this app is built against -- resolves the two-candidate-address
   uncertainty from earlier in this project's history. Confirmed as
   "the address this real data came from," which is different from (and
   a real basis of confidence for, but not literally the same claim as)
   "confirmed as what Teia's production app currently calls" -- worth
   keeping that distinction honest rather than overclaiming. */
#define MESSENGER_CONTRACT_DEFAULT "KT19VtkiKbg69tQW2iVkpRLc9h3mkS8y9MMw"
static char g_messenger_contract[TEZOS_T9_MAX_LEN + 1] = MESSENGER_CONTRACT_DEFAULT;

/* Real .tez name resolution needs the real, DIRECT NameRegistry
   contract that holds the `records` bigmap -- NOT the documented
   stable proxy entry point (NameRegistry.CheckAddress), which this
   project's own resolution code deliberately doesn't resolve through
   (see tezos_tez_domain.h's own header comment for the full real
   reasoning: no live network access from this sandbox to verify that
   proxy's exact storage shape, and guessing felt like the wrong kind
   of confidence given this project's own history). Left honestly
   empty by default, same pattern as fields with no real default to
   protect -- needs a real, live-confirmed direct contract address
   entered here before .tez resolution can work at all. */
static char g_name_registry_contract[TEZOS_T9_MAX_LEN + 1] = "";

/* Network selector -- added specifically to make real testing on a
   non-mainnet network (Shadownet, requested for real engineer testing)
   actually possible. Every TZKT-querying app (Wallet, Scanner,
   Messenger) previously hardcoded TEZOS_NETWORK_MAINNET directly, with
   no way to select anything else -- a real gap, not a hypothetical
   one, caught precisely because real testing was requested. Defaults
   to mainnet, matching prior behavior exactly for anyone who hasn't
   changed it. */
static tezos_network_t g_network = TEZOS_NETWORK_MAINNET;
static const char *NETWORK_NAMES[3] = {"Mainnet", "Ghostnet", "Shadownet"};

/* ============ Device Name edit screen (real T9 consumer) ============ */

typedef struct { tezos_t9_state_t t9; } device_name_state_t;
static device_name_state_t g_device_name_state;
static tezos_screen_t g_device_name_screen;

static void device_name_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    device_name_state_t *st = (device_name_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "DEVICE NAME", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    tezos_draw_text(fb, d, 8, 50, "Shown in Messenger profile", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 68, "and Beacon pairing.", &TEZOS_FONT_SMALL, TEZOS_MUTED);

    tezos_draw_rect(fb, d, 8, 110, fb->width - 16, 2, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 120, &TEZOS_FONT_BODY, &st->t9);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SAVE", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void device_name_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    device_name_state_t *st = (device_name_state_t *)self->app_state;
    *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
}

static tezos_action_t device_name_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                               tezos_screen_t **next, bool *needs_redraw) {
    device_name_state_t *st = (device_name_state_t *)self->app_state;
    (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) { /* SAVE */
        const char *typed = tezos_t9_get_text(&st->t9);
        if (typed[0] != '\0') { /* don't save an empty name over a real one */
            strncpy(g_device_name, typed, TEZOS_T9_MAX_LEN);
            g_device_name[TEZOS_T9_MAX_LEN] = '\0';
            /* Save failure here (unwritable path) doesn't block using the
               new name THIS session -- same "still works now, just won't
               survive a restart" tradeoff as the signer store's save path. */
            tezos_store_save_plain(DEVICE_NAME_STORE_PATH, "device_name", g_device_name);
        }
        return TEZOS_ACTION_POP;
    }
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) { /* CANCEL -- discard edits */
        return TEZOS_ACTION_POP;
    }
    *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *device_name_screen_get(void) {
    tezos_t9_init(&g_device_name_state.t9);
    /* pre-fill with the current name so editing feels like editing, not
       starting from scratch -- t9_state_t's buffer is a plain field,
       fine to set directly from within this module */
    strncpy(g_device_name_state.t9.buffer, g_device_name, TEZOS_T9_MAX_LEN);
    g_device_name_state.t9.buffer[TEZOS_T9_MAX_LEN] = '\0';
    g_device_name_state.t9.length = (int)strlen(g_device_name_state.t9.buffer);

    g_device_name_screen = (tezos_screen_t){
        .render = device_name_render, .handle_input = device_name_handle_input,
        .on_resume = NULL, .on_tick = device_name_on_tick,
        .app_state = &g_device_name_state, .debug_name = "settings_device_name",
    };
    return &g_device_name_screen;
}

/* ============ RPC Node edit screen (same T9 pattern as Device Name) ============ */

typedef struct { tezos_t9_state_t t9; } rpc_node_state_t;
static rpc_node_state_t g_rpc_node_state;
static tezos_screen_t g_rpc_node_screen;

static void rpc_node_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    rpc_node_state_t *st = (rpc_node_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "RPC NODE", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    tezos_draw_text(fb, d, 8, 50, "Used by Wallet + Scanner to", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 68, "broadcast signed operations.", &TEZOS_FONT_SMALL, TEZOS_MUTED);

    tezos_draw_rect(fb, d, 8, 110, fb->width - 16, 2, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 120, &TEZOS_FONT_BODY, &st->t9);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SAVE", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void rpc_node_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    rpc_node_state_t *st = (rpc_node_state_t *)self->app_state;
    *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
}

static tezos_action_t rpc_node_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                            tezos_screen_t **next, bool *needs_redraw) {
    rpc_node_state_t *st = (rpc_node_state_t *)self->app_state;
    (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) { /* SAVE */
        const char *typed = tezos_t9_get_text(&st->t9);
        if (typed[0] != '\0') { /* don't save an empty URL over a real one */
            strncpy(g_rpc_node, typed, TEZOS_T9_MAX_LEN);
            g_rpc_node[TEZOS_T9_MAX_LEN] = '\0';
            tezos_store_save_plain(DEVICE_NAME_STORE_PATH, "rpc_node", g_rpc_node);
        }
        return TEZOS_ACTION_POP;
    }
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP; /* CANCEL */
    *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *rpc_node_screen_get(void) {
    tezos_t9_init(&g_rpc_node_state.t9);
    strncpy(g_rpc_node_state.t9.buffer, g_rpc_node, TEZOS_T9_MAX_LEN);
    g_rpc_node_state.t9.buffer[TEZOS_T9_MAX_LEN] = '\0';
    g_rpc_node_state.t9.length = (int)strlen(g_rpc_node_state.t9.buffer);

    g_rpc_node_screen = (tezos_screen_t){
        .render = rpc_node_render, .handle_input = rpc_node_handle_input,
        .on_resume = NULL, .on_tick = rpc_node_on_tick,
        .app_state = &g_rpc_node_state, .debug_name = "settings_rpc_node",
    };
    return &g_rpc_node_screen;
}

/* ============ Messenger Contract edit screen (real, same T9 pattern
   as RPC Node above) ============ */

typedef struct { tezos_t9_state_t t9; } messenger_contract_state_t;
static messenger_contract_state_t g_messenger_contract_state;
static tezos_screen_t g_messenger_contract_screen;

static void messenger_contract_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    messenger_contract_state_t *st = (messenger_contract_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "MESSENGER CONTRACT", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    tezos_draw_text(fb, d, 8, 50, "Confirmed against TZKT directly", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 68, "as the source of the message", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 86, "data this app is built for.", &TEZOS_FONT_SMALL, TEZOS_MUTED);

    tezos_draw_rect(fb, d, 8, 130, fb->width - 16, 2, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 140, &TEZOS_FONT_BODY, &st->t9);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SAVE", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void messenger_contract_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    messenger_contract_state_t *st = (messenger_contract_state_t *)self->app_state;
    *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
}

static tezos_action_t messenger_contract_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                                      tezos_screen_t **next, bool *needs_redraw) {
    messenger_contract_state_t *st = (messenger_contract_state_t *)self->app_state;
    (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) { /* SAVE -- empty IS a valid save here (means "not configured"),
        unlike RPC Node's "don't save empty over a real default" rule, since there's no real default to protect */
        const char *typed = tezos_t9_get_text(&st->t9);
        strncpy(g_messenger_contract, typed, TEZOS_T9_MAX_LEN);
        g_messenger_contract[TEZOS_T9_MAX_LEN] = '\0';
        tezos_store_save_plain(DEVICE_NAME_STORE_PATH, "messenger_contract", g_messenger_contract);
        return TEZOS_ACTION_POP;
    }
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP; /* CANCEL */
    *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *messenger_contract_screen_get(void) {
    tezos_t9_init(&g_messenger_contract_state.t9);
    strncpy(g_messenger_contract_state.t9.buffer, g_messenger_contract, TEZOS_T9_MAX_LEN);
    g_messenger_contract_state.t9.buffer[TEZOS_T9_MAX_LEN] = '\0';
    g_messenger_contract_state.t9.length = (int)strlen(g_messenger_contract_state.t9.buffer);

    g_messenger_contract_screen = (tezos_screen_t){
        .render = messenger_contract_render, .handle_input = messenger_contract_handle_input,
        .on_resume = NULL, .on_tick = messenger_contract_on_tick,
        .app_state = &g_messenger_contract_state, .debug_name = "settings_messenger_contract",
    };
    return &g_messenger_contract_screen;
}

/* ============ Name Registry Contract edit screen (real, same T9
   pattern as Messenger Contract above) ============ */

typedef struct { tezos_t9_state_t t9; } name_registry_contract_state_t;
static name_registry_contract_state_t g_name_registry_contract_state;
static tezos_screen_t g_name_registry_contract_screen;

static void name_registry_contract_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    name_registry_contract_state_t *st = (name_registry_contract_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "NAME REGISTRY", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    tezos_draw_text(fb, d, 8, 50, "The DIRECT NameRegistry", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 68, "contract (not the proxy) --", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_draw_text(fb, d, 8, 86, "needed for .tez resolution.", &TEZOS_FONT_SMALL, TEZOS_MUTED);

    tezos_draw_rect(fb, d, 8, 130, fb->width - 16, 2, TEZOS_MUTED);
    tezos_t9_render(fb, d, 8, 140, &TEZOS_FONT_BODY, &st->t9);

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SAVE", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("CANCEL", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "CANCEL", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void name_registry_contract_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    name_registry_contract_state_t *st = (name_registry_contract_state_t *)self->app_state;
    *needs_redraw = tezos_t9_tick(&st->t9, elapsed_ms);
}

static tezos_action_t name_registry_contract_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                                          tezos_screen_t **next, bool *needs_redraw) {
    name_registry_contract_state_t *st = (name_registry_contract_state_t *)self->app_state;
    (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) { /* SAVE -- empty IS a valid save (means
        "not configured yet"), same real reasoning as Messenger Contract */
        const char *typed = tezos_t9_get_text(&st->t9);
        strncpy(g_name_registry_contract, typed, TEZOS_T9_MAX_LEN);
        g_name_registry_contract[TEZOS_T9_MAX_LEN] = '\0';
        tezos_store_save_plain(DEVICE_NAME_STORE_PATH, "name_registry_contract", g_name_registry_contract);
        return TEZOS_ACTION_POP;
    }
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP; /* CANCEL */
    *needs_redraw = tezos_t9_handle_input(&st->t9, ev);
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *name_registry_contract_screen_get(void) {
    tezos_t9_init(&g_name_registry_contract_state.t9);
    strncpy(g_name_registry_contract_state.t9.buffer, g_name_registry_contract, TEZOS_T9_MAX_LEN);
    g_name_registry_contract_state.t9.buffer[TEZOS_T9_MAX_LEN] = '\0';
    g_name_registry_contract_state.t9.length = (int)strlen(g_name_registry_contract_state.t9.buffer);

    g_name_registry_contract_screen = (tezos_screen_t){
        .render = name_registry_contract_render, .handle_input = name_registry_contract_handle_input,
        .on_resume = NULL, .on_tick = name_registry_contract_on_tick,
        .app_state = &g_name_registry_contract_state, .debug_name = "settings_name_registry_contract",
    };
    return &g_name_registry_contract_screen;
}

/* ============ Network selector (real, list-select rather than T9 --
   a fixed set of 3 real choices, not free text) ============ */

typedef struct { tezos_list_state_t list; } network_state_t;
static network_state_t g_network_state;
static tezos_screen_t g_network_screen;
static tezos_list_item_t g_network_items[3];
static char g_network_labels[3][32];

static void network_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    network_state_t *st = (network_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "NETWORK", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, 44, "Affects Wallet/Scanner/Messenger", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    tezos_list_render(fb, d, 8, 68, fb->width - 16, fb->height - 108, &TEZOS_FONT_BODY, &st->list);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t network_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                           tezos_screen_t **next, bool *needs_redraw) {
    network_state_t *st = (network_state_t *)self->app_state;
    (void)next;
    if (tezos_list_handle_input(&st->list, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        g_network = (tezos_network_t)st->list.selected;
        char buf[8];
        snprintf(buf, sizeof buf, "%d", (int)g_network);
        tezos_store_save_plain(DEVICE_NAME_STORE_PATH, "network", buf);
        return TEZOS_ACTION_POP;
    }
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *network_screen_get(void) {
    for (int i = 0; i < 3; i++) {
        snprintf(g_network_labels[i], sizeof g_network_labels[i], "%s%s",
                 i == (int)g_network ? "> " : "  ", NETWORK_NAMES[i]);
        g_network_items[i] = (tezos_list_item_t){g_network_labels[i], NULL};
    }
    tezos_list_init(&g_network_state.list, g_network_items, 3);
    g_network_state.list.selected = (int)g_network;

    g_network_screen = (tezos_screen_t){
        .render = network_render, .handle_input = network_handle_input,
        .on_resume = NULL, .on_tick = NULL,
        .app_state = &g_network_state, .debug_name = "settings_network",
    };
    return &g_network_screen;
}


/* ============ Wi-Fi (real nmcli integration, see tezos_wifi.h) ============ */

typedef enum { WIFI_SCANNING, WIFI_LIST, WIFI_NO_NETWORKS, WIFI_ENTERING_PASSWORD, WIFI_RESULT } wifi_phase_t;

typedef struct {
    wifi_phase_t phase;
    char current_ssid[64];
    tezos_wifi_network_t networks[TEZOS_WIFI_MAX_NETWORKS];
    int network_count;
    tezos_list_state_t list;
    tezos_list_item_t list_items[TEZOS_WIFI_MAX_NETWORKS];
    char list_labels[TEZOS_WIFI_MAX_NETWORKS][128]; /* "SSID (SIGNAL%, SECURITY)" --
        sized generously (SSID up to 63 chars + security up to 31 chars +
        formatting) so snprintf never needs to truncate in practice */
    tezos_t9_state_t password_t9;
    bool connect_ok;
} wifi_state_t;
static wifi_state_t g_wifi_state;
static tezos_screen_t g_wifi_screen;

static void wifi_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    wifi_state_t *st = (wifi_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "WI-FI", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    if (st->current_ssid[0]) {
        char status[80];
        snprintf(status, sizeof status, "Connected: %s", st->current_ssid);
        tezos_draw_text(fb, d, 8, 36, status, &TEZOS_FONT_SMALL, TEZOS_CYAN);
    } else {
        tezos_draw_text(fb, d, 8, 36, "Not connected", &TEZOS_FONT_SMALL, TEZOS_MUTED);
    }

    const char *action_label = "SELECT";
    switch (st->phase) {
        case WIFI_SCANNING:
            tezos_draw_text(fb, d, 8, 60, "Scanning...", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            break;
        case WIFI_NO_NETWORKS:
            tezos_draw_text(fb, d, 8, 60, "No networks found.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, 78, "(nmcli not found/no", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, 96, "WiFi hardware here)", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            break;
        case WIFI_LIST:
            tezos_list_render(fb, d, 8, 56, fb->width - 16, fb->height - 96, &TEZOS_FONT_BODY, &st->list);
            break;
        case WIFI_ENTERING_PASSWORD:
            tezos_draw_text(fb, d, 8, 56, "Enter password:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_t9_render(fb, d, 8, 76, &TEZOS_FONT_BODY, &st->password_t9);
            action_label = "CONNECT";
            break;
        case WIFI_RESULT:
            tezos_draw_text(fb, d, 8, 60, st->connect_ok ? "CONNECTED" : "CONNECTION FAILED",
                            &TEZOS_FONT_BODY, st->connect_ok ? TEZOS_CYAN : TEZOS_PINK);
            break;
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    if (st->phase == WIFI_LIST || st->phase == WIFI_ENTERING_PASSWORD) {
        tezos_draw_text(fb, d, 8, fb->height - 26, action_label, &TEZOS_FONT_SMALL, TEZOS_CYAN);
    }
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void wifi_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    wifi_state_t *st = (wifi_state_t *)self->app_state;
    if (st->phase == WIFI_ENTERING_PASSWORD) *needs_redraw = tezos_t9_tick(&st->password_t9, elapsed_ms);
}

static tezos_action_t wifi_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                        tezos_screen_t **next, bool *needs_redraw) {
    wifi_state_t *st = (wifi_state_t *)self->app_state;
    (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;

    if (st->phase == WIFI_LIST) {
        if (tezos_list_handle_input(&st->list, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            tezos_wifi_network_t *net = &st->networks[st->list.selected];
            if (strcmp(net->security, "--") == 0) {
                /* open network -- no password needed, connect directly */
                st->connect_ok = tezos_wifi_connect(net->ssid, "");
                st->phase = WIFI_RESULT;
            } else {
                tezos_t9_init(&st->password_t9);
                st->phase = WIFI_ENTERING_PASSWORD;
            }
            *needs_redraw = true;
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == WIFI_ENTERING_PASSWORD) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            const char *password = tezos_t9_get_text(&st->password_t9);
            tezos_wifi_network_t *net = &st->networks[st->list.selected];
            st->connect_ok = tezos_wifi_connect(net->ssid, password);
            st->phase = WIFI_RESULT;
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        *needs_redraw = tezos_t9_handle_input(&st->password_t9, ev);
        return TEZOS_ACTION_NONE;
    }

    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *wifi_screen_get(void) {
    memset(&g_wifi_state, 0, sizeof g_wifi_state);
    tezos_wifi_current_connection(g_wifi_state.current_ssid, sizeof g_wifi_state.current_ssid);

    bool scan_ok = tezos_wifi_scan(g_wifi_state.networks, TEZOS_WIFI_MAX_NETWORKS, &g_wifi_state.network_count);
    if (!scan_ok || g_wifi_state.network_count == 0) {
        g_wifi_state.phase = WIFI_NO_NETWORKS;
    } else {
        for (int i = 0; i < g_wifi_state.network_count; i++) {
            snprintf(g_wifi_state.list_labels[i], sizeof g_wifi_state.list_labels[i], "%s (%d%%, %s)",
                     g_wifi_state.networks[i].ssid, g_wifi_state.networks[i].signal_percent,
                     g_wifi_state.networks[i].security);
            g_wifi_state.list_items[i].label = g_wifi_state.list_labels[i];
            g_wifi_state.list_items[i].secondary = NULL;
        }
        tezos_list_init(&g_wifi_state.list, g_wifi_state.list_items, g_wifi_state.network_count);
        g_wifi_state.phase = WIFI_LIST;
    }

    g_wifi_screen = (tezos_screen_t){
        .render = wifi_render, .handle_input = wifi_handle_input,
        .on_resume = NULL, .on_tick = wifi_on_tick,
        .app_state = &g_wifi_state, .debug_name = "settings_wifi",
    };
    return &g_wifi_screen;
}

/* ============ Profiles: multiple accounts under one seed ============
   Scoped deliberately: this is MULTIPLE ACCOUNTS derived from the ONE
   already-imported seed (the account index already supported by
   tezos_mock_signer_from_mnemonic/SLIP-0010, verified since the very
   first BIP39/SLIP-10 work in this project) -- NOT the broader
   "multiple separate seeds behind discreet PINs" vision from earlier
   project history. That's a real, separate, much bigger feature; this
   is the concrete, buildable subset of it. Account labels/indices are
   stored in the PLAIN (non-encrypted) store -- knowing "there's an
   account named Savings at index 1" isn't sensitive, only the seed
   itself is, and that's already protected by tezos_store.h. */

#define PROFILES_MAX_ACCOUNTS 8
#define PROFILES_STORE_PATH DEVICE_NAME_STORE_PATH /* same plain store file, different keys */

typedef enum { PROFILES_NO_MNEMONIC, PROFILES_LIST, PROFILES_ADDING } profiles_phase_t;

typedef struct {
    profiles_phase_t phase;
    char labels[PROFILES_MAX_ACCOUNTS][32];
    int account_count;
    int active_index;
    tezos_list_state_t list;
    tezos_list_item_t list_items[PROFILES_MAX_ACCOUNTS + 1]; /* +1 for "Add account" row */
    char list_labels[PROFILES_MAX_ACCOUNTS + 1][96];
    tezos_t9_state_t new_account_t9;
} profiles_state_t;
static profiles_state_t g_profiles_state;
static tezos_screen_t g_profiles_screen;

static void profiles_load(profiles_state_t *st) {
    char buf[16];
    st->account_count = tezos_store_load_plain(PROFILES_STORE_PATH, "account_count", buf, sizeof buf)
                       ? atoi(buf) : 0;
    if (st->account_count == 0) {
        /* first time Profiles has been opened for this seed -- account
           0 always exists implicitly (it's what import/unlock already
           derives), just not yet named/recorded */
        strcpy(st->labels[0], "Main");
        st->account_count = 1;
    } else {
        for (int i = 0; i < st->account_count && i < PROFILES_MAX_ACCOUNTS; i++) {
            char key[32];
            snprintf(key, sizeof key, "account_%d_label", i);
            if (!tezos_store_load_plain(PROFILES_STORE_PATH, key, st->labels[i], sizeof st->labels[i])) {
                snprintf(st->labels[i], sizeof st->labels[i], "Account %d", i);
            }
        }
    }
    st->active_index = tezos_store_load_plain(PROFILES_STORE_PATH, "active_account", buf, sizeof buf)
                      ? atoi(buf) : 0;
}

static void profiles_save_accounts(profiles_state_t *st) {
    char buf[16];
    snprintf(buf, sizeof buf, "%d", st->account_count);
    tezos_store_save_plain(PROFILES_STORE_PATH, "account_count", buf);
    for (int i = 0; i < st->account_count; i++) {
        char key[32];
        snprintf(key, sizeof key, "account_%d_label", i);
        tezos_store_save_plain(PROFILES_STORE_PATH, key, st->labels[i]);
    }
    snprintf(buf, sizeof buf, "%d", st->active_index);
    tezos_store_save_plain(PROFILES_STORE_PATH, "active_account", buf);
}

static void profiles_rebuild_list(profiles_state_t *st) {
    for (int i = 0; i < st->account_count; i++) {
        char addr[128];
        const char *addr_preview = "(unavailable)";
        char addr_buf[24];
        if (wallet_derive_account_address((uint32_t)i, addr, sizeof addr)) {
            snprintf(addr_buf, sizeof addr_buf, "%.12s...", addr);
            addr_preview = addr_buf;
        }
        snprintf(st->list_labels[i], sizeof st->list_labels[i], "%s%s (%s)",
                 i == st->active_index ? "> " : "  ", st->labels[i], addr_preview);
        st->list_items[i].label = st->list_labels[i];
        st->list_items[i].secondary = NULL;
    }
    strcpy(st->list_labels[st->account_count], "+ Add Account");
    st->list_items[st->account_count].label = st->list_labels[st->account_count];
    st->list_items[st->account_count].secondary = NULL;
    tezos_list_init(&st->list, st->list_items, st->account_count + 1);
}

static void profiles_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    profiles_state_t *st = (profiles_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "PROFILES", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    const char *action_label = "SELECT";
    switch (st->phase) {
        case PROFILES_NO_MNEMONIC:
            tezos_draw_text(fb, d, 8, 50, "Import or unlock a recovery", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_draw_text(fb, d, 8, 68, "phrase first.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            break;
        case PROFILES_LIST:
            tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->list);
            break;
        case PROFILES_ADDING:
            tezos_draw_text(fb, d, 8, 40, "Name this account:", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            tezos_t9_render(fb, d, 8, 60, &TEZOS_FONT_BODY, &st->new_account_t9);
            action_label = "SAVE";
            break;
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    if (st->phase != PROFILES_NO_MNEMONIC) {
        tezos_draw_text(fb, d, 8, fb->height - 26, action_label, &TEZOS_FONT_SMALL, TEZOS_CYAN);
    }
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void profiles_on_tick(tezos_screen_t *self, int elapsed_ms, bool *needs_redraw) {
    profiles_state_t *st = (profiles_state_t *)self->app_state;
    if (st->phase == PROFILES_ADDING) *needs_redraw = tezos_t9_tick(&st->new_account_t9, elapsed_ms);
}

static tezos_action_t profiles_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                            tezos_screen_t **next, bool *needs_redraw) {
    profiles_state_t *st = (profiles_state_t *)self->app_state;
    (void)next;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;

    if (st->phase == PROFILES_LIST) {
        if (tezos_list_handle_input(&st->list, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            if (st->list.selected == st->account_count) {
                /* "+ Add Account" row */
                if (st->account_count < PROFILES_MAX_ACCOUNTS) {
                    tezos_t9_init(&st->new_account_t9);
                    st->phase = PROFILES_ADDING;
                    *needs_redraw = true;
                }
            } else {
                /* switch to the selected account */
                if (wallet_switch_account((uint32_t)st->list.selected)) {
                    st->active_index = st->list.selected;
                    profiles_save_accounts(st);
                    profiles_rebuild_list(st);
                    *needs_redraw = true;
                }
            }
        }
        return TEZOS_ACTION_NONE;
    }

    if (st->phase == PROFILES_ADDING) {
        if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
            const char *typed = tezos_t9_get_text(&st->new_account_t9);
            if (typed[0] != '\0') {
                int new_index = st->account_count;
                strncpy(st->labels[new_index], typed, sizeof st->labels[0] - 1);
                st->account_count++;
                st->active_index = new_index;
                wallet_switch_account((uint32_t)new_index); /* derive + activate immediately */
                profiles_save_accounts(st);
                profiles_rebuild_list(st);
            }
            st->phase = PROFILES_LIST;
            *needs_redraw = true;
            return TEZOS_ACTION_NONE;
        }
        *needs_redraw = tezos_t9_handle_input(&st->new_account_t9, ev);
        return TEZOS_ACTION_NONE;
    }

    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *profiles_screen_get(void) {
    memset(&g_profiles_state, 0, sizeof g_profiles_state);
    if (!wallet_has_session_mnemonic()) {
        g_profiles_state.phase = PROFILES_NO_MNEMONIC;
    } else {
        profiles_load(&g_profiles_state);
        wallet_switch_account((uint32_t)g_profiles_state.active_index); /* restore last-active on entry */
        profiles_rebuild_list(&g_profiles_state);
        g_profiles_state.phase = PROFILES_LIST;
    }

    g_profiles_screen = (tezos_screen_t){
        .render = profiles_render, .handle_input = profiles_handle_input,
        .on_resume = NULL, .on_tick = profiles_on_tick,
        .app_state = &g_profiles_state, .debug_name = "settings_profiles",
    };
    return &g_profiles_screen;
}

/* ============ About screen (real, honest about mock values) ============ */

static void about_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    (void)self;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "ABOUT", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    struct { const char *label; const char *value; bool mock; } rows[] = {
        {"tezOS version",    "0.1.0-dev",           false},
        {"Signer firmware",  "not built yet",        true},
        {"Board",            "Pi Zero 2 W",          false},
        {"Display panel",    "ILI9486 3.5\"",        false},
        {"Storage free",     "-- (no storage layer)", true},
        {"Battery",          "-- (no PiSugar link)",  true},
        {"Wi-Fi",             "-- (no network layer)", true},
        {"Signer link",       "disconnected (mock)",  true},
        {"Device name",       g_device_name,          false},
    };
    int y = 40;
    for (size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); i++) {
        tezos_draw_text(fb, d, 8, y, rows[i].label, &TEZOS_FONT_SMALL, TEZOS_MUTED);
        int vw = tezos_text_width(rows[i].value, &TEZOS_FONT_SMALL);
        tezos_draw_text(fb, d, fb->width - vw - 8, y, rows[i].value, &TEZOS_FONT_SMALL,
                        rows[i].mock ? TEZOS_MUTED : TEZOS_TEXT);
        y += 22;
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "PLAY SNAKE", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t about_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                         tezos_screen_t **next, bool *needs_redraw) {
    (void)self; (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        /* real, discoverable, but deliberately not a main-app-grid entry --
           see docs/build-notes/snake-build-notes.md for the placement
           reasoning */
        *next = snake_entry_screen();
        return TEZOS_ACTION_PUSH;
    }
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t g_about_screen = {
    .render = about_render, .handle_input = about_handle_input,
    .on_resume = NULL, .on_tick = NULL,
    .app_state = NULL, .debug_name = "settings_about",
};

/* ============ Lock / Passcode: real change-PIN flow ============
   Scoped exactly as documented in wallet-app-build-notes.md when this
   was first flagged: the PIN's only job is protecting the saved
   recovery phrase (tezos_store.h), not a general device-lock system.
   This screen closes the "no change-PIN flow" gap -- previously the
   PIN could only ever be set once, at initial import. */

typedef enum {
    LOCK_NO_PHRASE,       /* nothing saved yet -- nothing to protect */
    LOCK_READY,           /* a phrase is saved; offer to change its PIN */
    LOCK_OLD_PIN_WRONG,   /* verification failed -- wrong PIN or corrupted file */
    LOCK_AWAITING_NEW_PIN,/* old PIN verified, now collecting the new one */
    LOCK_DONE,            /* new PIN saved successfully */
} lock_phase_t;

typedef struct {
    lock_phase_t phase;
    tezos_store_signer_data_t verified_data; /* populated once the old PIN verifies */
    char pin_buffer[TEZOS_PIN_MAX_LEN + 1];
    tezos_pin_result_t pin_result;
} lock_state_t;
static lock_state_t g_lock_state;
static tezos_screen_t g_lock_screen;

static void lock_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    lock_state_t *st = (lock_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "LOCK / PASSCODE", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);

    int y = 50;
    const char *action_label = "BACK";
    switch (st->phase) {
        case LOCK_NO_PHRASE:
            tezos_draw_text(fb, d, 8, y, "No recovery phrase saved yet.", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            y += 18;
            tezos_draw_text(fb, d, 8, y, "Import one first (Settings >", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            y += 18;
            tezos_draw_text(fb, d, 8, y, "Import Recovery Phrase).", &TEZOS_FONT_SMALL, TEZOS_MUTED);
            break;
        case LOCK_READY:
            tezos_draw_text(fb, d, 8, y, "A recovery phrase is saved,", &TEZOS_FONT_SMALL, TEZOS_TEXT);
            y += 18;
            tezos_draw_text(fb, d, 8, y, "protected by your PIN.", &TEZOS_FONT_SMALL, TEZOS_TEXT);
            y += 30;
            action_label = "CHANGE PIN";
            break;
        case LOCK_OLD_PIN_WRONG:
            tezos_draw_text(fb, d, 8, y, "Wrong PIN, or corrupted data.", &TEZOS_FONT_SMALL, TEZOS_PINK);
            y += 30;
            action_label = "RETRY";
            break;
        case LOCK_AWAITING_NEW_PIN:
            tezos_draw_text(fb, d, 8, y, "Current PIN verified.", &TEZOS_FONT_SMALL, TEZOS_CYAN);
            y += 18;
            tezos_draw_text(fb, d, 8, y, "Now enter a new PIN.", &TEZOS_FONT_SMALL, TEZOS_TEXT);
            y += 30;
            action_label = "SET NEW PIN";
            break;
        case LOCK_DONE:
            tezos_draw_text(fb, d, 8, y, "PIN changed successfully.", &TEZOS_FONT_SMALL, TEZOS_CYAN);
            break;
    }

    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    if (st->phase != LOCK_NO_PHRASE && st->phase != LOCK_DONE) {
        tezos_draw_text(fb, d, 8, fb->height - 26, action_label, &TEZOS_FONT_SMALL, TEZOS_CYAN);
    }
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static void lock_on_resume(tezos_screen_t *self) {
    lock_state_t *st = (lock_state_t *)self->app_state;
    if (st->pin_result != TEZOS_PIN_CONFIRMED) return; /* cancelled -- no state change */
    st->pin_result = TEZOS_PIN_PENDING;

    if (st->phase == LOCK_READY || st->phase == LOCK_OLD_PIN_WRONG) {
        /* this PIN entry was for the OLD/current PIN -- verify it by
           actually attempting a decrypt, not just accepting whatever
           was typed */
        if (tezos_store_load_signer(TEZOS_STORE_DEFAULT_SIGNER_PATH, st->pin_buffer, &st->verified_data)) {
            st->phase = LOCK_AWAITING_NEW_PIN;
        } else {
            st->phase = LOCK_OLD_PIN_WRONG;
        }
    } else if (st->phase == LOCK_AWAITING_NEW_PIN) {
        /* this PIN entry was for the NEW pin -- re-save the already-
           decrypted data under it */
        tezos_store_save_signer(TEZOS_STORE_DEFAULT_SIGNER_PATH, st->pin_buffer, &st->verified_data);
        st->phase = LOCK_DONE;
    }
}

static tezos_action_t lock_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                        tezos_screen_t **next, bool *needs_redraw) {
    lock_state_t *st = (lock_state_t *)self->app_state;
    (void)needs_redraw;
    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;
    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        if (st->phase == LOCK_READY || st->phase == LOCK_OLD_PIN_WRONG) {
            *next = tezos_pin_entry_get("CURRENT PIN", st->pin_buffer, &st->pin_result);
            return TEZOS_ACTION_PUSH;
        }
        if (st->phase == LOCK_AWAITING_NEW_PIN) {
            *next = tezos_pin_entry_get("NEW PIN", st->pin_buffer, &st->pin_result);
            return TEZOS_ACTION_PUSH;
        }
    }
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t *lock_screen_get(void) {
    memset(&g_lock_state, 0, sizeof g_lock_state);
    g_lock_state.phase = tezos_store_exists(TEZOS_STORE_DEFAULT_SIGNER_PATH) ? LOCK_READY : LOCK_NO_PHRASE;
    g_lock_screen = (tezos_screen_t){
        .render = lock_render, .handle_input = lock_handle_input,
        .on_resume = lock_on_resume, .on_tick = NULL,
        .app_state = &g_lock_state, .debug_name = "settings_lock",
    };
    return &g_lock_screen;
}

/* ============ Main Settings list ============ */

typedef struct { tezos_list_state_t list; } settings_home_state_t;
static settings_home_state_t g_settings_home_state;

static tezos_list_item_t g_settings_items[12] = {
    {"Device Name", g_device_name}, /* secondary points at the live buffer --
                                        stable address, contents update live */
    {"Profiles", NULL},
    {"Lock / Passcode", NULL},
    {"Wi-Fi", NULL},
    {"Storage", NULL},
    {"Services", NULL},
    {"About", NULL},
    {"Import Recovery Phrase", NULL}, /* real -- T9 + BIP39 wordlist candidate
                                          entry, verified with a full 12-word
                                          real-typed test; see import_recovery.c */
    {"RPC Node", g_rpc_node}, /* real -- was DEV_RPC_NODE duplicated in
                                 wallet.c and scanner.c, now one real setting */
    {"Messenger Contract", g_messenger_contract}, /* real -- deliberately no
        compiled-in default, since the real address isn't confirmed yet
        (see docs/build-notes/messenger-app-build-notes.md) */
    {"Name Registry", g_name_registry_contract}, /* real -- the DIRECT
        NameRegistry contract for .tez resolution, deliberately empty by
        default (see tezos_tez_domain.h) */
    {"Network", NULL}, /* real -- Mainnet/Ghostnet/Shadownet, affects every
        TZKT-querying app; added specifically to make real Shadownet
        testing possible, since every app previously hardcoded mainnet */
};

static void settings_home_render(tezos_screen_t *self, tezos_fb_t *fb, tezos_dirty_t *d) {
    settings_home_state_t *st = (settings_home_state_t *)self->app_state;
    tezos_draw_rect(fb, d, 0, 0, fb->width, fb->height, TEZOS_BG);
    tezos_draw_text(fb, d, 8, 6, "SETTINGS", &TEZOS_FONT_HEADER, TEZOS_PRIMARY);
    tezos_draw_rect(fb, d, 0, 28, fb->width, 2, TEZOS_PRIMARY);
    tezos_list_render(fb, d, 8, 40, fb->width - 16, fb->height - 80, &TEZOS_FONT_BODY, &st->list);
    tezos_draw_rect(fb, d, 0, fb->height - 34, fb->width, 2, TEZOS_PRIMARY);
    tezos_draw_text(fb, d, 8, fb->height - 26, "SELECT", &TEZOS_FONT_SMALL, TEZOS_CYAN);
    int backw = tezos_text_width("BACK", &TEZOS_FONT_SMALL);
    tezos_draw_text(fb, d, fb->width - backw - 8, fb->height - 26, "BACK", &TEZOS_FONT_SMALL, TEZOS_PINK);
}

static tezos_action_t settings_home_handle_input(tezos_screen_t *self, tezos_input_event_t ev,
                                                 tezos_screen_t **next, bool *needs_redraw) {
    settings_home_state_t *st = (settings_home_state_t *)self->app_state;
    if (tezos_list_handle_input(&st->list, ev)) { *needs_redraw = true; return TEZOS_ACTION_NONE; }

    if (ev.type == TEZOS_INPUT_SOFTKEY_R) return TEZOS_ACTION_POP;

    if (ev.type == TEZOS_INPUT_SOFTKEY_L) {
        switch (st->list.selected) {
            case 0: *next = device_name_screen_get(); return TEZOS_ACTION_PUSH; /* real */
            case 1: *next = profiles_screen_get(); return TEZOS_ACTION_PUSH; /* real */
            case 2: *next = lock_screen_get(); return TEZOS_ACTION_PUSH; /* real */
            case 3: *next = wifi_screen_get(); return TEZOS_ACTION_PUSH; /* real */
            case 6: *next = &g_about_screen; return TEZOS_ACTION_PUSH;          /* real */
            case 7: *next = import_recovery_entry_screen(); return TEZOS_ACTION_PUSH; /* real */
            case 8: *next = rpc_node_screen_get(); return TEZOS_ACTION_PUSH; /* real */
            case 9: *next = messenger_contract_screen_get(); return TEZOS_ACTION_PUSH; /* real */
            case 10: *next = name_registry_contract_screen_get(); return TEZOS_ACTION_PUSH; /* real */
            case 11: *next = network_screen_get(); return TEZOS_ACTION_PUSH; /* real */
            default: /* Storage, Services -- genuinely
                        blocked on backend pieces that don't exist yet */
                *next = tezos_stub_screen_get(g_settings_items[st->list.selected].label);
                return TEZOS_ACTION_PUSH;
        }
    }
    return TEZOS_ACTION_NONE;
}

static tezos_screen_t g_settings_home_screen = {
    .render = settings_home_render, .handle_input = settings_home_handle_input,
    .on_resume = NULL, .on_tick = NULL,
    .app_state = &g_settings_home_state, .debug_name = "settings_home",
};

/* Both accessors below need to reflect a persisted value even if
   Settings was never actually opened this session (e.g. configured in
   an earlier session, device restarted, user goes straight to Wallet).
   Loading only inside settings_entry_screen() would silently miss that
   case -- caught while adding the RPC node setting, same class of bug
   as everything this store exists to prevent. */
static bool g_settings_loaded = false;
static void ensure_settings_loaded(void) {
    if (g_settings_loaded) return;
    tezos_store_load_plain(DEVICE_NAME_STORE_PATH, "device_name", g_device_name, sizeof g_device_name);
    tezos_store_load_plain(DEVICE_NAME_STORE_PATH, "rpc_node", g_rpc_node, sizeof g_rpc_node);
    tezos_store_load_plain(DEVICE_NAME_STORE_PATH, "messenger_contract", g_messenger_contract, sizeof g_messenger_contract);
    tezos_store_load_plain(DEVICE_NAME_STORE_PATH, "name_registry_contract", g_name_registry_contract, sizeof g_name_registry_contract);
    char net_buf[8];
    if (tezos_store_load_plain(DEVICE_NAME_STORE_PATH, "network", net_buf, sizeof net_buf)) {
        int n = atoi(net_buf);
        if (n >= 0 && n <= 2) g_network = (tezos_network_t)n; /* bounds-check -- a corrupted/stale
            value shouldn't be cast into an out-of-range enum */
    }
    g_settings_loaded = true;
}

tezos_screen_t *settings_entry_screen(void) {
    ensure_settings_loaded();
    /* real fix for a real, pre-existing bug: this was hardcoded to 9,
       stale from when the array had fewer real rows -- RPC Node,
       Messenger Contract, Network, and Name Registry (indices 8-11)
       were all genuinely unreachable via the actual list navigation,
       since the list widget never knew they existed. Computed from
       the array's own real size now, so this can't silently drift
       out of sync again as more real rows get added. */
    tezos_list_init(&g_settings_home_state.list, g_settings_items,
                    (int)(sizeof(g_settings_items) / sizeof(g_settings_items[0])));
    return &g_settings_home_screen;
}

const char *settings_get_device_name(void) {
    ensure_settings_loaded();
    return g_device_name;
}

const char *settings_get_rpc_node(void) {
    ensure_settings_loaded();
    return g_rpc_node;
}

/* Returns the confirmed default (KT19VtkiKbg69tQW2iVkpRLc9h3mkS8y9MMw)
   unless the user has overridden it in Settings -- see
   MESSENGER_CONTRACT_DEFAULT's definition above for the confirmation
   note. */
const char *settings_get_messenger_contract(void) {
    ensure_settings_loaded();
    return g_messenger_contract;
}

/* Real, deliberately empty unless the user has set it -- see this
   file's own static declaration comment and tezos_tez_domain.h for
   the full reasoning: this needs the DIRECT NameRegistry contract,
   not the documented stable proxy, and no live network access exists
   from this sandbox to confirm that address with real confidence. */
const char *settings_get_name_registry_contract(void) {
    ensure_settings_loaded();
    return g_name_registry_contract;
}

tezos_network_t settings_get_network(void) {
    ensure_settings_loaded();
    return g_network;
}
