#pragma once
/*
 * Settings app — first real consumer of the T9 widget (Device Name
 * editing), plus a real About screen. See
 * docs/build-notes/settings-app-build-notes.md for the full scope.
 *
 * Honest about what's real here: Device Name (full T9 edit + save) and
 * About (static info display) are real. Profiles, Lock/Passcode, Wi-Fi,
 * Storage, and Services all genuinely depend on backend pieces that
 * don't exist yet (the profile/PIN system, network layer) — those rows
 * push the generic stub screen rather than fake functionality that
 * isn't there.
 */
#include "../../core/tezos_core.h"
#include "../../chain/tezos_tzkt.h"

tezos_screen_t *settings_entry_screen(void);

/* Public accessor -- not just for testing. Messenger's profile display
   and Beacon's pairing handshake will both need to read the current
   device name eventually; this is the one place it's read from. */
const char *settings_get_device_name(void);

/* RPC node URL used for broadcasting -- was a hardcoded placeholder
   duplicated in both wallet.c and scanner.c (DEV_RPC_NODE in each,
   genuinely the same value but two separate constants that could drift).
   Now one real, persisted, user-editable setting. Returns a sensible
   compiled-in default if nothing's been configured yet. */
const char *settings_get_rpc_node(void);

/* Contract address Messenger sends to -- defaults to
   KT19VtkiKbg69tQW2iVkpRLc9h3mkS8y9MMw, confirmed by the user directly
   against TZKT's storage schema (see settings.c's definition for the
   full note). Still a real, user-editable setting rather than a bare
   constant, same reasoning as RPC Node. */
const char *settings_get_messenger_contract(void);
const char *settings_get_name_registry_contract(void);

/* Which network Wallet/Scanner/Messenger all query -- defaults to
   Mainnet. Added specifically to make real testing on a non-mainnet
   network (Shadownet) actually possible; every TZKT-querying app
   previously hardcoded TEZOS_NETWORK_MAINNET with no way to select
   anything else. */
tezos_network_t settings_get_network(void);
