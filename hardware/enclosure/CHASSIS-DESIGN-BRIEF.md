# Chassis Design Brief

**Status: real research done, no CAD/modeling started.** This
replaces the old placeholder (`# Status: not yet started`) with an
actual design brief — real, sourced component dimensions and design
considerations to model against in Blender. It is not a finished
design, and nothing here has been built or fit-tested; every
dimension below is sourced from manufacturer/datasheet-level
information, cited so it can be double-checked before anything gets
cut or printed.

**Confirmed direction**: the signer lives inside the same chassis as
the main device (not a separate tethered unit) — a real, firm
constraint, not one of several options. Early builds use the Pi Zero
2 W; a Compute Module 4 migration is planned later, which has real,
concrete implications for internal volume (see below). Modeling in
Blender, 3D printed.

---

## 1. Real component footprints

| Component | Real dimensions | Source / confidence |
|---|---|---|
| Pi Zero 2 W (bare board) | 65mm × 30mm × 5mm | Official Raspberry Pi product brief |
| Pi Zero 2 W (with SD card + USB connector overhang) | ~68mm × 32mm | Real, measured community report (Raspberry Pi forums) |
| Pi Zero 2 W (with soldered GPIO header) | 65mm × 30mm × 12mm | Real vendor spec (PiCockpit) |
| CM4 module itself | 55mm × 40mm × 4.7mm | Official Raspberry Pi spec |
| CM4 *official* reference carrier board | 160mm × 90mm | Official Raspberry Pi CM4 IO Board spec — **too large for this chassis as-is** |
| CM4 *compact* third-party carriers (real, existing examples) | roughly 55mm × 62mm and up | Real, existing third-party boards (e.g. DFRobot's CM4 IoT carrier) — **a custom carrier for this project doesn't exist yet, so its exact size is still unknown** |
| 3.5" ILI9486+XPT2046 SPI display module (PCB) | Real range observed: ~85–97mm × 55–60mm, varies by vendor | Multiple real vendor listings — **the exact module this project sources should be confirmed against its own datasheet once picked**, since generic "3.5 inch ILI9486" listings vary meaningfully between vendors |
| Same display's active viewing area | ~73mm × 49mm | Consistent across the vendor listings checked |
| Pi Camera Module Rev 1.3 (bare board) | ~25mm × 24mm × 9mm | Multiple, independently converging real sources |
| TCA8418 keypad matrix | **Not yet determined** | Real, open item — depends on the final key layout/count, which isn't decided yet |
| Signer MCU (F410 / Pico 2 / MSP430FR, whichever is chosen) | Small (chip itself typically under 10mm square); real breakout/carrier board size depends on which is chosen | Real, open item — the chip decision itself is still open (see the earlier signer-chip discussion) |
| Battery (PiSugar or similar) | **Not yet specified** | Real, open item — no specific model has been chosen yet |

## 2. The real, concrete implication of the CM4 migration plan

The *official* CM4 carrier board (160×90mm) is a full development
board, not something to design a handheld chassis around — it's
larger than a Pi Zero 2 W by a wide margin. Real, existing compact
third-party CM4 carriers show it's genuinely possible to get much
smaller (some in the 55×62mm range), but **this project doesn't have
its own compact CM4 carrier board designed yet** (per
`hardware/carrier-board/STATUS.md`, that's explicitly Stage 2, planned
*after* the Pi Zero 2 W stage). That means its exact real footprint
can't be pinned down precisely today.

**Practical recommendation**: design the chassis's main electronics
bay with real, deliberate extra internal volume beyond the bare
minimum the Pi Zero 2 W needs — enough that a somewhat larger custom
CM4 carrier board could plausibly fit later without a full chassis
redesign, even though the exact number can't be nailed down until
that carrier board is actually laid out. This is a real, honest
trade-off (a roomier internal cavity than the Zero alone strictly
needs) in exchange for not having to redesign the whole enclosure at
Stage 2.

## 3. Aesthetic direction already on record

`BOM.md` already states **"Nokia/Game Boy Camera-inspired form
factor"** — a real, existing direction, not something to decide from
scratch. This is consistent with everything else already built:
Press Start 2P for headers, the boot splash's retro gleam animation,
the whole dark-navy/electric-blue/cyan/pink palette (see
`docs/design-system/PALETTE.md`).

## 3.5. Real, direct next step — a known-parts stack-up before any case design

**Real, direct instruction**: before any actual enclosure/case
modeling starts, get the assembly of known real parts arranged into a
real physical stack — understand how the actual components sit
relative to each other in space, before designing a shell around a
guess.

**A real, concrete starting point using the dimensions already
researched above**, given the v0 decision (magnet wire harness, not a
flat carrier PCB — see `hardware/carrier-board/STATUS.md`) means this
stack doesn't need to accommodate a rigid board's flat footprint yet,
just each real component's own real envelope, connected by real wire
runs rather than PCB traces:

| Layer (bottom to top, a real starting proposal) | Component | Real footprint |
|---|---|---|
| 1 | Pi Zero 2 W | 65×30×5mm bare, up to 12mm with header pins — decide early whether headers are soldered flat or a low-profile connector is used, since that's a real 7mm difference in this layer alone |
| 2 | 3.5" display module | ~85–97×55–60mm PCB (vendor-dependent, real exact module TBD), ~73×49mm visible area — likely the single largest real footprint in the whole stack, probably the layer that drives the chassis's real overall X-Y dimensions |
| 3 | TCA8418 keypad matrix | Real footprint TBD — depends on final key count/layout, still undecided (see the icon/menu concept above, which affects key count too) |
| Separate, not stacked | Camera Module | 25×24×9mm — likely mounted at an edge/face for lens access, not stacked flat under the display |
| Separate, not stacked | Signer board (F412, once real hardware exists) | Real footprint TBD — chip itself is small, but the real breakout/carrier isn't designed yet |
| Separate, not stacked | Battery | Real footprint TBD — no specific model chosen yet |

**This is a real, honest starting proposal, not a finished
arrangement** — it's meant to be the actual thing checked and adjusted
in Blender against the real parts once they're in hand, not treated as
already decided. The real, useful next step this project can take
before that: physically laying the actual components out on a flat
surface, measuring real clearances needed for the v0 wire harness's
bend radius and connector heights, and photographing/sketching that
real arrangement — the same real information a CAD stack-up would
capture, just gathered by hand first.

## 4. Real layout considerations, not yet resolved into an actual model

- **Camera port placement**: needs a real opening for the CSI-connected
  Camera Module's lens, positioned so Scan mode (aimed at a screen/QR
  code) and Capture mode (aimed at a subject) are both genuinely usable
  — these are two different real use postures, worth thinking through
  rather than picking one orientation by default.
