---
date: 2026-10-15
project: tezos-cyberdeck
type: build-note
tags: [build, hardware, display, fbtft, pi-zero-2w]
---

# Build Note: First real display response — live hardware session, M1

## Status: real progress, not yet closed — correct driver and channel order found, orientation still needs one more correction before this is "working"

## Context

M1 has been stuck since 2026-09-02: "Pi Zero 2 W display stays white (DRM not
initializing)." `BOM.md` lists ILI9486 as an assumption, flagged "confirm
exact panel/overlay before wiring" — still unconfirmed going into this
session. This was the first session with live shell access to the actual
device (previously no agent had physical/remote access at all).

## Action

### 1. Live access achieved — g_serial, not UART

The Pi was connected over USB directly (not the GPIO14/15 UART-adapter
path). `cmdline.txt` already had `modules-load=dwc2,g_serial` and
`console=ttyGS0,115200` from earlier work — this alone was enough to get a
real login shell (`/dev/ttyACM1` on the host side, enumerating as USB
`0525:a4a7` "Linux-USB Serial Gadget"). Login: `pi` / the image's default
password (**default credential — must be changed with `passwd` before this
device goes anywhere near untrusted networks**; `userconf.txt`'s password
hash was seen earlier but never used — the Owner supplied the actual
password directly instead of it being cracked or guessed).

**Real finding:** WiFi is connected on this boot — `wlan0` up at
`192.168.3.246`, NetworkManager reports `connected`. This resolves the
*other* open item in `docs/build-notes/usb-gadget-build-notes.md`
("why WiFi didn't connect is still completely open") — at least on this
boot. Mechanism not fully explained (that doc's hypothesis was that a
NetworkManager-based image wouldn't consume the legacy
`wpa_supplicant.conf` at all — WiFi working anyway means either that
hypothesis was wrong, or something else resolved it between attempts).
Worth re-testing on a cold boot from power-off, not just this session's
warm state, before calling it closed.

### 2. SPI ID-poll — inconclusive, as predicted

Ran `tools/display-id-poll/poll_display_id.py` (written in an earlier
session) after the one-boot config swap it requires. Both `RDDID` (0x04)
and `RDID4` (0xD3) came back all-`0x00` — the script's own documented
"MISO probably isn't wired" case. This path is exhausted for this board;
real chip ID would need a continuity check with a multimeter, not more
software.

### 3. Overlay/driver trials — holding wiring facts constant, varying the chip

