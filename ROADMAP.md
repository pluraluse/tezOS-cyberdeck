# Roadmap

Organic bring-up: each milestone must provably work before the next one
depends on it. Don't skip ahead.

## Path to v1.0 — what's left, app by app (2026-10-16)

The M0–M11 milestone numbering below tracks *bring-up sequence*, not
*completion* — a lot has been built ahead of its milestone slot (see
each phase's own notes). This section is the complementary view: for
each app and subsystem, real current status and exactly what closes it
out, grounded in the actual source tree and the real, re-verified test
suite (72 suites, 679 checks, 0 failures — see `tests/README.md`), not
assumption. Full narrative: `docs/build-notes/project-status-2026-10-16.md`.

### The three real gaps that actually block "done," in priority order

1. **The hardware signer has never been flashed.** The wire protocol
   (ISO 7816-4 APDU framing, session layer, RNG safety) is real, tested,
   cross-compiled for the STM32F412 — but the device still runs on
   `tezos_mock_signer_*`, explicitly marked *"NEVER use with a seed
   protecting real funds."* This is the actual trust boundary. Nothing
   else on this list matters if this doesn't close.
2. **No real keypad or touch hardware.** `linux_shim.c`'s `evdev` paths
   are still placeholders (M2) — tonight's navigation was driven by a
   synthetic `/dev/uinput` keyboard (`tools/uinput-keypad/`), not real
   hardware. TCA8418 has a mainline kernel driver (confirmed, M2's own
   notes), it just isn't wired and loaded yet.
3. **No real camera sensor.** Camera's full software pipeline (Scan
   mode, Capture mode, dither, 4 color-depth modes) is built and tested
   — against no real sensor. The screen literally says "no real camera
   sensor here" where live preview would go.

### Per-app status

