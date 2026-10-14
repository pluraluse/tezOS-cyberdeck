#pragma once
/*
 * Beacon Wallet — a real, standalone Beacon-compatible wallet app,
 * genuinely separate from Camera and from tezOS's own core identity.
 *
 * Kept protocol-generic, not branded as any specific wallet, for now.
 * Whether/how to represent a specific real wallet (Kukai or otherwise)
 * on this device is a real, open question that needs revisiting with
 * a clearer shared understanding before deciding either way — not
 * something to guess at again here. What building this against Kukai's
 * real, published Beacon behavior *did* confirm, concretely: the
 * architecture genuinely works, and it generalizes — the same
 * integration point (Beacon: pairing, session, incoming
 * permission/operation requests) is what any Beacon-compatible wallet
 * (Kukai, Temple, others) would use, not something specific to one of
 * them. That's the real, keepable result of this exercise, independent
 * of the branding question.
 *
 * Camera's Scan mode still owns the QR photograph->decode step (that's
 * a real camera-shaped job) and hands off the decoded peer info here
 * once a pairing QR is scanned -- this app owns everything from
 * "connect" onward: the persistent identity, the live session, and
 * real incoming-request handling (permission/operation requests,
 * approve->confirm->sign->respond). tezOS's own apps (Wallet, Camera)
 * and this Beacon-facing wallet meet at that one handoff point, not
 * tangled together.
 */
#include "../../core/tezos_core.h"
#include "../../chain/tezos_beacon_pair.h"

/* Called by Camera once a real pairing QR has been scanned and
   decoded -- takes over from there (connect, then a live session).
   This is the normal, real entry point into this app. */
tezos_screen_t *beacon_wallet_screen_from_peer(const tezos_beacon_peer_info_t *peer);

/* The app-grid entry point, for opening this app directly without
   having just scanned a pairing QR. Real, honest behavior: if there's
   no active session yet, says so plainly and points at Camera > Scan
   rather than pretending a session exists. */
tezos_screen_t *beacon_wallet_entry_screen(void);

/* Exposed specifically for direct testing (see
   tests/test_beacon_wallet.c) -- injects a real, active (simulated)
   session directly into this app's state, at the session-active
   phase, so incoming-request handling can be exercised without a real
   pairing handoff or live Matrix connectivity. NOT for production use. */
tezos_screen_t *beacon_wallet_test_inject_active_session(void);

/* Exposed specifically for direct testing -- classifies a decrypted
   Beacon message JSON (same real, already-verified schema as
   tests/test_beacon_message.c's own fixtures) and stores the result
   into whatever screen is currently active, exactly matching what a
   real incoming message would do once Matrix connectivity is real. */
bool beacon_wallet_test_inject_message(const char *json);
