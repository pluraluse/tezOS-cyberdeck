# tezos-cyberdeck

A community-buildable Tezos cyberdeck: hardware wallet, block explorer,
contract-entrypoint scanner, and NFT/social client, built around a custom
framebuffer-native window manager (tezOS) running on commodity Linux SBC
hardware, with an airgapped signer as the actual trust boundary.

**Status: core software and a real operation forging/signing backend
built and verified; hardware bring-up not yet started.** The render API,
screen/app/input state machine, and four `tezOS default` bitmap fonts are
implemented and proven working (see `docs/architecture/CORE-ARCHITECTURE.md`).
Operation forging and signing (`src/chain/`) are genuinely implemented
and verified byte-for-byte against Taquito's own reference test vectors,
and Wallet's home + Send-confirm screens (`src/apps/wallet/`) exercise
that pipeline for real (see `docs/build-notes/wallet-app-build-notes.md`).
Real DRM/evdev wiring to actual Pi Zero 2 W hardware, a network layer,
popups/alerts design, and icon assets are still open — see `ROADMAP.md`.

## Mission

Make hardware-isolated signing — not "connect wallet" in a browser — the
natural, accessible way to use the Tezos ecosystem. The device should feel
like real, professional infrastructure, not a novelty.

## Pillars

1. **Wallet & signing** — airgapped signer, wired not wireless, physically
   disconnected except when signing. Every operation is decoded and shown
   before confirmation, never blind-signed. No shortcuts here, ever.