| App | Status | What closes it out |
|---|---|---|
| **Wallet** | Real — forge/sign/broadcast, balance, address book, `.tez` domains, saved contacts, send flow, all verified against Taquito vectors | Domain/TZKT Info sub-pages are still placeholder pages (no action wired yet). Otherwise closes when the hardware signer does (gap #1 above) |
| **Explorer** | Real — address entry, entrypoint/holdings browsing, consolidates the old Scanner capability generically | No open gap found this pass |
| **Gallery** | Real — owned-token query, real image decode, pannable 1:1 viewer | No open gap found this pass |
| **Discover** | Real — address browsing, bookmarking, cross-verified Suggested list | No open gap found this pass |
| **Messenger** | Real — Channels, Token Gated Chat, Poll/Token Comments, compose/inbox UI | Never round-tripped through a real Matrix homeserver (needs real network access this project's dev sandbox didn't have) — worth doing now that WiFi is confirmed working on real hardware |
| **Camera** | Real software pipeline — Scan mode, Capture mode, box-average downsample, 4 color-depth modes | No real sensor wired (gap #3). Also: Capture Mode's scroll interaction for the two oversized presets (800×480, 640×480) isn't implemented; save/mint handoff is blocked on 1-bit Art not existing yet |
| **Beacon Wallet** | Real incoming-request handling (approve → confirm → sign/respond), kept protocol-generic | **Fixed 2026-10-16**: `perform_permission_confirm()` was substituting an address for the public key; now calls a real `wallet_get_active_public_key()` and fails cleanly (not silently wrong) if no signer is active |
| **1-bit Art** | Pure stub — zero lines of code, `app_entry_stub_art()` only | In progress 2026-10-16 — see below |
| **Settings** | Real for 10 of 12 rows (Device Name, Profiles, Lock/Passcode, Wi-Fi, About, Import Recovery, RPC Node, Messenger Contract, Name Registry, Network) | Storage and Services rows are explicit stubs — "genuinely blocked on backend pieces that don't exist yet" |
| **Snake** | Real — reachable from Settings' About screen by design, not a top-level grid app | No open gap |

**Scanner removed 2026-10-16** (was: orphaned, 417 lines, tested but unreferenced since Explorer superseded it — the Owner confirmed its presence was a wording mix-up, not an intentional keep). `scanner.c`/`.h`, its `STATUS.md`, and both its tests are gone; `scanner-app-build-notes.md` stays as historical record with a removed-banner.

### Core subsystem status

| Subsystem | Status | What closes it out |
|---|---|---|
| Hardware signer (STM32F412) | Wire protocol real/tested/cross-compiled; chip never flashed | Flash it, then re-run the signer test suite against real hardware, not the software simulation |
| Display | **Real, confirmed on hardware** (this session) — `fbtft,ili9481`, fbdev shim, color/mirror fixed in software | Final mounting orientation still deferred to enclosure design, not a driver question anymore |
| Keypad (TCA8418) | Mainline driver confirmed to exist; not wired | Physical wiring + device-tree overlay load, then point `linux_shim.c`'s hardcoded `evdev` path at the real device (same pattern `tools/uinput-keypad/README.md` documents for the synthetic stand-in) |
| Touch (XPT2046) | Not implemented at all — keypad-first per stated input priority | Not started; lower priority than keypad by explicit project decision |
| Camera sensor | Not wired | Physical hardware task, not software |
| WiFi | **Confirmed working** (this session) — `wlan0` up, real IP, real network calls made from Wallet | Re-confirm from a full cold boot, not just this session's warm state (flagged, not yet done) |
| Headless access (USB gadget / UART) | `g_serial` console is what every real-hardware test this session actually used — genuinely working, not just "boots" | The open items in `usb-gadget-build-notes.md` (UART wiring, `g_ether` networking) are arguably lower priority now that `g_serial` + WiFi both work |
| Boot-to-kiosk systemd service | `scripts/install-tezos-service.sh` exists, fixed this session (dropped stale libdrm) | Never actually installed and booted via systemd on real hardware — every test this session ran the binary manually over SSH, not via the real boot path |
| Kukai software wallet (genuine, kiosk browser) | Explicitly deferred to Stage 2 hardware (real GPU/DRM needed, confirmed absent on Stage 1) | Not actionable until Stage 2 — see `hardware-roadmap-build-notes.md` |

### Sequenced plan

1. ~~Fix the Beacon Wallet pubkey fallback bug~~ — **done 2026-10-16.**
2. ~~Decide Scanner's fate~~ — **removed 2026-10-16.**
3. **Flash the STM32F412 signer** and re-verify the real signer test suite against it — this is the one gap that makes "hardware wallet" literally true instead of aspirational.
4. **Wire the TCA8418 keypad** — turns every "confirmed via synthetic uinput" result this session into a real, hands-on-the-device result.
5. **Install and boot the systemd kiosk service for real** — closes the gap between "the binary runs when I SSH in and launch it" and "the device actually boots into this."
6. **Build 1-bit Art** (in progress 2026-10-16 — 1-bit *and* 8-bit color depth modes, see the app's own build note once it exists) — unblocks Camera's save/mint handoff.
7. **Wire the camera sensor**, **re-confirm WiFi from cold boot**, **round-trip Messenger through a real Matrix homeserver** — hardware/network confirmations now realistically possible given tonight's access, not blocked on anything structural.
8. Everything in the existing "Deferred / post-launch tier" below stays deferred — this list is about closing what's already in scope, not expanding scope.

---

## M0 — Base image bring-up
- Raspberry Pi OS Lite on the Pi Zero 2 W (fast bring-up; leaner custom
  image comes later, once software is proven).
- Confirm exact display driver IC (assume ILI9486 for the 3.5" 320x480
  panel) and touch controller (XPT2046) against the actual panel purchased.

## M1 — Display working
- `piscreen`-style DRM/KMS overlay loading correctly (not legacy fbtft —
  deprecated in kernel 5.4+). See `docs/build-notes/hardware-roadmap-build-notes.md`.
- Confirm 2x13 header pin conflict resolved for breadboard phase (jumpers
  now, wire harness later — no stacking header needed long-term).
- **Status: shim code written and compiles clean against real libdrm
  headers** (`src/shim-linux/linux_shim.c`) — DRM mode-setting, dumb
  buffer creation at RGB565 (matching `tezos_fb_t`'s native format),
  mmap, and dirty-rect-aware flip via `drmModeDirtyFB`. **Not yet
  runtime-verified** — no real DRM device exists outside actual Pi
  hardware. First real-hardware task: confirm the panel driver actually
  accepts RGB565 dumb buffers (noted as a real open question in the
  shim's comments — some tinydrm-class drivers only support XRGB8888).

**Real update (2026-10-15):** first live hardware session with real shell
access (USB `g_serial`, not UART). Full writeup:
`docs/build-notes/display-bringup-build-notes.md`. Short version: `piscreen`
(ILI9486, the BOM assumption) and the bundled Adafruit `pitft35-resistive`
(HX8357D) overlay both failed — the Adafruit one bound cleanly but produced
zero visual response at all. The generic `fbtft,ili9481` overlay, using DC/
RESET pins confirmed by decompiling `piscreen.dtbo` (GPIO24/25), produced
the first real pixel-level response this project has had, at `rotate=0`
only — `rotate=90`/`270` both broke buffer coverage and were abandoned.
Color/orientation correctness didn't survive repeated reboots consistently
through driver flags alone (`bgr` gave different results across boots on
the same config — likely a breadboard connection issue, not confirmed, not
chased further). **Decision: design around it instead of chasing root
cause** — `rotate=0` is locked in, R/B channel swap is done in software
(`tools/display-pattern/baseline_pattern.py`) rather than trusted to the
`bgr` flag, and buffer origin `(0,0)` is treated as a fixed reference point
whose physical corner is a mounting/enclosure decision, not a display-driver
question. Touch (XPT2046) is not wired into the current overlay at all yet.

**Real update (2026-10-15, continued):** `src/shim-linux/linux_shim.c` was
rewritten against fbdev instead of libdrm — the shim as originally written
could never have worked, since the working overlay (`fbtft,ili9481`)
creates a plain `/dev/fb1`, not a DRM device. Full writeup:
`docs/build-notes/linux-shim-fbdev-rewrite-build-notes.md`. Everything
above the display backend (logical/physical canvas, letterboxing,
dirty-rect, boot splash hand-off) needed no changes. Compiles and links
clean against the real `core`/`chain`/`apps` source tree for the first
time ever for this file; runs and fails correctly on a host with no real
panel.

**Real update (2026-10-15, continued) — ran on actual Pi hardware, and it
worked.** Boot splash rendered on real hardware for the first time this
project has ever had real hardware access. Found one real bug immediately
— everything mirrored left-right — fixed the same way as the color-order
issue (guaranteed in software in `fbdev_flip()`, not trusted to a
devicetree flag). Went further: built a synthetic `/dev/uinput` keyboard
(`tools/uinput-keypad/`, since no real keypad is wired yet — M2) and drove
real navigation — idle grid → Wallet (real balance displayed, exercising
the mock signer + HTTP + TZKT read path, not just rendering) → back to
idle → Settings (dense text list, a harder render test than icons, held
up correctly). 11+ minutes stable, no crashes, idle CPU near zero. Full
writeup: `docs/build-notes/linux-shim-fbdev-rewrite-build-notes.md`. M1's
core exit criterion (stable image on real hardware) is now substantively
met; remaining gaps are M2 (real keypad/touch hardware, not synthetic)
and a longer soak test.

## M2 — Input working
- TCA8418 keypad decoder wired over I2C: numeric matrix + 2 softkeys +
  dedicated Up/Down buttons, all through one interrupt line.
- XPT2046 touch confirmed working via mainline `ads7846`-compatible driver.
- **Status: confirmed mainline Linux has a real TCA8418 driver**
  (`drivers/input/keyboard/tca8418_keypad.c`, kernel 4.11+) — the keypad
  shows up as standard evdev once the device-tree overlay is loaded, no
  custom I2C code needed. See `hardware/pinouts/tca8418-overlay-notes.md`
  for the binding and the keycode mapping the shim expects. Touch (XPT2046)
  reading is stubbed in the shim but not yet implemented — keypad-first,
  per the project's stated input priority.

## M3 — "Hello framebuffer"
- Minimal `libdrm` test program: open device, set mode, paint a test pattern.
- Separate minimal evdev reader logging raw key/touch events.
- Two standalone programs, each proving one half of the physical loop,
  before any OS logic exists.
- **Superseded by M1/M5's combined work** — `src/shim-linux/linux_shim.c`
  does both of these directly against the real core engine rather than
  as two separate throwaway test programs, since the core engine already
  existed and was proven by the time this milestone was reached.
- **Independently reproduced outside the original dev environment.**
  Everything in this project up to this point had only ever been verified
  in the single sandbox it was built in — a real, if modest, limitation
  on how much confidence to place in "verified." A separate engineer,
  working from `COMPILE-AND-TEST-PLAN.md` in a different Debian
  development environment with no other context on the project,
  independently reproduced both core claims: the full permanent test
  suite (`tests/run_all_tests.sh`) passed all 21 suites, and the actual
  target binary (`src/shim-linux/linux_shim.c`, built with the exact
  `-O2` command that also ships in `scripts/install-tezos-service.sh`)
  compiled with zero warnings and, when run, printed the expected
  "DRM init failed" message and exited with status 1 — the correct,
  designed behavior for an environment with no real `/dev/dri/*`, not a
  defect. This is the first independent confirmation this project's
  test suite and build process behave identically outside the
  environment they were built in.

## M4 — Boot-to-kiosk
- Pi boots straight into a systemd service running the test binary — no
  login prompt, no desktop session. tezOS *is* the shell from day one.
- **Status: systemd unit and install script written**
  (`scripts/tezos.service`, `scripts/install-tezos-service.sh`) — waits
  on the DRM device node explicitly rather than a generic boot-complete
  target, restarts on failure. Not yet tested on real hardware (depends
  on M1/M2 being verified first). **A real staleness bug was caught and
  fixed here**: the install script's build command had drifted badly out
  of date — it only compiled the original 4 core files and would have
  failed to link on a real Pi, missing the entire chain layer and every
  real app screen added since. Fixed to match the actual current build
  command (same one `tests/run_all_tests.sh` and every full-stack
  verification in this project's history has used), and confirmed that
  exact command compiles clean at `-O2` (the install script's actual
  optimization level, different from the `-O0`-equivalent default used
  in ad-hoc dev builds — this caught two real `-O2`-only warnings in
  `tezos_popup.c`, since fixed).

## M5 — tezOS core skeleton
- DRM init + evdev init + drawing primitives (blit rect, bitmap-font text)
  + input dispatch loop. The foundation everything else builds on.
- **Status: core logic built and verified in software** — see
  `docs/architecture/CORE-ARCHITECTURE.md` and `src/core/`. The render
  API, screen/app/input state machine, shared list widget, T9 text-entry
  widget (now with real delay-based auto-commit via a new optional
  `on_tick` screen callback and `tezos_core_on_tick`, not just the
  different-key/Up commit paths — see CORE-ARCHITECTURE.md for the full
  writeup, including a real display-never-flips-after-first-frame bug
  in the shim that got caught and fixed while wiring this up), dirty-rect redraw strategy, real end-to-end PUSH/POP navigation, real body/small
  fonts (MonoMEK, confirmed CC0), and a `draw_icon` primitive with a real
  icon bitmap asset (MEK-Dings) are all implemented and proven working.
  What's left for this milestone: the icon-to-app mapping (a design
  decision — see `docs/design-system/ICONS.md`), MEKmode as an optional
  future swap for the display/header role (MonoMEK covers all roles fine
  for now — this is polish, not a gap), popups/alerts (layout/transitions), real DRM/evdev
  platform-shim code, and every app's actual screen (currently all push
  a generic placeholder stub — T9 exists as a widget now but isn't wired
  into any of them yet).

## M6 — First real screen
- Idle/home screen with T9 shortcut navigation. No network, no wallet, no
  signer required yet — fastest path to "this feels like a real device."

## M7 — Networking
- WiFi config + basic HTTP client wired into core. Unblocks Explorer/Wallet
  (TZKT calls).
- **Status: real HTTP client and TZKT read client built and verified.**
  `src/chain/tezos_http.c` (libcurl) — real transport mechanics verified
  against a live, reachable endpoint (this sandbox's network is
  domain-restricted and doesn't reach api.tzkt.io specifically, so that
  exact endpoint was verified differently: `src/chain/tezos_tzkt.c`'s
  field parsing — `hash`, `balance`, `counter` — was checked against
  genuine live API responses fetched directly, not guessed or
  fabricated). See `docs/build-notes/wallet-app-build-notes.md` for the
  captured fixtures and the real bug this caught (see below). **Wired
  into Wallet's actual Send flow**: `perform_send()` now attempts a real
  counter fetch and falls back to the old placeholder only if the
  network genuinely isn't reachable — this device may legitimately be
  offline, so that fallback isn't optional. Broadcasting a signed
  operation (a *different* endpoint — a Tezos RPC node's
  `/injection/operation`, not TZKT, which is read-only) is still not
  implemented. WiFi configuration UI still doesn't exist (Settings' Wi-Fi
  row still pushes the stub).

## M8 — Wallet + signer pairing
- Signer serial protocol (forged-op in → decode/display → signature out)
  working end to end with the STM32F401 or Pi Pico.
- **Status: protocol spec written and the protocol layer itself is built
  and verified in software** — see `docs/architecture/SIGNER-PROTOCOL.md`
  for the full framing/message-type spec, and `src/chain/tezos_signer_protocol.c`
  for the implementation (frame encode/decode, corruption detection via
  checksum, and a software simulation of the signer's request/response
  loop). Verified with 15 checks: framing round-trips correctly for
  every message type, a corrupted frame is correctly rejected, and a
  full simulated GET_ADDRESS → SIGN_REQUEST round trip produces a
  signature that matches the direct-API path exactly and genuinely
  self-verifies — reusing the same forge/sign pipeline already verified
  against Taquito's golden vectors, just reached through the protocol
  layer instead of called directly. **Explicitly documented simulation
  gap, not a real-firmware shortcut**: the simulation auto-confirms
  every `SIGN_REQUEST` immediately, since there's no physical button in
  software — real firmware must implement an actual wait-for-
  confirmation step, and the "user rejected" error path needs to be
  real handling, not a case that can never occur.
- Device becomes an actual wallet at this point, not just a UI shell.
  Still needed: real UART transport (this is all in-memory byte buffers
  right now), and actual STM32/Pico firmware implementing the spec —
  both genuinely need real hardware in the loop.

## M9 — Core app suite v1
- Explorer, Scanner (generic entrypoint decode/fill/forge), Gallery (owned
  tokens via TZKT), Settings (profiles, WiFi, storage, About), Camera
  (capture + scan modes).
- Wallet gains: delegation/staking, `.tez` domain resolution in Send.
- **Status: Wallet's home + Send-confirm screens are real** (`src/apps/wallet/`),
  backed by a genuinely implemented and rigorously verified operation
  forging + signing layer (`src/chain/` — see
  `docs/build-notes/wallet-app-build-notes.md`). Forging and signing are
  verified byte-for-byte against Taquito's own reference test vectors,
  not just "doesn't crash." Michelson entrypoint-call parameter encoding
  is now real too (`src/chain/tezos_micheline.c`, verified against
  Taquito's exact test vectors and an independent cross-check of the
  signed-zarith algorithm — see `docs/build-notes/wallet-app-build-notes.md`),
  unblocking Scanner/HEN-minting/Messenger's actual purpose, though none
  of those screens are built yet. **Broadcasting is now real too**
  (`src/chain/tezos_rpc.c`, `POST /injection/operation`, verified against
  a real working forge/sign/inject example — see
  `docs/build-notes/wallet-app-build-notes.md`), wired into both Send
  and Delegate — as are the real account counter and real branch (head
  hash), both via TZKT, both falling back to the old placeholders only
  if the network genuinely isn't reachable. **Update: Send's recipient
  and amount are real now too** (T9 entry, real tez-to-mutez conversion,
  unit-tested directly since a wrong conversion means sending the wrong
  amount of real money) — this was Wallet's oldest, most central open
  gap, closed alongside a real, systemic memory leak found and fixed
  across four files (Wallet, Messenger, Scanner, Explorer all shared
  the identical gap) — see
  `docs/build-notes/wallet-app-build-notes.md` for the full writeup.
  Still placeholder: balance display, and Delegate's baker-address
  field (a real, separate, still-unwired gap, deliberately not touched
  by the Send work). **Update: Gallery is real now** — see its own
  section further down; Explorer is real too, with its own section further down.
- **Update: Scanner is consolidated into Explorer** (see Explorer's own
  section further down, and `docs/build-notes/explorer-app-build-notes.md`)
  — the real use case (recovering assets from a project whose website
  is already gone) doesn't need a QR code from a site that no longer
  exists, just the contract address. Scanner's real code is kept, not
  deleted, pending folding its QR-input path into Explorer as an
  alternate method — not done yet. The section below describes
  Scanner's real capability as it was built, kept for historical
  accuracy.
- **Scanner — real, "generic entrypoint decode/fill/forge" delivered,
  with one important honest caveat.** QR-based call-spec input (real
  `libqrencode`/`zbar`, the same pipeline
  already verified for Beacon/Camera), real Michelson entrypoint-call
  forging, real signing via a new `wallet_sign_operation()` API
  (Scanner has no signer of its own — signs with whatever identity is
  active in Wallet), real broadcast attempt, real confirmation gating
  before anything irreversible. **The QR payload's own JSON schema is
  hand-rolled, not based on any existing standard** — checked after the
  fact (not before, unlike Beacon/Matrix/RPC) and confirmed no Tezos
  equivalent to Ethereum's EIP-681 exists. Means no real third-party
  dApp can produce a QR this Scanner would recognize today — see
  `docs/build-notes/scanner-app-build-notes.md`. **Real contract
  entrypoint discovery added since** — pressing UP after decoding a QR
  fetches the contract's actual entrypoints via TZKT
  (`tezos_tzkt_get_contract_entrypoints()`, real endpoint confirmed via
  multiple independent sources *before* writing code) and flags whether
  the scanned entrypoint claim is genuinely real, a useful safety check
  that's still separate from — and doesn't resolve — the underlying
  schema-interop gap above. That endpoint's exact response shape
  couldn't be verified against a live fetch this session (disclosed
  honestly, not glossed over — see the build notes), so parsing is
  deliberately narrow (names only) and defensive. Deliberately scoped to
  single-int-parameter calls (matching what `tezos_micheline.c` covers
  well) and a pre-specified call — both real, bounded follow-ups, not
  silent gaps.
- **Settings — real, second full app.** Device Name (real T9 edit/save
  round trip, verified with 10 checks including the actual multi-tap
  sequence and cancel-discards-correctly) and About (real, honestly
  distinguishes actual values from mock ones with no backend yet —
  see `docs/build-notes/settings-app-build-notes.md`) are built.
  **Real persistence now exists too** — `src/chain/tezos_store.c`, a
  genuinely encrypted (PIN-derived key via Argon2id, not theater) key
  store closing the gap where an imported recovery phrase used to
  vanish on every restart. Verified with the strongest test in this
  project: two separate process invocations sharing only the file on
  disk, proving a real cross-restart recovery, not just "save returns
  true" — see `docs/build-notes/wallet-app-build-notes.md`. **Lock/Passcode
  is now a complete Settings feature**, not just the underlying PIN
  concept — a real change-PIN flow (verifies the old PIN by actually
  decrypting, re-saves under a new one). **RPC Node is real too** — one
  persisted, T9-editable setting replacing what used to be a
  `DEV_RPC_NODE` placeholder duplicated separately in `wallet.c` and
  `scanner.c`. **Wi-Fi is real** — genuine `nmcli`/NetworkManager
  integration (confirmed as the actual current standard on this
  project's target OS *before* writing any code, unlike two earlier
  gaps this project caught the hard way — see
  `docs/build-notes/settings-app-build-notes.md`), verified against a
  mock nmcli exercising the real fork+execvp+exit-status mechanism and
  nmcli's own documented terse-output escaping convention. **Profiles is
  real now too** — multiple accounts derived from the one already-
  imported seed (the HD account index verified since this project's
  first BIP39/SLIP-10 work, not new crypto), with a real add/switch UI,
  verified with the same cross-process rigor as the encrypted store: a
  completely separate process correctly restores the previously-active
  account from nothing but the file on disk. Deliberately scoped to
  "multiple accounts, one seed" — the broader "multiple separate seeds
  behind discreet PINs" vision from earlier project history remains a
  real, separate, bigger feature, not built. Storage/Services still push
  the generic stub — no storage-layer UI or services concept exists yet
  beyond the key/settings store already built.
- **Shared dynamic entrypoint-calling engine started** — recognized that
  Messenger's "post a message to a real contract" and Scanner's "fully
  interact with any contract without the website" are the same
  underlying capability. Lives in `tezos_micheline.c` (a natural
  extension, not a new module): parses a contract's real entrypoint
  parameter type in the **standard Michelson/Micheline type grammar**
  (deliberately not TZKT's own less-documented JSON schema convention,
  which a live example couldn't be captured for) and builds a matching
  value tree from user-supplied leaf strings. Covers int/nat/mutez/
  string/bytes/address/bool/unit leaves and pair/or/option composites —
  scoped deliberately, an out-of-scope type (map, lambda, set) fails
  parsing cleanly rather than being approximated. **A real, serious bug
  caught before shipping**: the first draft used one shared buffer for
  encoding address/bytes leaves, meaning a type with two such leaves
  (e.g. `pair(address, address)`) would have silently corrupted the
  first one. Caught by testing two genuinely different real addresses
  in one such type and searching the final bytes for both
  independently. Every dynamically-built value verified to encode
  byte-for-byte identically to the equivalent built through this
  module's existing, already-verified direct constructors — see
  `docs/build-notes/scanner-app-build-notes.md`. **Fetching a real
  entrypoint's type from TZKT is done too** —
  `tezos_tzkt_get_entrypoints_with_types()`. Confirmed against the real,
  live Liquidity Baking response the user pulled directly:
  `"michelineParameters"` (this project's first guess) is exactly
  correct, and the real data surfaced a real gap synthetic fixtures
  never would have — every non-trivial entrypoint on that real contract
  uses a `timestamp` type, which wasn't supported at all until this was
  caught and fixed. **Update, two more real
  contracts checked**: QuipuSwap DEX 2.0 (30 real entrypoints) added
  `list` and `key_hash` support (both genuinely common there) and caught
  a real capacity bug (node cap of 16 was one node short of a real
  entrypoint, raised to 48). The real Teia Channels contract — the one
  Messenger actually needs — surfaced a bigger gap: `write_message`'s
  cc/bcc/attachments fields are `option(list(...))`, a combination the
  original list-only-at-the-root design couldn't build at all. Fixed
  with a real architectural change (list handling unified into the main
  value builder via a side-channel, works anywhere in the tree now), a
  breaking API change with every caller found and updated, and the
  *entire* test suite re-run to confirm no wider regression. Not yet
  done: wiring this into Scanner's UI or Messenger itself.

## M10 — Messenger
**Send side is real, built, and tested.** Not against the previously-
planned `channels.py`-based design or the old address — the live
contract data (confirmed by the user directly against TZKT:
`KT19VtkiKbg69tQW2iVkpRLc9h3mkS8y9MMw`) turned out to be a different,
richer design: a single `write_message` entrypoint with email-like
fields (subject/content/receivers/cc/bcc/attachments/threading) and
built-in token-gating, rather than Channels' separate
create_channel/post_message/Merkle-allowlist model. See
`docs/build-notes/messenger-app-build-notes.md`'s correction section for
the full history. Real UI (compose: receiver/subject/content via T9 →
confirm → real forge/sign/broadcast attempt), hand-built
`write_message` parameter value (the general dynamic engine's `map`
gap meant this needed direct construction, not routed through Scanner's
engine), verified level-by-level against the real confirmed type — not
just "it builds something." A real stack-use-after-return bug was
caught by testing under AddressSanitizer, not by inspection — fixed
before it could cause an actual crash on real hardware.

**Not built**: reading/displaying received messages. The real storage
schema (also confirmed via TZKT) suggests receiving a message mints an
FA2 token whose ID equals the message ID — meaning "what's in my
inbox" should be a standard, already-well-indexed FA2 token-balance
query followed by a `message_infos` big_map lookup per ID, not novel
indexing work. This is a real, load-bearing assumption from reading the
storage shape, not yet independently confirmed. **Update: the Inbox is
real now too** — token-balance fetch, per-message lookup, a real list
and detail view, tested under AddressSanitizer (same discipline that
caught the send-side bug), including a test-only injection hook to
verify list→detail→back navigation without needing live network
access. The core "held token = received message" assumption is still
genuinely unconfirmed — that's exactly what real outside testing is
for, not something to keep guessing at from a sandbox with no network.
**A real Network setting was also added** (Mainnet/Ghostnet/Shadownet)
— every TZKT-querying app had mainnet hardcoded with no way to select
anything else, which real Shadownet testing immediately required; all
9 call sites across Wallet/Scanner/Messenger updated.
- Discover app (objkt/Teia/bootloader.art browsing, buy via Scanner handoff).

## Snake — real, built, reachable from Settings
A real, native app (not a hidden easter egg), in the spirit of the
classic phone/console version — `src/apps/snake/`. See
`docs/build-notes/snake-build-notes.md`. Real seedable PRNG (not
`rand()`'s global state), real high-score persistence, real
180-reversal prevention, real distinct head/body sprite coloring.
Reachable from Settings → About ("PLAY SNAKE"), a real, considered
placement decision rather than growing the main app grid back after it
was just deliberately pared down. Two real test-design bugs (not
product bugs) were caught and fixed while writing the tests — both
documented honestly in the build notes rather than quietly cleaned up.
18+ checks under AddressSanitizer, zero errors.

## Headless Access (USB Gadget Mode & UART Console) — real research, one real hardware attempt so far
See `docs/build-notes/usb-gadget-build-notes.md` for the full writeup.
**Real hardware update**: a real attempt showed the device boots
successfully (a genuine, positive signal) but neither WiFi nor the USB
gadget setup produced reachable results — meaning there's currently no
way to reach the device over any network at all. Real, current research
on two fronts: USB gadget mode (`g_ether` alone, not combined with mass
storage — backed by two independent, converging real sources: the
Raspberry Pi Foundation's own official tooling deliberately doesn't
combine functions, citing real Windows driver-binding conflicts, and an
independent builder who tried a composite gadget reached the same
conclusion plus surfaced a real data-corruption risk with mass storage
specifically), and real UART serial console setup as the independent
fallback that doesn't depend on WiFi or USB working at all — the real
next step given the current stuck state, since it gives a real shell to
diagnose the other two problems from inside the device rather than
guessing blind from outside it. None of this confirmed working yet;
real, disclosed open item.

**Real update (2026-10-15):** `enable_uart=1` and `dtoverlay=disable-bt`
applied directly to the card's `config.txt` (backed up first); `cmdline.txt`
already had `console=serial0,115200`. Not yet boot-tested on real hardware —
see `docs/build-notes/usb-gadget-build-notes.md` for the full writeup, the
pre-existing-but-undocumented `piscreen` overlay swap found on the same card,
and a real lead on the WiFi question (the card's WiFi creds are in the legacy
`wpa_supplicant.conf`, which a NetworkManager-based image — likely, given this
is the same Trixie-class build already noted above — does not consume; not
yet confirmed or fixed).

## STM32F401 Bootstrap — real, linked, a real flashable image, never flashed yet
Real hardware on hand now: an STM32F401CCU6 Black Pill board, while
the real F412 target is on order. Used to prove the *full* real
toolchain pipeline for the first time — see
`docs/build-notes/stm32f401-bootstrap-build-notes.md`. A real linker
script and startup file (STM32F401CCU6's own real 256KB flash/64KB
RAM), a real link that succeeds, producing a genuinely valid ELF
(confirmed the `.isr_vector` sits at the real `0x08000000` flash base,
and the disassembly of `uart_read_byte()` was read by hand — real,
correct machine code polling the real USART1 address, not just "it
linked"). A real `build.sh` reproduces byte-identical output from a
clean run. Reuses the wire protocol and session layers completely
unchanged — proof the chip-independent design choice paid off, not
luck. **Deliberately excludes the RNG safety layer** — the STM32F401
doesn't have that hardware peripheral at all; linking it here would
either fail outright or silently read garbage and report it as real
entropy, exactly the failure mode that design exists to prevent.
Real, honest scope: never actually flashed to real hardware, no clock
configuration exists yet, and only core Cortex-M4 exception vectors
are populated (real, acceptable for polled-only operation with
interrupts never enabled).

## `.tez` Name Resolution — real, tested, wired into Wallet Send
Real, direct priority call: sequenced ahead of 1-bit Art, built first.
See `docs/build-notes/tez-domain-resolution-build-notes.md`. Built
against Tezos Domains' own official documented mechanism (a
NameRegistry contract's `records` bigmap, keyed by UTF-8-encoded
domain bytes), not guessed at — the hex-encoding convention was
verified by hand against a real worked example. Three real, honest
scope cuts, all disclosed: the documented proxy-resolution step isn't
implemented (no live network access to verify its exact storage
shape — the registry contract is instead a real, user-configurable
Settings value, empty by default rather than a possibly-wrong guessed
address), full Unicode normalization isn't implemented, and expiry
checking isn't implemented. Wired into Wallet's Send flow: a `.tez`
recipient triggers real resolution, with a real, distinct error for
each real failure mode (unconfigured registry, unresolvable name) —
never silently proceeding with an unresolvable name as if it were a
real address. **Two real, unrelated bugs found and fixed along the
way**: the Settings list was initialized with a stale hardcoded item
count, making several existing settings rows (RPC Node, Messenger
Contract, Network) genuinely unreachable via real navigation this
whole time; and a real build-system gap where a new hard dependency in
`wallet.c` broke ~19 other tests that link Wallet, fixed by moving the
dependency into `APP_WALLET`'s own shared definition. 26 checks total
across three real test suites, all under AddressSanitizer. Full
69-suite regression run twice, full-stack build clean.

## Grid Home Screen — real, tested, replaces the old text-list entirely
Real, direct follow-through on "the grid should be a first class
experience": see `docs/build-notes/grid-home-screen-build-notes.md`.
Real navigation built against this device's actual input (confirmed by
reading the real input enum, not assumed) — no dedicated left/right
exists, so `NUM` 1-9 jumps directly to a grid position (a real,
deliberate fit with a phone numpad's own 3×3 physical layout),
`UP`/`DOWN` traverse sequentially with real wrap-around as a fallback.
A real, live "selected app" name preview replaces the old static logo.
Real, integer-scaled icon rendering (`tezos_draw_icon_bitmap_scaled`,
nearest-neighbor, deliberately not smoothed) gives icons real visual
presence at 3× native size. The actual navigation math was extracted
into its own portable, testable module
(`tezos_grid_nav.h`/`.c`) specifically because the home screen had
never been independently testable before, even in its old list-based
form. 14 checks on the navigation math, 10 on the rendering functions
(2 new for scaled blit) — including a real test bug (an undersized
test framebuffer) AddressSanitizer caught and this project fixed on
the first run, not hidden. Full 72-suite regression run twice,
full-stack build clean, and the actual rendered grid reviewed visually
via a real screenshot, not assumed correct from the math alone.

## App Menu Icons — real rendering pipeline built, all 9 app icons designed
Real technical foundation done: see
`docs/build-notes/app-icons-build-notes.md`. A real correction made
along the way — this project's own earlier claim of "no icons at all"
was wrong; real infrastructure (MEK-Dings, a licensed font) already
existed, just stylistically mismatched once the actual glyphs were
checked directly. Direction settled: true multi-color mascot-style
icons (ICQ/AIM era). A real shared rendering function
(`tezos_draw_icon_bitmap`, extracted from duplicated blit code in
Gallery and Discover) plus a real authoring tool
(`tools/rasterize_icons.py`, icons as plain editable text) now exist.
8 checks under AddressSanitizer, including pixel-exact placement and
real bounds-clipping verification. All 9 real icons designed, reviewed
twice: a second pass, with concepts confirmed agreed-upon beforehand,
gave real, clear improvements to Wallet, Discover, and Beacon Wallet.
Explorer's second attempt is honestly a disclosed tradeoff, not a
clean win — bolder but less recognizable as a magnifying glass — and
is stated as such rather than claimed as success. Wired into the real
grid home screen below.

## Discover's Suggested list — real, built, cross-verified
Real follow-through on the design discussed earlier: see
`docs/build-notes/discover-suggested-list-build-notes.md`. Three
addresses (Teia Marketplace, objkt.com Marketplace v2, Liquidity
Baking), each cross-verified against multiple independent real
sources before inclusion — not a single unverified claim, matching
this project's own hard-learned lesson about exactly that mistake.
Visually distinct from real bookmarks (a real ★ prefix, appended after
the user's own list, never equal footing). 4 checks under
AddressSanitizer, including a real screenshot confirming the exact
correct address gets visited when a Suggested entry is selected.

## Discover — real, built almost entirely by reusing existing capability
A real Tezos address browser, in the spirit of an old-web browser --
visit any address, see real basic info, browse real holdings with real
images, bookmark for next time. See
`docs/build-notes/discover-app-build-notes.md`. **A real, deliberate
decision**: no hardcoded "known dApps" list -- the real, already-tested
address book *is* the bookmark list, user-curated like a real
browser's, not vendor-curated (this project has its own recent history
of a contract address being mislabeled with the wrong platform name in
a third-party report, caught only by independently checking it --
shipping an unverifiable hand-picked list felt like the wrong kind of
confidence to bake in). Nothing about Discover needed new chain-layer
code -- reuses Explorer's real contract-info/entrypoint functions and
Gallery's real, already-generic owned-tokens/image-decode pipeline
unchanged. A real design bug (browsing gated behind bookmarking, an
arbitrary constraint no real browser has) was caught and fixed while
writing this app's own test. 10 checks under AddressSanitizer, zero
errors. Real, disclosed scope: no hand-off to Explorer's entrypoint-
calling yet, and real, disclosed duplication of Gallery's image
pipeline rather than a shared extraction in this pass.

## Signer Wire Protocol — real, tested, cross-compiled for the real STM32F412
The chip decision landed on the STM32F412 (over the cheaper F401/F411,
specifically for its real hardware TRNG). See
`docs/build-notes/signer-protocol-build-notes.md`. Real ISO 7816-4
APDU framing (`firmware/signer/protocol/tezos_signer_apdu.*`),
deliberately portable and chip-independent, 22 checks under
AddressSanitizer. A real operation-assembly session layer
(`tezos_signer_session.*`) reassembles a complete operation across one
or more chunks, 31 checks — including a real logic bug this module's
own tests caught (an earlier version discarded a legitimately
in-progress multi-chunk operation's bytes on its final chunk, since it
couldn't distinguish "genuine completion" from "a fresh, unrelated
single-chunk operation"; fixed by tracking whether a real sequence was
already underway). **A real hardware RNG safety layer**
(`tezos_signer_rng_core.c`/`stm32f412/tezos_signer_rng.c`), built
directly against the real, ongoing August 2026 Coldcard hardware
wallet incident — a firmware path that silently fell back to a weak,
predictable seed when its real hardware entropy source misbehaved,
with no error or warning, generating brute-forceable keys. This
project's RNG driver follows Coldcard's own real, subsequent fix
(github.com/Coldcard/firmware #692/#693) rather than reinventing it,
and enforces one rule structurally: every caller gets a genuinely
verified random word or a signal to halt (`tezos_signer_panic_halt()`)
— never a silent fallback to anything softer. 8 checks, using the same
register-mocking technique Coldcard's own engineers used. All three
modules **cross-compiled cleanly for the real target**: a real ARM
cross-compiler installed, real STM32F412 CMSIS headers vendored from
ST's and ARM's own official repos (Apache 2.0), verified via `objdump`
to genuinely be `armv7e-m` Cortex-M4 machine code at both `-O0` and
`-O2`, zero warnings. `main.c` genuinely dispatches `INS_SIGN` through
the session layer now. Real, honest scope: this proves the protocol
code is valid for the real chip, not a flashable firmware image — no
linker script, startup file, real UART/GPIO clock configuration, or
actual signing logic exist yet.

## Address Book — real, built, integrated into Wallet
A real, shared "save a name, reuse the address" module
(`src/core/tezos_address_book.h`/`.c`), core-adjacent so any app can use
it. See `docs/build-notes/address-book-build-notes.md`. Real validation
on add (via `tezos_forge_address`, the same check every forge call
uses), real persistence (verified with a genuine two-process test,
same rigor as Settings' own), real delete-and-shift. Wired into
Wallet's Contacts page, which previously showed three hardcoded fake
entries — now real, with a "+ Add Contact" flow and real "send to a
saved contact" that jumps straight to amount entry, skipping manual
recipient re-entry. A real ordering bug (calling a function before its
definition, after a mid-file relocation) was caught by the compiler
and fixed by moving the code, not by a forward-declaration workaround.
**Not yet done**: Messenger's and Explorer's own address entry don't
offer "pick from saved" yet, and there's no dedicated contact-delete UI
(the underlying function exists and is tested; no screen calls it yet).

## Boot Splash — real, tested, wired into boot
An original animation in the spirit of the classic 16-bit console boot
logo (a bright gleam sweeping through "tezOS," letter by letter, plus
an accent beam) — built for tezOS's own name and palette, not a
reproduction of anyone else's specific artwork. See
`docs/build-notes/boot-splash-build-notes.md`. Shown once, first,
before the real home screen; auto-advances after the animation
completes, or immediately on any real key press. A real design
correction happened while building this: the first draft tried to skip
itself via `ACTION_POP`, which would have been a silent no-op on real
hardware (the splash is the only screen on the stack at that point, and
`tezos_core_pop()` correctly refuses to ever pop the last screen) —
fixed so the platform shim's own main loop performs the real transition
for both the timer-elapsed and key-press-skip cases. 10/10 checks
passing; never yet rendered on real DRM hardware, only in the software
test harness, same disclosed gap as everything else in this project.

## Beacon Wallet — real, standalone app, kept protocol-generic
tezOS can now act as a genuine Beacon-compatible wallet in its own
right, so this device can pair with any Beacon dApp. See
`docs/build-notes/beacon-wallet-app-build-notes.md`'s naming section
for the full, honest back-and-forth this went through, and
`docs/CONCEPTS.md`'s platform-philosophy note. **A real architectural
correction happened mid-build**: this started as an extension of
Camera's Scan mode, then was properly separated into its own standalone
app, `src/apps/beacon_wallet/` — Camera hands off the decoded pairing
info at exactly one point once a QR is scanned, and everything from
"connect" onward (a persistent identity, deliberately separate from any
Tezos account key; real incoming-request handling for both
`permission_request` and `operation_request`; approve via the same
popup-confirm pattern used throughout this project; sign via the real
forge/sign/broadcast pipeline; respond with the real result) lives in
the new app, not tangled into Camera's own internals. **The naming
question itself went through several real corrections and landed on
protocol-generic for now** — this project built and tested the app
against Kukai's real, published Beacon behavior, which confirmed the
architecture works and genuinely generalizes to any Beacon-compatible
wallet, but Kukai has been removed from production entirely; no Kukai
name or branding ships anywhere. How (or whether) to represent a
specific real wallet by name on this device is a real, open question,
deliberately not decided here. A real bug (a genuine `strncpy`
null-termination gap on network-derived data, not a false positive) was
caught and fixed while building this. Verified 8/8 under
AddressSanitizer, reusing the exact real `permission_request` fixture
already verified against `beacon-sdk` schemas elsewhere in this
project. Real, disclosed gaps: DENY doesn't send a real rejection
response, only the first operation in a batch is handled, the
`permission_response`'s public key is a placeholder, and the live
Matrix round trip remains genuinely unverifiable from this sandbox.

## Explorer — real (consolidates the former Scanner app)
Address lookup (T9, primary flow) plus real activity/staleness signals,
real entrypoints (names+types, via the shared dynamic engine), real
holdings, and now **real entrypoint calling** — built for the actual
stated use case: recovering your own assets from a project whose
website has already gone dark, which needs a contract address, not a
QR code from a site that no longer exists. See
`docs/build-notes/explorer-app-build-notes.md` for the full writeup,
including two real compiler warnings caught and fixed (one via a larger
buffer, one via this project's already-established pragma-suppression
pattern for a known GCC static-analysis limitation). **Deliberately,
permanently out of scope: any vulnerability/bug detection** — see
`docs/SECURITY-FINDINGS.md`.

**Update: the single largest remaining UI-engineering gap is closed.**
A new interactive type-walker (`tezos_micheline_walker_t`,
`tezos_micheline.h`/`.c`) turns the shared dynamic engine into a real,
pausable state machine a UI can drive one T9 prompt at a time — verified
with 27 checks including the exact `option(list(address))` shape
Messenger needed, now provably buildable through the general walker
rather than hand-built. Wired into Explorer: select an entrypoint with a
known type, get prompted for each real value in the correct order,
confirm, and it forges/signs/broadcasts through the same real pipeline
every other send flow uses. **A second real stack-use-after-return
bug, latent in Messenger's and Scanner's confirmation flows too, was
found and fixed at the source** (`tezos_popup.c` now copies its strings
instead of storing raw caller pointers) — see
`docs/architecture/CORE-ARCHITECTURE.md`'s popup section. Real,
disclosed scope remaining: `or`-typed choices always take `Left`,
optional fields are always filled in (never omitted), and a `map`
anywhere in an entrypoint's type still makes it uncallable — all
unchanged, pre-existing engine-level scope cuts, not new gaps from this
work.

## Gallery — real (built ahead of its original milestone slot)
Real owned-token list + detail view via `tezos_tzkt_get_owned_tokens()`
— the same real, live-verified endpoint as Messenger's Inbox,
generalized to every contract an address holds anything on rather than
one specific contract. See
`docs/build-notes/gallery-app-build-notes.md` for the full writeup,
including a real, structural bug this caught and fixed in
`scripts/install-tezos-service.sh`: the hand-listed app directory list
was silently missing both `messenger.c` and `gallery.c` — the exact
"drifts stale" problem this project's own history already documents
happening twice before with a different list. Fixed properly this time
by replacing it with a `find`-based glob, structurally immune to this
for every future app.

**Update: the thumbnail + 1:1 pixel viewer is real now too** — see
`docs/build-notes/gallery-viewer-build-notes.md`. This project's first
genuinely new third-party dependency (`stb_image.h`, real, checked
license, vendored into `third_party/stb/`), real IPFS URI resolution,
and a real pannable 1:1 viewport — the one genuinely new UI interaction
this project's toolkit didn't have before. Reuses Camera's *already-
generic* downsample/RGB565-convert functions directly, needing zero new
image-processing code for the resize/convert step. Two real memory
leaks (a stale `full_pixels` buffer across repeated Gallery visits) were
caught and fixed before shipping. 20 checks total across the decode/
IPFS module and the viewer's pan/clamp/blit mechanics, all under
AddressSanitizer.

## M11 — Community readiness
- Stage 1 custom carrier board (see hardware roadmap notes) replacing
  breadboard/jumper wiring.
- **Chassis: real design brief written** (`hardware/enclosure/CHASSIS-
  DESIGN-BRIEF.md`) — real, sourced component dimensions (Pi Zero 2 W,
  the CM4 migration path's real implications for internal volume, the
  3.5" display, camera module) and layout considerations, to model
  against in Blender and 3D print. No CAD/modeling started yet; this
  is the requirements groundwork the old placeholder was waiting on.
- Full build docs, BOM finalized, curated app-repo model documented.
- License finalized (MIT + CERN-OHL-P) across all repos.

## Deferred / post-launch tier (valuable, not blocking)
- **Camera-direct minting** — real idea, deliberately kept as a draft,
  not built: letting Camera's 1-bit/Art1bit output mint directly rather
  than requiring an external step. Explicit decision made when this was
  raised: **check whether an existing, already-audited contract (e.g.
  the real, confirmed HEN contract, `KT1RJ6PbjHpwc3M5rw5s2Nbmefwbuwbdxton`)
  already exposes a usable public mint entrypoint before considering a
  custom-deployed contract** — deploying new Michelson/SmartPy is a
  genuinely different category of work (real authorship this project
  has no experience in, real origination cost, real security review
  before trusting it with value) than anything built so far, and was
  agreed to be real scope creep for now. Revisit only when actually
  decided to move forward, not before.
- **Gallery: real thumbnail + 1:1 pixel viewer** — fully scoped, not
  started (see `docs/build-notes/gallery-app-build-notes.md`'s own
  scoping section for the full real plan: IPFS URI resolution, a real,
  checked candidate decode library (`stb_image.h`), reusing Camera's
  existing downscale/dither techniques generalized for arbitrary
  images, and one genuinely new UI pattern this project doesn't have
  yet — pannable 1:1 viewing on a fixed 320×480 panel). A real,
  standalone effort, not a quick follow-up.
- **`.tez` name management/registration app** — a real consideration,
  raised alongside the Farcaster-style identity discussion (see
  `docs/architecture/` or search history for the Tezos Domains contract
  address, `KT1Mqx5meQbhufngJnUAGEGpa4ZRxhPSiCgB`, confirmed real via
  the TZKT explorer and a third-party indexer's documented API). Open
  question, explicitly not decided yet: build this as a `.tez`
  registration/management app on top of the existing name service, OR
  go phone-number-style instead (a device-tied secondary identifier,
  closer to Farcaster's FID model) — these are two different identity
  approaches, not the same feature with two names, and the choice
  affects how Messenger/contact-adding ends up working. Revisit once
  there's a clearer sense of which direction fits the device's actual
  use, not before.
- HEN v2 collection cloning (user-as-creator origination) + mint/list forms.
- Minimal DeFi swap feature (single DEX/aggregator via Scanner pipeline).
- Local mesh chat (offline conference/badge social, ad-hoc WiFi).
- Protocol governance read-only display in Explorer.
- Stage 2 hardware: Compute Module migration (CM4 vs CM5 — see hardware
  roadmap notes), only once a real performance bottleneck is observed on
  the Zero 2 W.
- **Genuine Kukai wallet as a selectable software-wallet option** — real
  goal, explicitly *not* a reskin: must be Kukai's actual, live,
  officially-hosted web app (`wallet.kukai.app`) running unmodified in a
  real browser, with Kukai's own branding — not a copy of their
  Apache-2.0-licensed code (`kukai-wallet/kukai`) rebuilt into this
  project, which would need ongoing attribution/NOTICE maintenance and
  would drift from their real app the moment it's forked. **Rejected
  approach, explicitly (Owner, 2026-10-15):** using Kukai's code without
  attribution, or presenting it as something anyone could put their own
  logo on. Investigated whether a kiosk browser is feasible on the
  *current* Pi Zero 2 W + SPI panel: no, and not just a RAM/CPU
  tightness problem — the confirmed working display path
  (`dtoverlay=fbtft,ili9481,...`, see `docs/build-notes/
  display-bringup-build-notes.md`) is a slow, non-GPU-accelerated SPI
  framebuffer, and every realistic kiosk-browser option (even the
  lightest, WPE WebKit + Cog) is built around GPU-accelerated DRM/KMS
  or Wayland output — a categorically different display technology,
  not a bigger version of the same one. Gated on Stage 2 (Compute
  Module, real DRM/GPU output) — revisit once that lands, not before.
- Beacon (TZIP-10) camera-based QR pairing, and the Matrix transport it
  needs, are no longer deferred — both real, verified, and out of this
  section (see M8/M9 status above and `docs/build-notes/beacon-build-notes.md`
  and `camera-app-build-notes.md`). Still genuinely deferred: a live
  round trip through this to a real Matrix homeserver (needs real
  network access this sandbox doesn't have), and final confirmation the
  Matrix usage matches beacon-sdk's specific parameter choices.
- Camera Capture Mode's scroll interaction for the two oversized presets
  (800×480, 640×480), and the save/mint handoff (blocked on the 1-bit
  art/drawing app not existing yet to coordinate a file format with) —
  the pipeline itself is built and verified, this is just the remaining
  UI wiring.

## Open dependencies (block downstream work until resolved)
- **tezOS visual/interaction design system** — palette and typography
  drafted (`docs/design-system/PALETTE.md`). Core render API, state
  machine, and bitmap fonts are built and verified
  (`docs/architecture/CORE-ARCHITECTURE.md`, `src/core/`). Popups/
  confirmation dialogs are now real too (`src/core/tezos_popup.c`,
  verified and wired into Wallet's actual Send flow — see
  CORE-ARCHITECTURE.md). Still open: icon assets as
  real bitmaps. Blocks: full app UIs, not the core loop itself anymore.
- **Teia gating mechanism for a token-gated builders' channel** —
  resolved architecturally: Token Gated Chat (`token_gate.py`, per
  `docs/build-notes/messenger-app-build-notes.md`) gates by live FA2
  balance automatically, no Merkle-allowlist maintenance needed. Still
  open: confirming whether Token Gated Chat is actually deployed
  anywhere, and verifying the live Channels contract's real
  entrypoints/storage against the (unmerged, single-author) PR this
  understanding is based on.
- **App screen coordinate audit — done, with real evidence, corrected
  from an earlier overstated concern.** All four real screens (Wallet,
  Settings, Camera, recovery-phrase import) were rendered at the target
  resolution (320×480) plus deliberately shortened (320×360) and taller
  (320×600) stress tests. No overlap or clipping anywhere. The
  footer/softkey-bar pattern (`fb->height - 34`) and list-widget
  available-height calculation (`fb->height - N`) turn out to already
  be used consistently across all four files — this had been flagged
  as broken without actually rendering the evidence; checking it found
  the concern was more pessimistic than the real code. Remaining
  top-anchored absolute Y-positions for stacked text content are a
  normal, correct pattern for fixed-line-height text, not the "assumes
  exactly 480px" anti-pattern originally worried about. See
  `docs/architecture/CORE-ARCHITECTURE.md`'s "logical canvas" section
  for the fuller writeup, including the actual fix this pass made: a
  fixed logical rendering canvas, letterboxed onto whatever physical
  display is really attached (what makes Pi+HDMI+1080p-monitor testing
  show a correctly-proportioned UI instead of a stretched one).
- **BIP39 mnemonic (recovery phrase) support — built and verified.**
  `src/chain/tezos_bip39.c` + `tezos_slip10.c` implement mnemonic
  generation/validation/seed derivation and ed25519 HD derivation for
  path `44'/1729'/account'/0'`, verified against official BIP39 and
  SLIP-0010 test vectors plus a full end-to-end signer test — see
  `docs/build-notes/wallet-app-build-notes.md`. Still open: the actual
  UI screens (backup display, import flow) don't exist yet, and Kukai's
  exact derivation path isn't independently confirmed to match (needed
  before promising cross-wallet recovery in user-facing copy).

## Deferred, lower priority given current budget constraints
- **Kukai-kiosk Wallet mode** — running Kukai's actual open-source web
  wallet (self-hosted from a pinned commit, not live-fetched) in an
  embedded kiosk browser (WPE WebKit/Cog, not Chromium) as an alternative
  signer path for users who don't want to trust custom signer firmware —
  see the Coldcard RNG-bug incident (2021 bug, exploited 2026, $100M+)
  for why this concern is well-founded, not hypothetical. Real and worth
  building, but genuinely resource-heavy for a Zero 2 W and a likely
  concrete argument for the Stage 2 CM4/CM5 migration — not worth forcing
  onto current hardware given CM4 isn't in budget right now. Revisit
  alongside that hardware decision, not before it.
