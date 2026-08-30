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

tezos_screen_t *settings_entry_screen(void);

/* Public accessor -- not just for testing. Messenger's profile display
   and Beacon's pairing handshake will both need to read the current
   device name eventually; this is the one place it's read from. */
const char *settings_get_device_name(void);
