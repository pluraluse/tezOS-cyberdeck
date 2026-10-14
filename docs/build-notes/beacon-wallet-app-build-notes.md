# Beacon Wallet App — Build Notes

## Status: real, standalone, genuinely separate from Camera

This app didn't start this way. The first version of "tezOS acts as a
real Beacon wallet" was built as an extension of Camera's Scan mode —
after all, Scan mode already handled the pairing QR, so it seemed
natural to extend it with connect/session/incoming-request handling
too. That was a real design mistake, caught and corrected in the same
session it was raised: bolting a wallet's signing logic onto a camera
app's internals is exactly the kind of tangling that makes a clean
boundary impossible later — for another wallet's team to recognize and
potentially adopt their own piece of it, for Temple or any other
Beacon wallet to slot in the same way, or even just for a person
reading this codebase to find where "the wallet" actually lives.

**The fix**: pull everything from "connect" onward out into its own
app, `src/apps/beacon_wallet/`. Camera's Scan mode narrows back down to
its real, actual job — scan and decode a real pairing QR — and hands
off the decoded peer info at exactly one point
(`beacon_wallet_screen_from_peer()`). Camera and Beacon Wallet meet at
that handoff, nothing more tangled than that.

## Naming — went back and forth, settled on protocol-generic for now

This is worth recording honestly rather than cleaning up after the
fact, since the back-and-forth itself is the real lesson. In order:

1. **First draft**: built as an extension of Camera's own Scan mode,
   no separate app at all.
2. **First correction**: pulled into its own app, kept deliberately
   unbranded/neutral ("Beacon Wallet"), reasoning that using Kukai's
   name without their involvement risked looking like unauthorized use
   of their identity.
3. **Second correction**: told that reasoning was backwards — the
   point was never to avoid saying Kukai's name, it was to avoid
   quietly building the same thing under tezOS's own generic label and
   erasing whose protocol/design work it represents. Renamed the
   visible UI to say "KUKAI" directly.
4. **Third correction, current state**: told that this still wasn't
   right — the actual relationship between this device and Kukai
   specifically needs a clearer shared understanding before deciding
   how (or whether) to represent them by name, not another guess.
   Reverted to the protocol-generic "Beacon Wallet" label. **Kukai has
   been removed from this project's production entirely** — no Kukai
   name or branding anywhere in the shipped app.

**What stays, and what this exercise actually confirmed**: Beacon
itself remains the real integration point for any wallet — the same
pairing/session/incoming-request handling this app does is exactly
what any Beacon-compatible wallet would need, and building it against
Kukai's real, published Beacon behavior is what confirmed the
architecture genuinely works and generalizes (to Temple, or anyone
else) rather than being something Kukai-specific. That result holds
regardless of the branding question, which is genuinely still open and
deliberately not decided here.

## What moved here, verbatim in spirit, adapted in name

Everything that used to live in Camera's `scan_screen_state_t`/
`scan_render`/`scan_input` for session handling now lives here instead:

- **A persistent Beacon identity** (`get_or_create_beacon_identity()`),
  deliberately separate from any Tezos account key (matching
  `tezos_beacon_crypto.h`'s own documented design), generated once with
  real OS entropy (`randombytes_buf`) and persisted so a dApp that's
  paired once keeps recognizing this device as the same wallet.
- **Real incoming-request classification and handling**
  (`classify_incoming_message`, `perform_permission_confirm`,
  `perform_operation_confirm`) — unchanged in substance from the
  Camera-embedded version, since the logic itself was already real and
  tested; only the home (a real app, not another app's internals) and
  the exposed test hooks' names (`beacon_wallet_test_inject_*` instead
  of `camera_test_inject_*`) changed.

## The real entry points

- **`beacon_wallet_screen_from_peer(peer)`** — the real, normal way
  this app gets used: Camera calls this once it's scanned and decoded
  a real pairing QR. Immediately attempts `tezos_beacon_prepare()` +
  `tezos_beacon_connect()` and lands on the session-active screen
  (or the real, disclosed "couldn't reach Matrix" state if that fails,
  same limitation as everywhere else Matrix is touched in this
  project).
- **`beacon_wallet_entry_screen()`** — the app-grid entry, for opening
  this directly without having just scanned a QR. Honest, not
  pretending: shows "no active session, pair via Camera > Scan first"
  rather than fabricating a connected state.

## Verified

8/8 checks under AddressSanitizer, zero errors — the exact real
`permission_request` fixture already verified against real `beacon-sdk`
schemas in `test_beacon_message.c` (not reinvented), plus a
real-address-format `operation_request` so the actual forge pipeline
gets genuinely exercised, not just schema parsing. Full stack compiles
clean at both `-O0` and `-O2`, and the `find`-based glob in the install
script and test runner picked up this entirely new app directory with
zero manual registration required — exactly the payoff of that earlier
structural fix.

## Real, disclosed scope cuts, unchanged from before the move

- **DENY doesn't send a real rejection response** — just returns to
  the active session locally. A real wallet would send an
  aborted/error response back to the dApp; not built in this pass.
- **Only the first operation in a batched request is handled** — real
  Beacon `operation_request`s can contain multiple operations;
  `tezos_beacon_op_t`'s own scope already only covers one at a time.
- **The public key sent in a `permission_response` is a placeholder**
  (reuses the derived address rather than the real public key) — a
  real, distinct gap, not yet closed.
- **Live Matrix connectivity remains genuinely unverifiable from this
  sandbox** — same disclosed limitation as the rest of the Beacon work.
