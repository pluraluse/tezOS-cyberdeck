# Display ID poll

Reads the 3.5" TFT panel's controller ID registers directly over raw SPI,
bypassing the `piscreen`/fbtft/DRM stack entirely. Written to settle
BOM.md's open "confirm exact panel/overlay before wiring" item with a real
hardware readback instead of another guess.

## Why not just try the Adafruit overlay instead?

Decompiled both overlays already on this exact SD card image
(`sudo apt install device-tree-compiler`, then
`dtc -I dtb -O dts overlays/piscreen.dtbo`) to check, rather than guessing:

| | `piscreen.dtbo` (currently active) | `pitft35-resistive.dtbo` (Adafruit, bundled, unused) |
|---|---|---|
| Display controller | `ilitek,ili9486` | `himax,hx8357d` |
| Touch controller | `ti,ads7846` (XPT2046-class) | `st,stmpe610` |
| DC pin | GPIO 24 | GPIO 25 |
| Touch IRQ pin | GPIO 17 | GPIO 24 |

Different controller IC, different touch IC, and the DC pin is wired to the
*other* chip's touch-IRQ role between the two. Swapping to the Adafruit
overlay on this board as-is would not work — it's not a pin-for-pin or
chip-for-chip compatible drop-in, it just happens to ship on the same SD
card image as part of the stock Raspberry Pi OS overlay collection. That's
why this script exists: find out what's actually on the board before
picking an overlay, rather than trying overlays as guesses.

## Prerequisite — free up the SPI bus for raw access

The `piscreen` overlay's own devicetree fragment explicitly disables the
generic `spidev0` node for SPI0 CS0 while it's bound (confirmed by reading
the decompiled overlay, not assumed) — so `/dev/spidev0.0` will not exist
while `dtoverlay=piscreen` is active in `config.txt`. For this one
diagnostic boot:

1. In `/boot/firmware/config.txt`, comment out:
   ```
   #dtoverlay=piscreen
   ```
2. Add:
   ```
   dtparam=spi=on
   ```
3. Reboot.
4. Run the script (see below).
5. **Revert step 1–2** before returning to normal use — the display won't
   render anything while `piscreen` is disabled.

## Running it

```bash
sudo apt install python3-spidev python3-rpi.gpio   # if not already present
python3 poll_display_id.py
```

Needs `sudo`/root or membership in the `spi`/`gpio` groups, depending on
the image's udev rules.

## Reading the result

- Real, non-trivial bytes back (not all-`0x00` or all-`0xFF`) → compare
  against the script's small lookup table and, failing that, against
  candidate datasheets by hand. This is the useful case.
- All-`0x00` or all-`0xFF` on both commands → most likely the board's MISO
  line simply isn't wired to the host (common on cheap write-only clone
  breakouts), not that the chip has no ID. In that case this approach is
  exhausted — would need a multimeter/continuity check on the board itself
  to confirm MISO wiring, which is real hands-on hardware work, not
  something resolvable from software.