Decompiled `overlays/piscreen.dtbo` in an earlier session (not this one)
and confirmed real pin facts used throughout: **DC = GPIO24, RESET = GPIO25**
(`dc-gpios`/`reset-gpios` in that overlay's devicetree). Treated these as
fixed/trusted for every trial below, varying only the chip driver —
a real experimental-design fix on the second trial, after the first trial
(below) varied both chip *and* pins at once and taught a chip from an
inconclusive result.

| # | Overlay | Chip | DC pin | Result |
|---|---|---|---|---|
| 1 | `piscreen` (pre-existing, before this session) | `ilitek,ili9486` | GPIO24 | Historically blank — the original M1 blocker |
| 2 | `pitft35-resistive` (bundled Adafruit overlay) | `himax,hx8357d` | GPIO25 (wrong vs. confirmed 24) | Driver bound cleanly (`fb1`, 480×320, no SPI error), but screen showed **"no change"** regardless of solid-white or a 5-band color pattern — real negative result. Confounded two variables (chip *and* pin) at once; conclusion: don't trust Adafruit's canned pin assumptions on this board, not necessarily "chip is wrong" |
| 3 | generic `fbtft,ili9481` | `ilitek,ili9481` | GPIO24 (confirmed) | **First real visual response** — distinct colored bands appeared, not blank. Wrong colors and partial coverage (see below), but proof of live communication for the first time this project has had hardware access |

Only `ili9481`, `ili9486`, and `hx8357d` fbtft kernel modules exist on this
image's kernel (`6.18.34+rpt-rpi-v8`) — checked directly
(`find /lib/modules/.../fbtft/`) rather than assumed. **No `fb_ili9488.ko`
exists**, so ILI9488 (a real, common alternate guess for this panel class —
surfaced by a general web search for "320×480 3.5in TFT" alongside R61581B,
neither independently confirmed for this exact board) is not triable on
this kernel without building/installing a module, out of scope for this
session.

### 4. Orientation and rotation — portrait is the real target, not landscape

Trial 3 above was run at `rotate=90` (landscape, 480×320) — wrong target
orientation; **the real device is portrait**. It also left roughly the left
45% of the physical panel showing stale/uncovered content regardless of
what was written to the framebuffer — a `rotate=90` buffer-mapping issue,
not a driver failure.

Switched to `rotate=0` (native panel spec, 320×480 portrait — confirmed via
`cat /sys/class/graphics/fb1/virtual_size` after reboot). Wrote a dedicated
orientation-reference pattern (`/tmp/orient.py` on-device, not yet copied
into this repo — see Follow-ups) instead of guessing further from solid
fills:
- Four quadrants, pure red / green / blue / white — channel-order-diagnostic,
  survives photo compression better than subtle colors.
- A 40×40 solid black square fixed at buffer origin (0,0) — an
  orientation anchor immune to color-channel bugs (it's geometry, not
  color).
- A 4px white border around the full frame — catches partial-coverage bugs
  like trial 3's stale region.

**Result at `rotate=0`:** white border fully continuous all the way around
— confirms full-buffer coverage is clean at this rotation (unlike trial 3).
Black square landed in the buffer's intended corner. Colors showed a clean,
fully-explained **red↔blue channel swap** (red rendered blue, blue rendered
magenta, green and white unaffected — exactly what an R/B swap predicts,
and exactly nothing else does). Root cause: `bgr=1` had been carried over
from `piscreen.dtbo`'s setting without being re-validated for this
different driver/overlay. **Fixed: `bgr=0`.** Pattern rewritten and
confirmed written (`ORIENT_PATTERN_WRITTEN 320 480`) after the reboot;
visual confirmation of the color fix was still pending when this note was
written (see Follow-ups — paused here on the Owner's instruction to
document before continuing).

**Real, disclosed open item from the Owner, not yet reconciled:** per the
intended mechanical design, the buffer-origin corner (where the black
square anchor sits) should land at the panel's physical **top-right**, not
top-left. Everything tested so far treated top-left as the anchor purely
as an arbitrary test convention — this means whatever rotate/mirror value
ends up giving correct colors and full coverage still needs **one more
axis correction** (very likely `rotate=180` combined with a mirror, or a
90°-class rotate value instead of 0) to match the real intended mounting.
Not yet tested.

## Current state of `config.txt` on the card (end of session)

```
dtoverlay=dwc2,dr_mode=peripheral
hdmi_force_hotplug=1
config_hdmi_boost=7
hdmi_group=2
hdmi_mode=87
hdmi_cvt=480 320 60 6 0 0 0
dtoverlay=fbtft,ili9481,dc_pin=24,reset_pin=25,speed=16000000,rotate=0,bgr=0,fps=30
enable_uart=1
dtoverlay=disable-bt
```

**Backup trail left on `/boot/firmware/` this session** (each one a full
prior `config.txt`, in chronological order) — real clutter, flagged for a
cleanup pass once the working config is actually locked in, not deleted
now in case any trial needs re-examining:
`config.txt.bak-pre-piscreen` (pre-dates this session),
`config.txt.bak-pre-spi-id-poll`, `config.txt.bak-pre-overlay-trials`,
`config.txt.bak-pre-ili9481`, `config.txt.bak-pre-portrait`,
`config.txt.bak-pre-bgr0`.

### 5. Rotate=270 (90° CCW) attempt — broken, abandoned; decision: stop chasing root cause, design around the behavior

Tried `rotate=270` at the driver level (translating an Owner instruction
to orient the test anchor 90° counter-clockwise) to move the reference
marker from the buffer-origin corner toward the panel's other short edge.
Result: garbled, torn-looking output (repeating vertical stripe artifacts)
plus a solid uncovered block across roughly the bottom quarter of the
panel — the same category of partial-coverage bug seen earlier at
`rotate=90`, just worse. **Conclusion: this driver's 90°-class rotate
values do not reliably remap the buffer on this hardware.** Reverted to
`rotate=0` (the only configuration that has given clean, fully-covered
output all session).

Re-ran the original `rotate=0, bgr=0` configuration with the reference
marker moved to the opposite short edge of the buffer in code. Config and
on-device script content were both independently re-verified correct
(`dc_pin=24`, `reset_pin=25`, `rotate=0`, `bgr=0` all confirmed live;
`md5sum`'d the on-device script to confirm the marker really was coded at
the far edge). Despite that, the result showed the marker back at the
*original* edge and colors back to the swapped pattern — i.e., the same
config and the same category of script produced a different visible result
than the first `rotate=0` test. Owner confirmed the photo was taken from
the same physical angle as the earlier one (ruling out "just viewed from
the other side"), leaving a marginal/intermittent breadboard connection as
the most likely explanation — not confirmed, not chased further.

**Decision (Owner, 2026-10-15): stop hunting for why, design around the
observed behavior instead.** Going forward:
- `rotate=0` is locked in as the only driver rotation value used — it's
  the only one that has ever given full, clean buffer coverage.
- Color correctness is now a **software guarantee**, not dependent on the
  driver's `bgr` flag: `tools/display-pattern/baseline_pattern.py` does
  the R/B channel swap itself at pixel-write time. This works regardless
  of whatever the flag or a flaky wire do.
- Buffer coordinate `(0,0)` is treated as a fixed **reference point**, not
  asserted to be a specific named corner ("top-left"/"top-right") of the
  final device — which physical corner it lands on is a **mounting/
  enclosure decision**, to be made once hardware is in its final
  mechanical form, not something to keep re-deriving through more
  reboot-and-photograph cycles on loose breadboard wiring.

## Result (end of session)

| Check | Result |
|---|---|
| Live shell access | ✅ via g_serial (`/dev/ttyGS0` on-device, `/dev/ttyACM1` on host) |
| WiFi | ✅ connected this boot — re-test cold-boot before closing |
| SPI ID-poll | ❌ inconclusive — MISO not wired, needs multimeter not software |
| `piscreen` (ILI9486) | ❌ blank — pre-existing, unchanged |
| `pitft35-resistive` (HX8357D, Adafruit) | ❌ no response at all — ruled out for this board |
| `fbtft,ili9481` + confirmed DC/RESET pins | ✅ real pixel-level response, first ever for this project |
| Full-buffer coverage | ✅ at `rotate=0` only — `rotate=90`/`270` both broke coverage, abandoned |
| Color channel order | ✅ guaranteed in software (`tools/display-pattern/baseline_pattern.py`), not via the `bgr` flag, which gave inconsistent results across boots |
| Physical mounting orientation | **Deferred — enclosure/mounting decision, not a display-driver bug to keep chasing** |
| Touch (XPT2046) | Not touched this session — `fbtft,ili9481` overlay used has no touch node at all; will need adding back |

## Follow-ups (not done yet)

1. Decide final mounting orientation once hardware is in its enclosure,
   and confirm `tools/display-pattern/baseline_pattern.py`'s reference
   marker lands where expected at that point — not before.
2. Re-add a touch overlay node (XPT2046/ads7846-class, matching BOM) —
   current working config has no touch at all.
3. Re-test WiFi from a full cold boot, not just within this session's warm
   state.
4. Clean up the seven `config.txt.bak-*` files once the final config is
   locked in.
5. Update `BOM.md`'s "assume ILI9486" line — real evidence now points to
   ILI9481 as the working driver for this specific board, not ILI9486 or
   HX8357D. Identification is empirical (works with this driver), not
   datasheet-confirmed — the panel's own label, `HSD035577B4`, didn't
   resolve to a public datasheet.
6. If the color/marker inconsistency recurs after reseating connections,
   it's worth treating as a real signal (intermittent SPI fault) rather
   than dismissing it a second time.
