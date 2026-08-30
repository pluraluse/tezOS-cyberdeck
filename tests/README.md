# Tests

A permanent, reusable test suite covering everything verified throughout
this project's development — 21 test suites, 171 individual checks.
Not scratch files: these ship with the repo so anyone (including you,
on real hardware) can re-verify the same things that were checked during
development.

## Running everything

```bash
sudo apt update
sudo apt install -y build-essential libdrm-dev libsodium-dev \
    libcurl4-openssl-dev libcjson-dev libqrencode-dev libzbar-dev pkg-config

./tests/run_all_tests.sh
```

One command, builds and runs all 17 suites, prints a clear pass/fail
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
| `test_signer_protocol.c` | The Pi<->signer wire protocol (framing + a software simulation) |
| `test_network.c` | HTTP client + TZKT parsing (see live-call note above) |
| `test_rpc.c` | Operation broadcasting (RPC injection) — body/response format verified against a real working forge/sign/inject example |
| `test_matrix.c` | Matrix client, endpoint shapes verified against the official spec |
| `test_beacon_crypto.c` | Beacon (TZIP-10) crypto handshake, verified against real beacon-sdk source + independent PyNaCl output |
| `test_beacon_message.c` | Beacon message schema, verified against real beacon-sdk TypeScript interfaces |
| `test_beacon_qr.c` | Beacon QR pairing payload — real libqrencode->zbar round trip |
| `test_beacon_pair.c` | The full pairing flow tied together — two simulated peers exchanging a real message end-to-end |
| `test_t9.c` | T9 multi-tap text entry, including delay-based auto-commit |
| `test_popup.c` | Confirmation dialogs — verifies genuine overlay behavior, not a full-screen replacement |
| `test_letterbox.c` | Logical canvas letterboxing — what makes Pi HDMI out to a non-matching display (e.g. a 1080p monitor) show a correctly-proportioned UI instead of a stretched one; verifies the real-panel case is pixel-identical (this layer doesn't affect actual target hardware) |
| `test_wallet_screens.c` | Wallet navigation + the popup-gated Send flow |
| `test_import_recovery.c` | Recovery-phrase import via the real T9 + BIP39-wordlist UI (real 12-word phrase, real tap sequence) |
| `test_camera_scan.c` | Camera Scan Mode's real QR pipeline (via a "simulate scan" trigger, since no real camera driver exists yet) |
| `test_camera_pipeline.c` | Camera Capture Mode's real image pipeline — box-average downsampling (proven different from nearest-neighbor), programmatically-generated Bayer dither matrices, all 4 color-depth modes |
| `test_camera_capture_flow.c` | Capture Mode's real end-to-end UI flow — mode-select, cycling resolution/depth settings, triggering a real capture, viewing the result |

See `docs/build-notes/*.md` for the full reasoning and reference sources
behind each verification — this suite is the reproducible proof, the
build notes are the explanation.
