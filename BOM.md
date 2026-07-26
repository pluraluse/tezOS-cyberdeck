# Bill of Materials & Feature Prioritization

## Hardware BOM

### Need (v1, can't ship without)
| Component | Notes |
|---|---|
| Raspberry Pi Zero 2 W | Main compute board |
| 3.5" 320x480 SPI TFT (ILI9486) + XPT2046 resistive touch | Confirm exact panel/overlay before wiring |
| TCA8418 keypad matrix/GPIO expander breakout (I2C) | Decodes numeric matrix + softkeys + up/down, zero extra Pi GPIO |
| Nokia-style numeric keypad (12-key matrix, sourced or built) | Wired into TCA8418 row/columns |
| 2x tactile switches (softkeys) + 2x tactile switches (Up/Down) | Wired into TCA8418 spare GPIO pins |
| Signer board: STM32F401 *or* Pi Pico (RP2040) | Bare-metal, airgapped, wired (not wireless) |
| Small OLED (signer transaction display) | 128x64-class, I2C/SPI |
| Mechanical inline disconnect switch | On signer's UART data lines (not power), signer side |
| Pi Camera module (CSI, ~1080p) + Zero-size CSI cable adapter | Note: Zero uses smaller CSI connector than full-size Pi boards |
| PiSugar-style UPS/battery HAT *or* 2x 18650 + charge/protection circuit | Power |
| microSD card | Boot + storage |

### Want (high value, soon after v1)
| Component | Notes |
|---|---|
| Custom carrier board (Stage 1, see hardware roadmap notes) | Replaces breadboard/jumper wiring |
| 3D-printed enclosure | Nokia/Game Boy Camera-inspired form factor |

### Nice to have (deferred/optional)
| Component | Notes |
|---|---|
| TCA8418-based I2C keypad IC upgrade path | Only needed if key count grows beyond a 4x4-class matrix |
| Compute Module (CM4 or CM5) + custom carrier | Stage 2 hardware migration — only if Zero 2 W proves to be an actual bottleneck |
| eMMC storage (via CM) | Replaces microSD reliability/speed concerns, CM-dependent |

## Software / licensing decisions (already made)
| Decision | Choice |
|---|---|
| Core language | C (matches signer firmware language; one language across the whole project) |
| Software license | MIT |
| Hardware design license | CERN-OHL-P |
| Render path | DRM/KMS (`piscreen`-style overlay), not legacy fbtft |
| App distribution | Curated repo only — no open app store, deliberately |
| Base OS (bring-up) | Raspberry Pi OS Lite → leaner custom image later |

## Feature prioritization

### Need (v1 core loop)
- Wallet: send/receive/sign, delegation/staking, `.tez` domain resolution
- Explorer: address/block/operation lookup, "Graveyard" abandoned-contract mode
- Scanner: generic entrypoint decode/fill/forge
- Gallery: owned-token viewer (TZKT balances)
- Settings: device name, profiles (PIN-gated personalization, not separate
  wallets), WiFi, storage, network selection (Mainnet/Ghostnet), About
- Camera: Scan mode (full-spec, Beacon QR) + Capture mode (4 resolution
  presets x 4 color-depth modes, box-average downsample, no upscale step)
- Signer: serial protocol, physical disconnect switch, on-device operation
  decode/display before signing

### Want (post-v1, already scoped)
- Discover: objkt/Teia/bootloader.art browsing, buy via Scanner handoff
- Messenger: Teia Channels V2 client (DMs, channels, comments)
- 1-bit art drawing app, feeding the same minting pipeline as Camera
- Beacon (TZIP-10) signer integration for third-party dApp compatibility

### Nice to have (explicitly deferred)
- HEN v2 collection cloning (origination, user-as-creator)
- Minimal DeFi swap (single DEX/aggregator via Scanner)
- Local mesh chat (offline conference/badge social)
- Protocol governance read-only display
- Token-gated builders' channel (Merkle allowlist gating)
- GLB 3D asset viewing in Gallery (explicitly an afterthought vs. owned-token thumbnails)
