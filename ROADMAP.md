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
  on M1/M2 being verified first).

## M5 — tezOS core skeleton
- DRM init + evdev init + drawing primitives (blit rect, bitmap-font text)
  + input dispatch loop. The foundation everything else builds on.
- **Status: core logic built and verified in software** — see
  `docs/architecture/CORE-ARCHITECTURE.md` and `src/core/`. The render
  API, screen/app/input state machine, shared list widget, dirty-rect
  redraw strategy, real end-to-end PUSH/POP navigation, real body/small
  fonts (MonoMEK, confirmed CC0), and a `draw_icon` primitive with a real
  icon bitmap asset (MEK-Dings) are all implemented and proven working.
  What's left for this milestone: the icon-to-app mapping (a design
  decision — see `docs/design-system/ICONS.md`), MEKmode as an optional
  future swap for the display/header role (MonoMEK covers all roles fine
  for now — this is polish, not a gap), T9
  text-entry widget, popups/alerts (layout/transitions), real DRM/evdev
  platform-shim code, and every app's actual screen (currently all push
  a generic placeholder stub).

## M6 — First real screen
- Idle/home screen with T9 shortcut navigation. No network, no wallet, no
  signer required yet — fastest path to "this feels like a real device."

## M7 — Networking
- WiFi config + basic HTTP client wired into core. Unblocks Explorer/Wallet
  (TZKT calls).

## M8 — Wallet + signer pairing
- Signer serial protocol (forged-op in → decode/display → signature out)
  working end to end with the STM32F401 or Pi Pico.
- Device becomes an actual wallet at this point, not just a UI shell.

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
  not just "doesn't crash." Still placeholder: balance display, branch/
  counter (need a network layer), recipient/amount entry (needs the
  T9 widget), broadcast (needs a network layer), and any entrypoint-call
  support in forging (plain tez transfers only so far). Explorer,
  Scanner, Gallery, Settings, Camera still push the generic stub screen.

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
- 1-bit Game Boy Camera-style capture mode refinements, Beacon QR pairing
  via camera (Scan mode).

## Open dependencies (block downstream work until resolved)
- **tezOS visual/interaction design system** — palette and typography
  drafted (`docs/design-system/PALETTE.md`). Core render API, state
  machine, and bitmap fonts are built and verified
  (`docs/architecture/CORE-ARCHITECTURE.md`, `src/core/`). Still open:
  popups/system messages/alerts (layout/transitions) and icon assets as
  real bitmaps. Blocks: full app UIs, not the core loop itself anymore.
- **Teia gating mechanism for a token-gated builders' channel** —
  resolved architecturally: Token Gated Chat (`token_gate.py`, per
  `docs/build-notes/messenger-app-build-notes.md`) gates by live FA2
  balance automatically, no Merkle-allowlist maintenance needed. Still
  open: confirming whether Token Gated Chat is actually deployed
  anywhere, and verifying the live Channels contract's real
  entrypoints/storage against the (unmerged, single-author) PR this
  understanding is based on.
