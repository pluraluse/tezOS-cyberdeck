# tezOS Cyberdeck — Build Notes Index

Master index of all build-notes documents. Add a line here every time a new
build-notes file is created, so the full decision trail stays navigable.

**Rebuilt 2026-10-16** — this index had drifted to listing 5 of the 25
files that actually exist; see `project-status-2026-10-16.md` for the
other self-tracking docs fixed in the same pass.

| Doc | Covers |
|---|---|
| [project-status-2026-10-16.md](./project-status-2026-10-16.md) | The real-hardware milestone (first boot splash, Wallet, Settings confirmed live via synthetic input) and the real-vs-mock state of the whole project as of this date |
| [display-bringup-build-notes.md](./display-bringup-build-notes.md) | Finding the actual working display overlay (`fbtft,ili9481`) by live hardware trial after the `piscreen` assumption failed; pin confirmation, the SPI ID-poll dead end, color/mirror fixes |
| [linux-shim-fbdev-rewrite-build-notes.md](./linux-shim-fbdev-rewrite-build-notes.md) | Rewriting `linux_shim.c` from libdrm to fbdev to match the real display path; first real app navigation (Wallet, Settings) confirmed on hardware |
| [boot-splash-build-notes.md](./boot-splash-build-notes.md) | The gleam-sweep boot splash animation design and the real skip/auto-advance mechanism |
| [boot-splash-logo-font-build-notes.md](./boot-splash-logo-font-build-notes.md) | Swapping the boot splash wordmark to GROUT Display (Michael Alexander/MEK.txt), confirmed live on hardware |
| [hardware-roadmap-build-notes.md](./hardware-roadmap-build-notes.md) | Stage 1 custom carrier board (Zero 2 W) vs. Stage 2 Compute Module migration (CM4 vs CM5), decision status, and the Kukai-kiosk-browser driver for Stage 2 |
| [usb-gadget-build-notes.md](./usb-gadget-build-notes.md) | Headless access research: USB gadget mode (`g_ether`/`g_serial`) and UART console setup; real hardware update (boots, WiFi since confirmed working) |
| [signer-protocol-build-notes.md](./signer-protocol-build-notes.md) | The Pi↔signer ISO 7816-4 APDU wire protocol, session layer, RNG safety design — real, tested, cross-compiled for the STM32F412, never flashed |
| [stm32f401-bootstrap-build-notes.md](./stm32f401-bootstrap-build-notes.md) | Proving the real toolchain pipeline on an STM32F401 Black Pill (the F412 target is on order) — real linked ELF, never flashed |
| [wallet-app-build-notes.md](./wallet-app-build-notes.md) | Real operation forging + signing (base58, zarith, tag bytes, watermark/blake2b/ed25519), all verified against Taquito reference vectors byte-for-byte; what's real vs. still placeholder |
| [address-book-build-notes.md](./address-book-build-notes.md) | Wallet's address book: validation, persistence, saved-contact send flow |
| [tez-domain-resolution-build-notes.md](./tez-domain-resolution-build-notes.md) | `.tez` name resolution and its Wallet Send integration |
| [settings-app-build-notes.md](./settings-app-build-notes.md) | Input model (softkeys + up/down), Settings app scope, profile architecture, discreet profile switching, About screen contents |
| [beacon-build-notes.md](./beacon-build-notes.md) | Beacon (TZIP-10) crypto handshake, message schema, QR pairing payload — verified against real beacon-sdk source |
| [beacon-wallet-app-build-notes.md](./beacon-wallet-app-build-notes.md) | The standalone Beacon Wallet app — real incoming-request handling, kept protocol-generic rather than branded as any specific wallet |
| [messenger-app-build-notes.md](./messenger-app-build-notes.md) | Teia messaging contracts: Channels, Token Gated Chat (solves builders'-channel gating), Poll/Token Comments — sourced from both the live frontend and an unmerged upstream PR, with the discrepancy between them flagged |
| [camera-app-build-notes.md](./camera-app-build-notes.md) | Camera app: Scan mode vs. Capture mode, resolution presets, color-depth modes, box-average downsample pipeline, minting integration — no real camera sensor wired yet |
| [scanner-app-build-notes.md](./scanner-app-build-notes.md) | The original Scanner app (QR scan → decode → forge/sign/broadcast) — superseded by Explorer. **Code removed 2026-10-16** (was orphaned-but-kept by mistake); this note stays as historical record only |
| [explorer-app-build-notes.md](./explorer-app-build-notes.md) | Explorer: address entry, entrypoint/holdings browsing, consolidates Scanner's capability generically |
| [discover-app-build-notes.md](./discover-app-build-notes.md) | Discover: address browsing, bookmarking, holdings with images |
| [discover-suggested-list-build-notes.md](./discover-suggested-list-build-notes.md) | Discover's Suggested list, cross-verified routing to real addresses |
| [gallery-app-build-notes.md](./gallery-app-build-notes.md) | Gallery: owned-token query, real image decode pipeline |
| [gallery-viewer-build-notes.md](./gallery-viewer-build-notes.md) | The pannable 1:1 pixel viewer — pan/clamp/blit mechanics |
| [grid-home-screen-build-notes.md](./grid-home-screen-build-notes.md) | The 3×3 icon-grid idle screen, replacing the earlier text-list home |
| [app-icons-build-notes.md](./app-icons-build-notes.md) | The real multi-color icon rendering pipeline and all 9 app icons |
| [snake-build-notes.md](./snake-build-notes.md) | Snake — real game logic, reachable from Settings' About screen, not a top-level grid app |

## Conventions for future build-notes files

- One file per app or subsystem, named `<name>-build-notes.md`.
- Add the new file to the table above the same session it's created.
- Record **decisions made**, not just options considered — mark open
  items explicitly as "Not yet decided" rather than leaving them implicit.
- Where a decision depends on another subsystem (e.g., an app's UI depending
  on the not-yet-defined tezOS visual/interaction design system), note the
  dependency explicitly rather than silently assuming it.
