#pragma once
/*
 * PIN entry — a reusable screen for the one thing a PIN does in this
 * project right now: deriving the key that protects the saved recovery
 * phrase (tezos_store.h). This is deliberately NOT a general device-lock
 * system (boot-time gating, screen timeout, etc.) — that's a real,
 * separate feature with its own design questions; scoping this to
 * exactly what the encrypted store needs keeps the claim honest.
 *
 * Plain digit entry, not T9 multi-tap — a PIN is digits, not letters,
 * so there's no multi-tap ambiguity to resolve. NUM 0-9 appends
 * directly, HASH backspaces, SOFTKEY_L confirms, SOFTKEY_R cancels.
 * Digits are masked on screen (shown as a count of entered digits, not
 * the digits themselves) since this is exactly the kind of input a
 * shoulder-surfing observer shouldn't be able to read off the screen.
 */
#include "tezos_core.h"

typedef enum {
    TEZOS_PIN_PENDING,
    TEZOS_PIN_CONFIRMED,
    TEZOS_PIN_CANCELLED,
} tezos_pin_result_t;

#define TEZOS_PIN_MAX_LEN 16

/* Returns a configured PIN entry screen ready to push. `pin_out` is
   caller-owned (a stable buffer, at least TEZOS_PIN_MAX_LEN+1 bytes) --
   written to only when the user confirms; check *result_out ==
   TEZOS_PIN_CONFIRMED before reading it. `result_out` is set to
   TEZOS_PIN_PENDING immediately (matches tezos_popup.h's convention). */
tezos_screen_t *tezos_pin_entry_get(const char *title, char *pin_out, tezos_pin_result_t *result_out);
