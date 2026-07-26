# Hardware Roadmap — Build Notes

Two-stage path from breadboard prototype to a real custom device. Do not
skip to Stage 2 before Stage 1's software is proven on real hardware.

## Stage 1 (near-term): custom carrier board for the Pi Zero 2 W

- Does **not** embed the SoC itself. The Zero 2 W plugs into the carrier
  board via its existing 40-pin header + CSI camera connector.
- Replaces breadboard/jumper wiring with proper traces and connectors for:
  display (SPI0 + touch), TCA8418 keypad decoder (I2C), battery management
  (PiSugar-equivalent, I2C), signer UART link (with inline mechanical
  disconnect switch).
- Low risk, immediate manufacturability win. This is the version that
  should exist before any community "build your own" documentation ships.

## Stage 2 (later, once software is proven): migrate to a Compute Module

Real, current, well-trodden path — Compute Modules are system-on-modules
built specifically for embedding into custom carrier boards/products,
using high-density board-to-board connectors instead of a header.

### CM4 vs CM5 — a real tradeoff, not a strict upgrade

| | CM4 | CM5 |
|---|---|---|
| Silicon | Pi 4 class | Pi 5 class (BCM2712) |
| Power draw | Closer to current Zero 2 W budget | Meaningfully higher |
| Headroom | Matches current software targets | Real extra headroom (faster IPFS ops, heavier camera processing) |
| Battery impact | Better fit for portable/battery goals | Cuts against battery-life goals |

- **CM4**: closer in power tier to the Zero 2 W already being built against —
  existing software targets stay realistic without re-tuning for a faster
  chip, and it's more power-frugal, which matters for a battery-powered
  device.
- **CM5**: based on Raspberry Pi 5 hardware, with configurable RAM/eMMC,
  PCIe, USB 3.0, and camera/display interfaces — real extra performance
  headroom, at the cost of higher power draw and heat.

### eMMC vs microSD (relevant if/when moving to a CM)

Non-Lite CM variants offer onboard eMMC — better reliability and speed than
a consumer microSD card, at the cost of losing the "just swap the card"
convenience of the current setup.

### Decision status

**Not yet decided, and shouldn't be** until it's clear whether the Zero 2 W's
performance is actually a limiting factor for something real (not a
guess). Build Stage 1 now; treat the CM migration and CM4-vs-CM5 choice as
a later decision driven by an actual observed bottleneck.