- **Display + keypad relationship**: a Nokia/Game Boy Camera-style
  form factor typically means the display sits above the keypad on one
  face — real, but the exact split (how much vertical space each gets)
  depends on the final key count/layout, still undecided.
- **Signer's physical placement**: since it must live inside the same
  chassis, real thought is needed on whether it's a separate small PCB
  tucked in its own compartment (real benefit: physically easier to
  swap/revise independently of the main carrier board, given the chip
  choice itself isn't finalized) versus integrated onto the same board
  as everything else.
- **Cable routing and connector access**: the real, physical USB port
  (for the gadget-mode work already documented) and the microSD card
  need real, accessible openings — worth deciding now which face/edge
  they live on, since that shapes the whole shell split.
- **Ventilation**: the Pi (and battery, under load) generate real heat;
  a fully sealed shell risks thermal issues, worth a real vent
  consideration rather than an afterthought.

## 5. Real, general FDM 3D-printing considerations, since printing is the confirmed manufacturing method

- **Wall thickness**: 2–3mm is a common, real starting point for a
  handheld enclosure needing both rigidity and printability — thinner
  risks warping/fragility, thicker adds unnecessary weight and print
  time.
- **Material**: PETG is a real, common choice over PLA for anything
  that sits near a running Pi + battery, since PLA softens at lower
  temperatures than PETG typically tolerates — worth deciding
  deliberately rather than defaulting to PLA out of habit.
- **Assembly method**: screws into heat-set brass inserts are a real,
  common, more durable choice than pure snap-fit tabs for something
  meant to be opened/serviced repeatedly (e.g., to access the SD card,
  or to swap the signer board later) — worth deciding now since it
  affects where mounting bosses need to go in the model.

## 6. Real, open questions worth deciding before modeling goes far

1. Final TCA8418 key count/layout (blocks §4's display/keypad split).
2. Which signer chip (blocks knowing its real breakout footprint).
3. Which specific 3.5" display module, by exact part/vendor (blocks
   knowing its real, precise PCB dimensions rather than a vendor-
   dependent range).
4. Which battery/power module.
5. Screw-together vs. snap-fit assembly (affects the model's mounting
   boss placement from the start, not just a late detail).
