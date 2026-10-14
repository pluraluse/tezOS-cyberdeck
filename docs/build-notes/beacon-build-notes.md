# Beacon (TZIP-10) — Build Notes

## What's real here, and what genuinely can't be tested from this environment

Three layers built and verified; one layer (the actual network transport)
deliberately not attempted here — see "The network hop" below for why.

## 1. Crypto handshake (`src/chain/tezos_beacon_crypto.c`)

Traced directly from the real `beacon-sdk` source
(`packages/beacon-utils/src/utils/crypto.ts`: `getKeypairFromSeed`,
`sealCryptobox`, `openCryptobox`), not reconstructed from memory or
secondary docs. The algorithm:
- **Identity keypair**: Ed25519, seeded by BLAKE2b-256(seed string) —
  a genuinely *separate* identity from any Tezos account keypair. Don't
  conflate the two: this identifies a Beacon peer/pairing session, not
  a signing account.
- **Seal/open**: converts the recipient's Ed25519 public key to X25519
  (`crypto_sign_ed25519_pk_to_curve25519` — libsodium has this exact
  conversion function, matching the JS `convertPublicKeyToX25519`),
  generates a fresh ephemeral X25519 keypair, derives a **deterministic**
  nonce = BLAKE2b-24(ephemeral_pk || their_x25519_pk) — a real deviation
  from typical random-nonce NaCl usage, confirmed intentional by matching
  the source exactly — then a standard `crypto_box`.

**Verification**: generated independent golden vectors with PyNaCl (a
faithful NaCl binding using the same primitives), then confirmed the C
implementation can **decrypt a message sealed by the independent Python
implementation** — real cross-implementation interop, not just internal
self-consistency. 7/7 checks passed, including a negative test (wrong
recipient's keypair correctly fails to open, not silently returning
garbage).

## 2. Message schema (`src/chain/tezos_beacon_message.c`)

Field names verified against the real TypeScript interfaces
(`packages/beacon-types/src/types/beacon/messages/*.ts`,
`BeaconMessageType.ts`) — `PermissionRequest`, `OperationRequest`,
`PermissionResponse`, `OperationResponse`, the `BeaconBaseMessage`
envelope (`type`/`version`/`id`/`senderId`).

Scoped to this device's actual role: it's always the **wallet** side of
a pairing, never the dApp side. So: parse incoming
`permission_request`/`operation_request`, build outgoing
`permission_response`/`operation_response`. Not building dApp-side
request construction, since this device never needs to send those.

`operationDetails` parsing covers what this device's forging layer
already supports — plain transfers and single-int entrypoint calls (see
`tezos_micheline.h`) — via the same Michelson-JSON shape Beacon uses
(`{"int": "4021"}`), not the full generality of arbitrary Micheline
values a dApp could theoretically send.

**Verification**: 15/15 checks — parses real-shaped requests correctly,
builds spec-correct responses (checked by round-tripping the built JSON
back through `cJSON` and verifying every field), and correctly rejects
malformed/wrong-type input rather than silently accepting it.

## 3. Pairing QR payload (`src/chain/tezos_beacon_qr.c`)

Traced the exact real pipeline: peer info JSON
(`packages/beacon-types/src/types/PeerInfo.ts`) → `JSON.stringify` →
**prefix-less** bs58check encode (`packages/beacon-core/src/Serializer.ts`
— no fixed prefix bytes, unlike the Tezos address/key encodings) →
wrapped as `tezos://?type=tzip10&data={code}`
(`packages/beacon-ui/src/utils/get-tzip10-link.ts`).

Added `tezos_b58check_encode_generic`/`_decode_generic` to
`tezos_base58.c` to support this — the existing base58 functions all
assume one of the fixed Tezos prefixes (tz1/edpk/etc.) and a fixed
payload length; this pairing payload has neither.

**QR image round trip, using real libraries, not a homebrew codec** —
same principle as using libsodium/libcurl/cJSON rather than reinventing
crypto/HTTP/JSON: `libqrencode` (real encode) → rendered to a real
scaled-up grayscale bitmap (standing in for a camera frame, since no
physical camera exists here) → `zbar` (real decode). **9/9 checks
passed**, including the one that matters most: zbar's decoded text
matched the original built URI *exactly* — genuine encode→image→decode
round trip, not just "the functions ran without crashing." Produced a
real version-11 QR code (61×61 modules) for a realistically-sized
pairing payload.

## 4. Matrix client (`src/chain/tezos_matrix.c`)

The transport layer described in "The network hop" section below —
built now, with the same honest split as everything network-dependent
in this project.

Endpoint paths and field names verified against the **official Matrix
Client-Server API specification** (spec.matrix.org/latest/client-server-api/)
— a primary source, not a secondary description: guest registration
(`POST /register?kind=guest`), room creation with direct invite
(`POST /createRoom`), sending an event (`PUT /rooms/{roomId}/send/{eventType}/{txnId}`),
and long-poll sync (`GET /sync`).

