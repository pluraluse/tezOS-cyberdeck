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

## M2 — Input working
- TCA8418 keypad decoder wired over I2C: numeric matrix + 2 softkeys +
  dedicated Up/Down buttons, all through one interrupt line.
- XPT2046 touch confirmed working via mainline `ads7846`-compatible driver.

## M3 — "Hello framebuffer"
- Minimal `libdrm` test program: open device, set mode, paint a test pattern.
- Separate minimal evdev reader logging raw key/touch events.
- Two standalone programs, each proving one half of the physical loop,
  before any OS logic exists.

## M4 — Boot-to-kiosk
- Pi boots straight into a systemd service running the test binary — no
  login prompt, no desktop session. tezOS *is* the shell from day one.

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
  decision — see `docs/design-system/ICONS.md`), MEKmode for the
  display/header role (still Press Start 2P as placeholder), T9
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
- **Teia gating mechanism for a token-gated builders' channel** — Merkle
  allowlist maintenance flow (not live token-balance check) needs a
  concrete update-cadence design before Messenger's gated channel ships.
