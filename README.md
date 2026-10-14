# tezos-cyberdeck ("tezOS," pronounced TEZ-oh-ess)

A community-buildable Tezos cyberdeck: hardware wallet, block explorer,
contract-entrypoint scanner, and NFT/social client, built around tezOS —
a custom framebuffer-native window manager running on Raspberry Pi OS
Lite (Debian-based) — with an airgapped signer as the actual trust
boundary.

tezOS doesn't assume one fixed screen size or enclosure. `tezos_gfx`/
`tezos_core`/`tezos_widgets` work off the actual framebuffer dimensions,
not hardcoded values, and this was checked with real evidence, not just
described: all four real app screens render correctly at the target
resolution (320×480) and at deliberately different stress-test sizes,
with no overlap or clipping (see `docs/architecture/CORE-ARCHITECTURE.md`'s
"logical canvas" section). That section also covers the actual fix that
makes testing on hardware that doesn't match the target panel show a
correctly-proportioned UI — letterboxed, not stretched — which matters
in practice (e.g. developing via a Pi's HDMI output to a normal 1080p
monitor before the real small panel is wired up). Two build paths
are supported deliberately either way: build your own from the open BOM,
or use an official kit/reference hardware configuration once one exists
— both run the same software.

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
│   └── signer/                    — STM32F412 signer firmware (production target) +
│                                     STM32F401 bootstrap (stm32f401_bootstrap/, real
│                                     hardware on hand now, proves the FULL real toolchain
│                                     pipeline for the first time -- real linker script +
│                                     startup file + a real link producing a genuinely
│                                     valid, flashable image, verified via objdump, not
│                                     just "it linked". Deliberately excludes the RNG
│                                     safety layer -- F401 lacks that hardware peripheral).
│                                     REAL, shared by both targets: the wire protocol
│                                     (protocol/tezos_signer_apdu.*, ISO 7816-4 APDU
│                                     framing), the operation-assembly session layer
│                                     (protocol/tezos_signer_session.*, caught a real logic
│                                     bug in its own tests). F412-only: a real hardware RNG
│                                     safety layer (protocol/tezos_signer_rng_core.c +
│                                     stm32f412/tezos_signer_rng.c) built directly against
│                                     the real August 2026 Coldcard incident and Coldcard's
│                                     own real fix. NOT built yet: ed25519 signing, the
│                                     physical confirm button, real UART/GPIO clock config,
│                                     real hardware flashing (never attempted yet)
│
├── third_party/
│   ├── stb/stb_image.h            — vendored (nothings/stb v2.30, real MIT/public-domain
│   │                                 dual license, checked before adding) -- used by
│   │                                 Gallery's real thumbnail/1:1 viewer
│   └── cmsis_stm32f4/             — real STM32F412 + ARM CMSIS Core headers, vendored
│                                     from STMicroelectronics' and ARM's own official repos
│                                     (Apache 2.0, checked before adding) -- lets the real
│                                     signer wire protocol cross-compile against the actual
│                                     target chip's real register definitions
├── src/
│   ├── core/                       — tezOS render API, shared list widget, T9 text-entry
│   │                                 widget, PIN entry widget (tezos_pin_entry.c), screen/app/
│   │                                 input state machine, 4 bitmap fonts, a real boot splash,
│   │                                 a real shared address book (tezos_address_book.h), and
│   │                                 real image decode (tezos_image_decode.h, wraps a vendored
│   │                                 stb_image.h -- see third_party/). Built and
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
│   │                                 reference vectors. tezos_micheline: Michelson entrypoint-
│   │                                 call parameter encoding, verified against Taquito's exact
│   │                                 test vectors (unblocks Scanner/HEN-minting/Messenger).
│   │                                 tezos_signer_protocol: the actual Pi<->signer wire protocol
│   │                                 (framing + a software simulation), see
│   │                                 docs/architecture/SIGNER-PROTOCOL.md. tezos_http/tezos_tzkt:
│   │                                 real network client (libcurl + TZKT reads), wired into
│   │                                 Wallet's real Send flow with a genuine offline fallback.
│   │                                 tezos_beacon_crypto/message/qr: Beacon (TZIP-10) crypto
│   │                                 handshake, message schema, and QR pairing payload, all
│   │                                 traced from the real beacon-sdk source and verified
│   │                                 (real QR encode/decode via libqrencode+zbar). tezos_matrix:
│   │                                 spec-verified Matrix client (register/createRoom/send/sync),
│   │                                 endpoint shapes checked against the official Matrix spec.
│   │                                 tezos_rpc: real operation broadcasting (POST /injection/operation),
│   │                                 verified against a real working forge/sign/inject example, wired
│   │                                 into Wallet's Send/Delegate.
│   │                                 tezos_beacon_pair: ties all three together into one real
│   │                                 pairing flow, verified with a full simulated exchange
│   │                                 recovering a message byte-for-byte between two independent
│   │                                 identities. tezos_store: real encrypted-at-rest key store
│   │                                 (PIN-derived key via Argon2id, not theater) closing the gap
│   │                                 where an imported recovery phrase used to vanish on every
│   │                                 restart — verified with two separate process invocations
│   │                                 sharing only a file on disk, the strongest test in this
│   │                                 project. tezos_wifi: real nmcli/NetworkManager integration,
│   │                                 confirmed as the real current standard for this project's
│   │                                 target OS before writing any code, fork+execvp (not
│   │                                 popen/system) for injection safety against user-typed
│   │                                 SSID/password — see docs/build-notes/beacon-build-notes.md for what's real vs.
│   │                                 the live homeserver round trip that genuinely isn't testable
│   │                                 from this environment. tezos_bip39, tezos_slip10: recovery-
│   │                                 phrase generation/validation and ed25519 HD derivation
│   │                                 (44'/1729'/account'/0'), verified against official BIP39
│   │                                 and SLIP-0010 test vectors (see wallet-app-build-notes.md)
│   └── apps/
│       ├── wallet/               — REAL: home + Send-confirm screens, genuinely forge
│       │                            + sign a transaction via src/chain/ (mock signer).
│       │                            Send's recipient/amount are real T9 entry now too --
│       │                            was Wallet's oldest hardcoded gap, closed alongside a
│       │                            real memory leak shared across 4 files (Wallet/
│       │                            Messenger/Scanner/Explorer). Contacts is real now
│       │                            too (was 3 hardcoded fake entries) -- backed by the
│       │                            new core/tezos_address_book.h, "+ Add Contact" +
│       │                            real "send to saved contact" (jumps straight to
│       │                            amount entry, address pre-filled) -- see
│       │                            wallet-app-build-notes.md for what's still placeholder
│       ├── explorer/            — REAL: consolidates what used to be a separate
│       │                            "Scanner" app. Address lookup (T9, not QR --
│       │                            the actual use case is recovering assets from
│       │                            a project whose website is already gone) ->
│       │                            real activity/staleness signals, real
│       │                            entrypoints (names+types), real holdings, and
│       │                            now REAL entrypoint calling -- a real T9-driven
│       │                            walker prompts for each parameter (or list item
│       │                            count) in the correct order, then forges/signs/
│       │                            broadcasts. Deliberately does NOT do
│       │                            vulnerability/bug detection -- see
│       │                            docs/SECURITY-FINDINGS.md.
│       ├── scanner/             — kept, not deleted: real QR-based "generic
│       │                            entrypoint decode/fill/forge" logic, no
│       │                            longer a separately-registered app -- real,
│       │                            reusable code pending folding into Explorer
│       │                            as an alternate input method (not done yet)
│       ├── gallery/              — REAL: owned-token viewer, real list +
│       │                            detail view via tezos_tzkt_get_owned_tokens
│       │                            (the same real, live-verified endpoint as
│       │                            Messenger's Inbox, generalized to every
│       │                            contract an address holds anything on).
│       │                            REAL thumbnail (real fetch/decode via a
│       │                            vendored stb_image.h -- this project's first
│       │                            new third-party dep -- + Camera's own
│       │                            already-generic downsample/RGB565 convert)
│       │                            and a REAL pannable 1:1 viewer, the one
│       │                            genuinely new UI interaction this project's
│       │                            toolkit didn't have before
│       ├── discover/            — REAL: a real Tezos address browser, old-web-browser
│       │                            style -- visit any address, real basic info + real
│       │                            holdings with real images, bookmark for next time.
│       │                            No hardcoded "known dApps" list -- the real address
│       │                            book IS the bookmark list, user-curated not
│       │                            vendor-curated. Reuses Explorer's + Gallery's real
│       │                            functions unchanged; nothing new at the chain layer
│       ├── messenger/          — REAL send + REAL inbox. Send: compose (T9
│       │                         receiver/subject/content), real forge/sign/
│       │                         broadcast of the actual write_message
│       │                         entrypoint (contract confirmed by the user
│       │                         directly against TZKT). Inbox: real FA2
│       │                         token-balance query + message_infos lookup,
│       │                         real list + detail view -- rests on a real,
│       │                         still-unconfirmed assumption (held token ID
│       │                         == received message ID), which is exactly
│       │                         what real outside testing is for
│       ├── camera/                — REAL: Scan mode -- Beacon QR pairing (real
│       │                            libqrencode->zbar->parse pipeline). Hands off the
│       │                            decoded peer info to beacon_wallet/ once "CONNECT"
│       │                            is pressed -- Camera's own job ends at scan+decode.
│       │                            AND Capture mode (real box-downsample + 4
│       │                            color-depth modes incl. programmatically-generated
│       │                            Bayer dithering, camera_pipeline.c). Both use a
│       │                            "simulate" trigger in place of real camera
│       │                            hardware, which doesn't exist here. Camera now
│       │                            opens to a real mode-select.
│       ├── beacon_wallet/        — REAL: a genuine, standalone Beacon-compatible
│       │                            wallet, kept protocol-generic ("Beacon Wallet",
│       │                            no specific wallet's name/branding). Built and
│       │                            tested against Kukai's real, published Beacon
│       │                            behavior -- confirmed the architecture works and
│       │                            generalizes to any Beacon-compatible wallet, but
│       │                            Kukai has since been removed from production
│       │                            entirely; representing a specific wallet by name
│       │                            is a real, open question, not decided here (see
│       │                            build notes' naming section for the full
│       │                            back-and-forth). Real persistent identity, real
│       │                            session, real permission_request/operation_request
│       │                            handling (approve -> confirm -> sign/broadcast ->
│       │                            respond). Live Matrix connectivity remains genuinely
│       │                            unverifiable from this sandbox.
│       ├── art1bit/                — 1-bit drawing app
│       ├── settings/             — REAL: Device Name (T9 edit/save), About, Import
│       │                            Recovery Phrase (T9 + live BIP39 wordlist filtering),
│       │                            RPC Node (real persisted setting, replaces a duplicated
│       │                            placeholder in wallet.c/scanner.c), Lock/Passcode (real
│       │                            change-PIN flow), Wi-Fi (real nmcli/NetworkManager
│       │                            integration, fork+execvp for injection safety), and
│       │                            Profiles (multiple accounts from one seed, real HD
│       │                            account-index switching, verified cross-process) screens.
│       │                            Storage/Services still push the stub (no storage-layer
│       │                            UI or services concept exists yet)
│       └── snake/                — REAL: a native game (not a hidden easter egg), real
│                                    seedable PRNG (not rand()'s global state), real
│                                    high-score persistence, real 180-reversal prevention.
│                                    Reachable from Settings -> About ("PLAY SNAKE") -- a
│                                    real, considered placement, not a main-app-grid slot
│
├── tests/                            — 72 permanent test suites, 685 checks total. ONE COMMAND:
│                                       ./tests/run_all_tests.sh — see tests/README.md. Two suites
│                                       (network/Matrix) attempt real live calls this project's own
│                                       dev sandbox couldn't reach -- genuinely more capable on real
│                                       hardware with real network than they were when first built.
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
5. `COMPILE-AND-TEST-PLAN.md` — hand this to anyone (human or agent) who
   needs to build and verify this project in a plain Debian dev
   environment before touching real hardware. Independently reproduced
   once already (`21 passed, 0 failed`, clean `-O2` build of the real
   target binary) — see `ROADMAP.md`'s M3 section and `tests/README.md`.
