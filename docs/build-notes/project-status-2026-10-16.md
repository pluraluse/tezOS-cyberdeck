---
date: 2026-10-16
project: tezos-cyberdeck
type: build-note
tags: [status, milestone, real-hardware]
---

# Build Note: Project status — real hardware, real firmware, real apps

## Status: the headline milestone this project has been building toward since 2026-08-30 — real, confirmed, on actual Pi hardware

## The milestone

As of 2026-10-15/16, this device **boots real firmware, on a real Pi, through a real display, and real apps actually render and respond to real input.** Every piece of that sentence was unconfirmed before this session:

- Display: `piscreen` (the original overlay assumption) never worked at all; the actual working path (`fbtft,ili9481,dc_pin=24,reset_pin=25,rotate=0,bgr=0`) was found by live hardware trial, not guessed — see `display-bringup-build-notes.md`.
- Platform shim: `linux_shim.c` was written against libdrm/DRM-KMS, which this hardware doesn't have at all (confirmed: `fbtft` is a legacy fbdev driver, no `/dev/dri/*` node exists). Rewritten against fbdev — see `linux-shim-fbdev-rewrite-build-notes.md`.
- Boot splash: rendered on real hardware for the first time this project has ever had hardware access, including a live font swap (GROUT Display) confirmed by eye on the real panel — see `boot-splash-logo-font-build-notes.md`.
- Navigation and app rendering: no keypad hardware exists yet (M2), so a synthetic `/dev/uinput` keyboard (`tools/uinput-keypad/`) was used to drive real navigation: idle grid → **Wallet opened and displayed a real balance** (exercising the mock signer + HTTP + TZKT read path, not just rendering) → back to idle → **Settings opened**, a dense text screen, rendered correctly.
- Stability: 11+ minutes continuous uptime, ~0% idle CPU, no crashes.
- WiFi connects on this image (`wlan0` up, real IP) — resolves a previously-open blocker, though not yet re-confirmed from a cold boot.

None of this was true as of 2026-09-02, when physical hardware bring-up first stalled. It's true now.

## What's real vs. what's still mock, stated plainly

**Real and running:**
- Core render engine, window manager, input state machine (`src/core/`).
- Full chain layer: Base58, operation forging, ed25519 signing, BIP39/SLIP10 HD derivation, Michelson/Micheline encoding, TZKT reads, RPC broadcasting, Beacon (TZIP-10) crypto/message/QR/pairing, a spec-verified Matrix client (`src/chain/`) — all cross-checked against independent reference implementations (Taquito, trezor/python-mnemonic, satoshilabs/slips), not just internally self-consistent.
- Display + input pipeline on real hardware (this session).
- Real network calls from real hardware (Wallet's balance fetch, this session).

**Still the mock signer** (`tezos_mock_signer_*`, `src/chain/tezos_signer.h`) — explicitly, loudly marked **"NEVER use with a seed protecting real funds."** The real hardware signer (STM32F412, airgapped via UART with a mechanical disconnect switch) has its wire protocol built and cross-compiled (`signer-protocol-build-notes.md`) but has never been flashed to real signer hardware. This is the actual remaining trust-boundary gap, not a detail.

**Test coverage, verified today, not assumed**: `tests/run_all_tests.sh` — **72 suites, 679 individual checks, 0 failures.** (`tests/README.md` had drifted to claiming 30 and then 17 suites in the same file, contradicting its own opening line of 72 — corrected alongside this note; see below.)

## Self-tracking docs that had drifted, found and fixed this pass

Two files that exist specifically to keep this project's own state honest had themselves gone stale — worth naming, since it's the same failure class flagged elsewhere in this project's history (fabricated/unmaintained "settled fact" documents):

1. **`tests/README.md`** had three different, mutually contradictory suite counts in one file (72 in the header, 30 in a parenthetical, 17 in the run instructions), a stale `libdrm-dev` install dependency (removed this session — see `linux-shim-fbdev-rewrite-build-notes.md`), and a reference to a `COMPILE-AND-TEST-PLAN-v2.md` that doesn't exist anywhere in this repo. Fixed to the real, re-verified numbers (72 suites, 679 checks) and the real install deps.
2. **`docs/build-notes/README-build-notes-index.md`** listed 5 build-notes files. There are 25. Rebuilt to the real, current list.

## What's actually left — see `ROADMAP.md`'s new "Path to v1.0" section

Rather than duplicate it here, the full per-app/per-facet gap list and completion plan now lives at the top of `ROADMAP.md`. Short version: the app suite is further along than the milestone numbering (M0–M11) alone suggests — most apps are real, not stub — but three things are the real remaining gaps: the hardware signer (never flashed), real keypad/touch input (M2, still placeholder `evdev` paths), and the camera sensor (no physical camera wired, pipeline built against no real sensor).