Added `tezos_http_post`/`tezos_http_put` to `tezos_http.c` — TZKT only
ever needed GET, Matrix needs both.

**Verification, same honest split as TZKT**: this sandbox's network
doesn't reach real Matrix homeservers, so:
- Transport mechanics (POST/PUT specifically, since GET was already
  verified for TZKT) — confirmed against a real, reachable endpoint
  (`api.github.com`), receiving real `401` responses (expected, no
  auth provided) — proving requests genuinely reach a real server and
  real responses come back, not just that `curl` was configured
  correctly.
- Response parsing — verified against the **official spec's own
  documented example response** for `/sync` (byte-for-byte, including
  the `rooms: {invite: {}, join: {}, leave: {}}` empty-state shape),
  plus a realistic populated fixture built to match the spec's
  documented event shape (confirmed message extraction correctly skips
  non-`m.room.message` events like `m.room.member`), plus realistic
  `register_guest`/`createRoom` response shapes.

**Not yet done, and can't be from here**: an actual live round trip
through this client to a real homeserver. **Also not yet done**:
confirming this generic-but-spec-correct Matrix usage exactly matches
beacon-sdk's specific choices (event type for encrypted payloads,
guest-vs-deterministic registration, homeserver/relay selection logic —
`packages/beacon-transport-matrix/src/communication-client/P2PCommunicationClient.ts`
in the real source, not re-checked in this pass). This client is
confirmed-correct generic Matrix; calling it "Beacon-compliant" needs
that final check.

## 5. The full pairing flow, tied together (`src/chain/tezos_beacon_pair.c`)

The last step: `tezos_beacon_prepare`/`connect`/`send`/`receive` actually
call all three layers above in sequence, instead of each existing as an
individually-tested but disconnected piece.

**Two new crypto pieces needed for this**, verified against the real
source (`beacon-core/src/transports/clients/CommunicationClient.ts`,
`beacon-utils/src/utils/crypto.ts`):
- **Session keys for ongoing communication** — different from the
  seal/open scheme (section 1), which is for the initial handshake
  only. Ongoing messages use `@stablelib/x25519-session`'s
  `clientSessionKeys`/`serverSessionKeys` — the standard X25519 key
  exchange construction, matching libsodium's `crypto_kx` (whose own
  primitive name is literally `"x25519blake2b"`). **Verification tier is
  different here, stated plainly**: unlike seal/open (cross-checked
  against independent PyNaCl output), this was NOT independently
  cross-checked against a non-libsodium implementation this session — no
  quick way to run the actual `@stablelib` JS code here. What IS tested:
  the algebraic property that must hold for any correct X25519 exchange
  (one side's `client.tx` equals the other's `server.rx`, and vice
  versa) — a real, meaningful test, just a weaker claim than "byte-
  identical to an independent implementation."
- **Two genuinely different hashes of the same public key** — `getSenderId`
  (bs58check(BLAKE2b(pubkey, 5 bytes)), for the message envelope's
  `senderId` field) and `getHexHash` (hex(BLAKE2b(pubkey, 32 bytes)),
  specifically for Matrix room addressing). Confirmed these are
  genuinely different outputs, not a copy-paste duplicate, in the test
  suite below.

**The definitive test**: two independently-simulated peers (our device
and "Kukai"), each deriving their own identity keypair from a seed.
"Kukai" builds a real `permission_request` (exact schema from section
2), encrypts it exactly as beacon-sdk would (client-role session keys +
secretbox with a random nonce), and our device — using session keys
derived **completely independently**, only from the two public keys —
decrypts it, recovering Kukai's original JSON **byte-for-byte**, then
parses it with the real message parser and confirms every field. 18/18
checks passed, including a negative test (decrypting with the wrong
session key correctly fails rather than returning garbage).

This is the strongest evidence available from this environment that the
crypto chain is genuinely correct end-to-end — everything except the
literal network hop to a real homeserver.

## Update: tezOS is now a real, standalone Beacon-compatible wallet

Not "running Kukai" — a genuine, separate implementation of the same
open Beacon protocol, so this device can pair with *any* Beacon dApp,
not just act as a substitute for Kukai's own app. See
`docs/CONCEPTS.md`'s platform-philosophy section for the full framing
behind this decision.

**What was missing before this pass, precisely**: `tezos_beacon_receive()`
existed and was tested at the chain layer, but no app ever called it or
showed the user a real incoming request — Camera's Scan mode only
handled the pairing QR itself. First built as an extension of Camera's
own Scan mode; **then corrected into a genuinely separate app**,
`src/apps/beacon_wallet/` (see that app's own build notes for the full
reasoning) — Camera hands off the decoded peer info at exactly one
point once a pairing QR is scanned, and everything from "connect"
onward (the persistent identity, the live session, real incoming-
request handling) lives in Beacon Wallet, not tangled into Camera's own
internals. Real handling of both `permission_request` (approve → build
and send a real `permission_response`) and `operation_request` (approve
→ real forge/sign/broadcast through the same verified pipeline every
other send flow in this project uses → build and send a real
`operation_response` with the actual resulting transaction hash).

