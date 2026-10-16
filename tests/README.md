# Tests

A permanent, reusable test suite covering everything verified throughout
this project's development — **70 test suites, 670 individual checks**
(re-counted and confirmed 2026-10-16, then again same day after removing
`test_scanner.c`/`test_scanner_entrypoints.c` along with the now-orphaned
Scanner app — see `docs/build-notes/project-status-2026-10-16.md`; this
file had drifted to claiming 30 and then 17 suites elsewhere in itself
before that pass).
Not scratch files: these ship with the repo so anyone (including you,
on real hardware) can re-verify the same things that were checked during
development.

**Independently confirmed, not just claimed**: a separate engineer, in a
different Debian development environment with no other context on this
project, ran this exact suite via `COMPILE-AND-TEST-PLAN.md` and got
`21 passed, 0 failed` — matching this repo's own claim exactly at that
point in the project, not just "probably works elsewhere." The suite has
grown substantially since (21 → 70 suites) as more apps and chain
functionality were built and verified; `COMPILE-AND-TEST-PLAN.md` itself
stays current (no separate v2 file — there isn't one) and is the one to
re-run for a fresh independent check. The same original session also
independently confirmed the real target binary
(`src/shim-linux/linux_shim.c`) builds clean at `-O2` — see
`ROADMAP.md`'s M3 section for that note, and
`docs/build-notes/linux-shim-fbdev-rewrite-build-notes.md` /
`project-status-2026-10-16.md` for the later real-hardware runtime
verification (the original note's "fails gracefully with no `/dev/dri/*`"
line is about the old, since-replaced libdrm backend).

`fixtures/nmcli` is a mock `nmcli` binary (a bash script, checked into
the repo) standing in for real NetworkManager — see `test_wifi.c`/
`test_wifi_screen.c` below.

## Running everything

```bash
sudo apt update
sudo apt install -y build-essential libsodium-dev \
    libcurl4-openssl-dev libcjson-dev libqrencode-dev libzbar-dev pkg-config

./tests/run_all_tests.sh
```

One command, builds and runs all 70 suites, prints a clear pass/fail
summary at the end.

## Worth knowing before you run this on a Pi

`test_network.c` and `test_matrix.c` attempt **real live network calls**
(to `api.tzkt.io` and a generic reachable test endpoint respectively).
These couldn't be verified this way during original development — the
dev sandbox's network was domain-restricted and didn't reach those
endpoints. On a Pi with real network access, these tests become *more*
capable than they were when first built: if the live calls succeed,
that's a genuinely new verification this project didn't have before.
If they fail (no WiFi configured yet, etc.), that's expected and not a
code defect — the fixture-based checks in those same files still run
and still matter regardless.

## What each suite covers

| File | Covers |
|---|---|
| `test_base58.c` | Base58Check encode/decode, verified against a real Taquito golden vector |
| `test_forge.c` | Operation forging, verified byte-for-byte against Taquito's exact reference output |
| `test_signer.c` | Ed25519 signing, verified against Taquito's complete seed->signature test vector |
| `test_bip39_slip10.c` | BIP39 mnemonics + SLIP-0010 HD derivation, verified against official test vectors |
| `test_micheline.c` | Michelson/Micheline encoding, verified against Taquito's exact test vectors |
| `test_micheline_dynamic.c` | The shared dynamic entrypoint-calling engine — parses a standard Micheline type into a tree and builds a matching value, verified byte-for-byte identical to the equivalent built directly; includes the test that caught a real buffer-aliasing bug (two addresses in one `pair(address,address)` type) |
| `test_signer_protocol.c` | The Pi<->signer wire protocol (framing + a software simulation) |
| `test_network.c` | HTTP client + TZKT parsing (see live-call note above) |
| `test_tzkt_entrypoints.c` | Contract entrypoint discovery — real endpoint confirmed via multiple independent sources before writing code; attempts a real live call against the actual Liquidity Baking contract, falls back honestly to fixture-based checks (including a deliberately malformed response, proving graceful degradation) |
| `test_tzkt_entrypoint_types.c` | Real entrypoint parameter *types* (not just names) — attempts a real live call, verified against fixtures for both "the guessed response field name matched" and "it didn't, degrade gracefully to names-only" |
| `test_micheline_real_liquidity_baking.c` | The exact real response pasted verbatim from a live fetch of the actual Liquidity Baking contract — confirmed the field-name guess correct and caught a real gap (unsupported `timestamp` type) that synthetic fixtures never surfaced |
| `test_micheline_real_quipuswap.c` | The exact real response from the actual QuipuSwap DEX 2.0 factory (30 entrypoints) — added `list`/`key_hash` support and caught a real node-count capacity bug, both against real data, not guesses; also caught and fixed a real transcription mistake in the test data itself, rebuilt programmatically rather than re-typed |
| `test_micheline_real_teia_channels.c` | The exact real response from the actual Teia Channels contract (the one Messenger needs) — proves the fix for `option(list(...))`, a combination the original design couldn't build at all until a real architectural change unified list-handling into the main value builder |
| `test_rpc.c` | Operation broadcasting (RPC injection) — body/response format verified against a real working forge/sign/inject example |
| `test_matrix.c` | Matrix client, endpoint shapes verified against the official spec |
| `test_beacon_crypto.c` | Beacon (TZIP-10) crypto handshake, verified against real beacon-sdk source + independent PyNaCl output |
| `test_beacon_message.c` | Beacon message schema, verified against real beacon-sdk TypeScript interfaces |
| `test_beacon_qr.c` | Beacon QR pairing payload — real libqrencode->zbar round trip |
| `test_beacon_pair.c` | The full pairing flow tied together — two simulated peers exchanging a real message end-to-end |
| `test_beacon_wallet.c` | Real incoming Beacon request handling (approve → confirm → sign/respond) in the new standalone Beacon Wallet app — reuses the exact real `permission_request` fixture already verified in `test_beacon_message.c`; this is the test that caught a real `strncpy` null-termination bug on network-derived data |
| `test_t9.c` | T9 multi-tap text entry, including delay-based auto-commit |
| `test_boot_splash.c` | The boot splash's gleam-sweep timing and real skip mechanism — this is the test that caught a design bug where the splash tried to skip itself via `ACTION_POP`, which would have been a silent no-op on real hardware |
| `test_popup.c` | Confirmation dialogs — verifies genuine overlay behavior, not a full-screen replacement |
| `test_pin_entry.c` | PIN entry widget — confirm/cancel/backspace/empty-rejection |
| `test_store.c` | Encrypted key store — correct-PIN round trip, and both a wrong PIN and a tampered file failing cleanly via authenticated encryption |
| `test_device_name_persist_p1.c` / `_p2.c` | Device name persistence — two separate process invocations sharing only a file on disk, proving a real cross-restart recovery |
| `test_letterbox.c` | Logical canvas letterboxing — what makes Pi HDMI out to a non-matching display (e.g. a 1080p monitor) show a correctly-proportioned UI instead of a stretched one; verifies the real-panel case is pixel-identical (this layer doesn't affect actual target hardware) |
| `test_wallet_screens.c` | Wallet navigation + the popup-gated Send flow |
| `test_import_recovery.c` | Recovery-phrase import via the real T9 + BIP39-wordlist UI (real 12-word phrase, real tap sequence), now including the PIN-gated encrypted save — verified with real assertions (exact address match), not just "didn't crash" (an earlier version of this test had none) |
| `test_camera_scan.c` | Camera Scan Mode's real QR pipeline (via a "simulate scan" trigger, since no real camera driver exists yet) |
| `test_camera_pipeline.c` | Camera Capture Mode's real image pipeline — box-average downsampling (proven different from nearest-neighbor), programmatically-generated Bayer dither matrices, all 4 color-depth modes |
| `test_camera_capture_flow.c` | Capture Mode's real end-to-end UI flow — mode-select, cycling resolution/depth settings, triggering a real capture, viewing the result |
| `test_messenger_build_value.c` | Structural, level-by-level verification of the hand-built `write_message` value against its real confirmed type — caught a real stack-use-after-return bug under AddressSanitizer that a normal run didn't surface |
| `test_messenger_ui.c` | Messenger's real compose → confirm → send UI flow, same real-navigation-depth pattern as Scanner's own end-to-end test |
| `test_messenger_inbox_ui.c` | The real Inbox's list → detail → back navigation, tested under AddressSanitizer; uses a test-only injection hook to verify with actual message content since this sandbox can't reach live network data |
| `test_gallery_ui.c` | Gallery's real list → detail → back navigation, same AddressSanitizer + injection-hook rigor as Messenger's Inbox test |
| `test_wallet_send.c` | Direct unit tests for `wallet_tez_to_mutez()` — real money conversion logic, tested directly rather than trusted via UI, including real precision-limit rejection |
| `test_wallet_send_ui.c` | The full real Send flow — recipient entry → amount entry → confirm → sign, under AddressSanitizer + LeakSanitizer; this is the test that caught a real, systemic memory leak shared by Wallet, Messenger, Scanner, and Explorer |
| `test_address_book.c` | The core address book module — real validation via `tezos_forge_address`, real delete-and-shift correctness, real book-full rejection |
| `test_address_book_persist_p1.c` / `_p2.c` | Real cross-process persistence for saved contacts — write in one process, read back correctly in a completely separate one, same rigor as Settings' own two-part persistence test |
| `test_wallet_contacts.c` | The full real Contacts UI — Add Contact flow, real rejection of invalid addresses, and real "send to a saved contact" that jumps straight to amount entry with the address pre-filled |
| `test_tez_domain.c` | Real `.tez` name resolution — validation, hex-encoding checked by hand against a real worked example, and confirming the real network call fails cleanly offline |
| `test_wallet_tez_domain.c` | Real Wallet Send integration for `.tez` names — regression check for raw addresses, and the real, distinct error shown when the registry isn't configured |
| `test_settings_registry.c` | The Name Registry settings row — and the test that caught a real, pre-existing bug where several settings rows were unreachable via actual list navigation due to a stale hardcoded item count |
| `test_signer_apdu.c` | The signer's real, portable ISO 7816-4 APDU wire protocol — all four real short-form cases, malformed-input rejection, and the truncation-safety check on response building (a silently truncated signature would be a genuinely dangerous bug) |
| `test_signer_session.c` | The signer's real operation-assembly session layer — reassembling a complete operation across `INS_SIGN` chunks; this is the test that caught a real logic bug where the first version discarded a legitimately in-progress multi-chunk operation's bytes on its final chunk |
| `test_signer_rng.c` | The signer's real hardware RNG safety layer — built directly against the real August 2026 Coldcard incident, using the same register-mocking technique Coldcard's own engineers used to verify their fix. Confirms every path returns either a genuinely verified word or a clear failure signal, never a silent fallback |
| `test_snake.c` | Real game logic — movement, real 180-reversal prevention, real wall/self collision, real food-eating verified via deterministic steering. Caught two real test-design bugs (not product bugs) while writing it, both documented honestly in the build notes |
| `test_settings_snake.c` | Confirms Settings' About screen actually hands off to the real Snake screen and back, not just that Snake works in isolation |
| `test_icon_bitmap.c` | The shared multi-color icon rendering function — pixel-exact placement using a synthetic position-encoded icon, and real bounds-clipping in both directions at screen edges, plus real pixel-exact verification of the integer-scaled blit variant |
| `test_grid_nav.c` | The home screen's real grid navigation math — all 9 numpad positions, real wrap-around at both boundaries with different app counts, and out-of-range/unrelated-input rejection |
| `test_image_decode.c` | Real image decode (`stb_image.h`) verified against an actual generated test PNG with known per-quadrant colors — checks exact decoded pixel values, not just "decode didn't crash" — plus real IPFS URI resolution and rejection cases |
| `test_gallery_viewer.c` | The pannable 1:1 viewer's pan/clamp/blit mechanics, verified with a synthetic position-encoded image so blitted pixel values can be checked exactly against the real source position; caught two real memory leaks and one test-design bug before shipping |
| `test_discover.c` | The real address-browser flow — visiting an address, browsing real holdings with images, and real independent bookmark/browse actions. This is the test that caught a real design bug (browsing was wrongly gated behind bookmarking first) |
| `test_discover_suggested.c` | Discover's Suggested list — confirms real bookmarks still work unchanged, and that Suggested entries genuinely route to their exact, cross-verified address, not just present as inert list text |
| `test_explorer_ui.c` | Explorer's real address-entry → info → Entrypoints/Holdings → back navigation, tested under AddressSanitizer |
| `test_micheline_walker.c` | The interactive type-walker engine — 27 checks covering leaf ordering, the critical `pair(address,address)` aliasing case, real list handling, and the exact `option(list(address))` shape Messenger needed, now proven through the general walker |
| `test_explorer_call.c` | The full end-to-end entrypoint-calling flow — walking a real `pair(int,string)` type via T9, confirming, and reaching a real result; this is the test that caught the second real stack-use-after-return bug (in `tezos_popup.c`, latent in Messenger's and Scanner's confirm flows too) |
| `test_tzkt_token_balances.c` | FA2 token-balance query for Messenger's inbox groundwork — verified against a real, live-captured response, correctly excludes zero-balance historical entries |
| `test_tzkt_get_message.c` | `message_infos` big_map lookup — the key/value shape confirmed against a real live capture (a different contract's bigmap), field names from the real confirmed storage schema; honestly discloses the smaller remaining gap (no live value example for this specific big_map yet) |
| `test_tzkt_owned_tokens.c` | Gallery's owned-token query — verified against the same real live capture as `test_tzkt_token_balances.c`, this time also checking contract-alias and real token-name extraction |
| `test_tzkt_contract_info.c` | Explorer's activity/staleness signal — uses the exact real response captured live earlier this session for the real Liquidity Baking contract |
| `test_rpc_node_config.c` | RPC node config — real T9 edit through Settings, confirms the value actually updates (closes a real duplication: this used to be a hardcoded constant in both wallet.c and scanner.c) |
| `test_lock_change_pin.c` | Settings' change-PIN flow — verifies wrong-old-PIN correctly fails, and that a successful change genuinely re-encrypts (old PIN stops working, new PIN recovers the identical original data) |
| `test_wifi.c` | Real `nmcli` command construction, terse-output parsing (including nmcli's documented escaped-colon convention), and the real `fork()+execvp()` mechanism — verified against a mock `nmcli` (`fixtures/nmcli`), since no WiFi hardware/NetworkManager exists in a dev environment |
| `test_wifi_screen.c` | The real Settings Wi-Fi screen end-to-end — scan, select a network, enter a password, real connect attempt — through the actual UI against the same mock `nmcli` |
| `test_profiles_p1.c` / `_p2.c` | Multi-account Profiles — adding a named account via real T9 produces a genuinely different, independently-correct address; two separate process invocations sharing only a file on disk prove the previously-active account is genuinely restored, not reset to default |

See `docs/build-notes/*.md` for the full reasoning and reference sources
behind each verification — this suite is the reproducible proof, the
build notes are the explanation.