2. **Full ecosystem coverage** — payments, staking/delegation, `.tez`
   domain resolution (Wallet); any contract including abandoned ones
   (Scanner, Explorer's "Graveyard" mode); owned NFTs (Gallery);
   marketplace browsing (Discover); social via Teia Channels V2
   (Messenger). Deferred but scoped: minimal DeFi swap, protocol
   governance visibility, HEN v2 collection cloning, offline
   conference/badge mesh chat, Beacon (TZIP-10) compatibility.
3. **Community buildable, deliberately not maximalist** — curated app
   repo, not an open app store. Commodity, sourceable hardware with a
   documented BOM, so cost is an addressable engineering problem, not
   something "solved" by asset price movements (see
   `docs/FUNDING-STRATEGY.md`).
4. **Legitimacy, not novelty** — open-source everything, get the signer
   independently audited, then pursue grant funding on the strength of a
   working device, in that order.
5. **Community governance, eventually — not by default** — single/small-
   team ownership through initial build and grant execution, moving
   toward a multisig and possible DAO only once there's a real treasury
   and real independent contributors to govern for (see
   `docs/GOVERNANCE-TRANSITION.md`).

## Scope decisions already made

These came up during planning and were decided against, at least for now
— noted here so they don't get silently re-litigated or re-built a
different way by someone who wasn't in that conversation. None of these
are permanent; if something changes, revisit and update this list.

- **Open app store** — considered, went with a curated repo instead. This
  is about avoiding OEM/carrier-style forced, undeletable bundled apps
  tied to sales agreements — not a cap on how many good apps the suite
  can have.
- **General-purpose phone OS** — tezOS is scoped to this device's app set,
  not a daily-driver phone replacement.
- **Full DeFi platform** — one minimal swap feature is the current
  ceiling, not lending, LP positions, or yield farming.
- **Multi-chain support** — staying Tezos-native.
- **Multi-chain hardware wallet ambitions** — scope is Tezos signing,
  done well and audited, not competing with Ledger/Trezor's broader coverage.
- **Solving affordability through token price** — the actual levers are
  grants, fiat/stablecoin pricing, and BOM cost reduction (see
  `docs/FUNDING-STRATEGY.md`).
- **Phone/Android hardware support** — considered (ESP32 and Android
  shims both came up), dropped in favor of Linux SBCs only for now — see
  `docs/build-notes/hardware-roadmap-build-notes.md`.

## Directory structure

```
tezos-cyberdeck/
├── README.md                    — this file
├── CONTRIBUTING.md               — workflow, review standards, AI-assisted contribution rules
├── ROADMAP.md                   — phased milestones, bring-up sequence
├── BOM.md                       — bill of materials, need/want/nice-to-have
├── LICENSE                      — MIT (software)
├── LICENSE-HARDWARE.md          — CERN-OHL-P (hardware designs), pointer + rationale
│
├── docs/
│   ├── build-notes/             — per-app/subsystem decision logs (see its own index)
│   ├── architecture/            — CORE-ARCHITECTURE.md (render API, state machine,
│   │                               memory model — built and verified); signer protocol
│   │                               spec still TBD
│   ├── design-system/           — palette + typography (PALETTE.md), ICONS.md,
│   │                               static mockups, interactive-clickthrough-mockup.html
│   │                               (idle menu → all 9 app screens → back; renders every
│   │                               character — static labels and the scrolling list alike —
│   │                               from the real rasterized MonoMEK/Press Start 2P glyph
│   │                               data via canvas, no web fonts, works fully offline);
│   │                               popups still open (see ROADMAP.md)
│   ├── FUNDING-STRATEGY.md      — grants, audit credibility, platform onboarding sequencing
│   └── GOVERNANCE-TRANSITION.md — path and timing toward community DAO governance
│
├── hardware/
│   ├── carrier-board/           — Stage 1 custom carrier PCB for Pi Zero 2 W (KiCad, TBD)
│   ├── enclosure/                — 3D-print files (TBD)
│   ├── signer/                    — signer board hardware reference (STM32F401 / Pi Pico, TBD)
│   └── pinouts/                    — GPIO maps, wiring diagrams; tca8418-overlay-notes.md
│                                       (device-tree binding + keymap the shim expects)
│
├── firmware/
│   └── signer/                    — bare-metal C signer firmware (TBD)
│
├── src/
│   ├── core/                       — tezOS render API, shared list widget, screen/app/
│   │                                 input state machine, and 4 bitmap fonts. Built and
│   │                                 verified — real end-to-end push/pop navigation
│   │                                 proven working (see
│   │                                 docs/architecture/CORE-ARCHITECTURE.md).
│   │                                 Not yet wired to real DRM/evdev hardware.
│   ├── shim-linux/            — linux_shim.c: real DRM/KMS + evdev platform shim.
│   │                             Compiles and links clean against real libdrm headers
│   │                             (verified in dev sandbox); fails gracefully with no
│   │                             real /dev/dri device (expected outside real Pi
│   │                             hardware). Not yet runtime-verified — see ROADMAP.md M1.
│   ├── chain/                     — tezos_base58, tezos_forge, tezos_signer: real operation
│   │                                 forging/signing, verified byte-for-byte against Taquito
│   │                                 reference vectors (see wallet-app-build-notes.md)
│   └── apps/
│       ├── wallet/               — REAL: home + Send-confirm screens, genuinely forge
│       │                            + sign a transaction via src/chain/ (mock signer —
│       │                            see wallet-app-build-notes.md for what's still placeholder)
│       ├── explorer/            — block/contract/operation browsing, "Graveyard" mode
│       ├── scanner/             — generic entrypoint decode/fill/forge
│       ├── gallery/              — owned-token viewer
│       ├── discover/            — objkt/Teia/bootloader.art browsing
│       ├── messenger/          — Teia Channels V2 client
│       ├── camera/                — capture + scan modes (see build-notes)
│       ├── art1bit/                — 1-bit drawing app
│       └── settings/             — device settings, profiles, About (see build-notes)
│
├── tools/                            — rasterize_font.py (font-to-bitmap conversion,
│                                       used for the 4 tezOS default fonts); dev-loop
│                                       SDL framebuffer emulator still TBD
└── scripts/                        — tezos.service (boot-to-kiosk systemd unit),
                                        install-tezos-service.sh (build + install, run on the Pi)
```

Directories marked **TBD** are placeholders — they exist now so the project
has one settled shape to build into, not because anything's inside yet.

## License

- **Software**: MIT (`LICENSE`) — matches the license of the Tezos ecosystem
  tooling this project builds on (Teia's `teia-ui` is also MIT).
- **Hardware designs** (PCB, enclosure): CERN-OHL-P — see `LICENSE-HARDWARE.md`.

## Where to start

1. `ROADMAP.md` — the actual bring-up sequence, phase by phase.
2. `BOM.md` — what to buy, and what's need/want/nice-to-have.
3. `docs/build-notes/README-build-notes-index.md` — every concrete decision
   made so far, and why.
4. `docs/FUNDING-STRATEGY.md` and `docs/GOVERNANCE-TRANSITION.md` — the
   legitimacy, funding, and eventual community-governance path.