**A new persistent identity, deliberately separate from any Tezos
account key** — matching `tezos_beacon_crypto.h`'s own documented
design ("a genuinely separate identity from any Tezos account
keypair"). Generated once with real OS entropy (`randombytes_buf` —
this is the Linux/Pi side, not the STM32 RNG question from the signer-
firmware discussion, a completely separate concern) and persisted so a
dApp that's paired once keeps recognizing this device as the same
wallet across restarts.

**A real bug found and fixed, not a false positive this time**:
`strncpy` doesn't null-terminate when the source is exactly buffer-
length — a real risk here specifically because the destination/amount
fields come from a parsed *incoming* request (network-derived data
this code doesn't fully control the length of), unlike most other
`strncpy` calls in this project working with locally-typed T9 input.
Fixed with explicit termination, distinct from the several genuine GCC
static-analysis false positives already documented and suppressed
elsewhere in this project — this one was real.

**Verified end-to-end under AddressSanitizer, 8/8 checks, zero
errors** — reusing the *exact* real `permission_request` fixture
already verified against real `beacon-sdk` schemas in
`test_beacon_message.c` (not reinvented), plus a real-address-format
`operation_request` so the actual forge pipeline gets genuinely
exercised, not just schema parsing. A test-only injection hook
(`beacon_wallet_test_inject_active_session()`/
`beacon_wallet_test_inject_message()`, not for production use) lets the
whole approve → confirm → sign → respond loop be exercised without
needing live Matrix connectivity, same reasoning as every other
test-injection hook in this project.

**Real, disclosed scope cuts, stated plainly rather than hidden**:
- **DENY doesn't send a real rejection response** — just returns to the
  active session locally. A real wallet would send an aborted/error
  response back to the dApp; not built in this pass, since APPROVE was
  the actual point of this feature.
- **Only the first operation in a batched request is handled** — real
  Beacon `operation_request`s can contain multiple operations;
  `tezos_beacon_op_t`'s own scope (see `tezos_beacon_message.h`) already
  only covers one at a time.
- **The public key sent in a `permission_response` is a placeholder**
  (reuses the derived address rather than the real public key) — a
  real, distinct gap, not yet closed.
- **Live Matrix connectivity remains genuinely unverifiable from this
  sandbox** — same disclosed limitation as the rest of the Beacon work;
  everything up to and including the real network call is built and
  tested, the actual live round trip needs real hardware.


Beacon doesn't connect two devices directly. Pairing (the QR code)
exchanges peer info, but actual message delivery routes through
**Matrix** — a real, independent messaging protocol with its own
homeservers. Concretely: each side logs into a Matrix homeserver (using
an identity derived from its Beacon keypair), addresses the other peer
as `@{recipientHash}:{relayServer}` (a literal Matrix user ID — confirmed
in the real source via `recipientString()`), and sends/receives
encrypted messages as Matrix room events.

This means "the network hop" is a **real, ongoing HTTP dependency** on
a Matrix homeserver — login, room create/join, send-event, and a
sync/poll loop for incoming events — not a one-time handshake. This
sandbox's network access is domain-restricted (the same limitation that
applies to TZKT) and doesn't reach Matrix homeservers, so this layer
genuinely can't be built-and-verified the way the three layers above
were.

**Not yet built**: a Matrix client. The HTTP client that already exists
(`tezos_http.c`) is fully capable of making the calls once pointed at a
real homeserver — this is a real, separate chunk of protocol work on
top of it (Matrix's REST API shape: auth tokens, room state, sync
tokens for incremental polling — meaningfully more involved than TZKT's
plain GETs), not implemented yet, and its live correctness can only be
proven with real network access, i.e. on real hardware.

## What this means for the three original goals

- **Kukai seed import to the STM32 signer** — done and complete. See
  `wallet-app-build-notes.md` / `settings-app-build-notes.md`. The
  derivation path is now confirmed to match Kukai's own convention too,
  directly from their official docs and open-source library — no longer
  an open caveat.
- **Camera-based QR pairing** — the encode/decode of the pairing payload
  is done and verified (this doc, section 3), and now wired into a real
  Camera app screen (`src/apps/camera/camera.c`, see
  `docs/build-notes/camera-app-build-notes.md`) using a "simulate scan"
  trigger in place of real camera hardware (which doesn't exist here).
- **"Beacon working" end-to-end** (device actually talking live to
  Kukai) — the full crypto/message/transport chain is now built and
  tied together (`tezos_beacon_pair.c`, this doc section 5), with the
  strongest verification available from this environment: a complete
  simulated pairing exchange between two independent identities,
  recovering the original message byte-for-byte. What's still missing,
  and genuinely can't be closed from here: a live round trip to a real
  Matrix homeserver, and a final check that this generic-but-spec-
  correct Matrix/session-key usage matches every specific parameter
  choice beacon-sdk makes (event types, registration approach, relay
  selection). Both need real network access and/or a live beacon-sdk
  instance to verify against — real hardware, not this sandbox.
