# Roadmap

Organic bring-up: each milestone must provably work before the next one
depends on it. Don't skip ahead.

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
  if the network genuinely isn't reachable. Still placeholder: balance
  display, recipient/amount entry (the T9 widget
  now exists and is verified — see `docs/architecture/CORE-ARCHITECTURE.md`
  — but isn't wired into Wallet's Send screen yet), and the RPC node
  itself is a clearly-marked placeholder pending a Settings-configurable
  field. Explorer,
  Scanner, Gallery, Camera still push the generic stub screen.
- **Settings — real, second full app.** Device Name (real T9 edit/save
  round trip, verified with 10 checks including the actual multi-tap
  sequence and cancel-discards-correctly) and About (real, honestly
  distinguishes actual values from mock ones with no backend yet —
  see `docs/build-notes/settings-app-build-notes.md`) are built. Profiles,
  Lock/Passcode, Wi-Fi, Storage, Services still push the generic stub —
  genuinely blocked on the profile/PIN system and network layer, not
  arbitrarily deferred.

## M10 — Messenger
- Teia Channels V2 client (contract `KT19ooSLPFxJQ5mx3kR4Qo2UY4KJDcdMdng9`):
  read via TZKT events/bigmaps, write via IPFS-pin + `post_message`/
  `create_channel`.
- Discover app (objkt/Teia/bootloader.art browsing, buy via Scanner handoff).

## M11 — Community readiness
- Stage 1 custom carrier board (see hardware roadmap notes) replacing
  breadboard/jumper wiring.
- Full build docs, BOM finalized, curated app-repo model documented.
- License finalized (MIT + CERN-OHL-P) across all repos.

## Deferred / post-launch tier (valuable, not blocking)
- HEN v2 collection cloning (user-as-creator origination) + mint/list forms.
- Minimal DeFi swap feature (single DEX/aggregator via Scanner pipeline).
- Local mesh chat (offline conference/badge social, ad-hoc WiFi).
- Protocol governance read-only display in Explorer.
- Stage 2 hardware: Compute Module migration (CM4 vs CM5 — see hardware
  roadmap notes), only once a real performance bottleneck is observed on
  the Zero 2 W.
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
