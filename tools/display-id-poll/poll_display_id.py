#!/usr/bin/env python3
"""
poll_display_id.py — identify the real SPI TFT controller IC by reading its
ID registers directly, bypassing fbtft/DRM entirely.

Why this exists: BOM.md assumes ILI9486 for the 3.5" 320x480 panel but says
"confirm exact panel/overlay before wiring" — and M1 in ROADMAP.md has been
stuck on a white screen. The `piscreen` overlay currently in config.txt
declares `compatible = "ilitek,ili9486"` with its own tuned init sequence,
but that's the overlay author's assumption, not a hardware readback. This
script asks the chip directly instead of trusting any overlay's label.

PREREQUISITE — config.txt must NOT have the panel overlay active:
    The piscreen overlay (decompiled from this exact image's
    overlays/piscreen.dtbo) explicitly disables the generic spidev0 node
    for SPI0 CS0 while it's bound — so /dev/spidev0.0 will not exist while
    `dtoverlay=piscreen` is active. For this diagnostic boot only:

        1. Comment out `dtoverlay=piscreen` in /boot/firmware/config.txt
        2. Add `dtparam=spi=on` (enables the generic spidev driver instead)
        3. Reboot
        4. Run this script
        5. Revert config.txt to restore normal display operation

    See README.md in this directory for the exact lines.

PIN ASSIGNMENTS — confirmed, not guessed:
    Pulled directly from this image's own overlays/piscreen.dtbo
    (decompiled with `dtc -I dtb -O dts`), not assumed from a generic
    board-model guide:
        DC (data/command)  -> GPIO 24  (dc-gpios, active high)
        RESET               -> GPIO 25  (reset-gpios, active low)
        LED (backlight)     -> GPIO 22  (unused by this script)
        Touch IRQ (ads7846) -> GPIO 17  (unused by this script)

WHAT IT DOES:
    1. Hardware-resets the panel via the RESET GPIO.
    2. Sends MIPI DCS command 0x04 (Read Display ID) and reads back 4 bytes.
    3. Sends ILI948x-family command 0xD3 (Read ID4) and reads back 4 bytes.
    4. Prints the raw bytes from both and a best-effort guess against a
       small table of commonly-reported controller signatures.

HONEST LIMITS:
    - Many cheap clone TFT breakout boards do not wire the panel's MISO
      line back to the host at all (write-only designs are common and
      cheaper) — if every read comes back as all-0x00 or all-0xFF, that is
      the most likely explanation, not a "no controller" result. This
      script says so explicitly rather than guessing a false match.
    - The ID lookup table below is drawn from values commonly reported in
      hobbyist tooling for this exact purpose (e.g. the long-standing
      MCUFRIEND_kbv Arduino library's auto-detect table), not independently
      re-verified against each chip's datasheet here. Treat a "guess" as a
      lead, not a confirmed identification — the raw bytes are the actual
      result; the lookup is a convenience on top of it.
"""

import sys
import time

try:
    import spidev
except ImportError:
    sys.exit(
        "error: the 'spidev' module isn't installed. On Raspberry Pi OS: "
        "sudo apt install python3-spidev   (or: pip3 install spidev)"
    )

try:
    import RPi.GPIO as GPIO
except ImportError:
    sys.exit(
        "error: the 'RPi.GPIO' module isn't installed. On Raspberry Pi OS: "
        "sudo apt install python3-rpi.gpio"
    )

DC_PIN = 24    # dc-gpios in overlays/piscreen.dtbo
RESET_PIN = 25  # reset-gpios in overlays/piscreen.dtbo

SPI_BUS = 0
SPI_DEVICE = 0  # CS0 — matches piscreen@0's `reg = <0x00>`
SPI_SPEED_HZ = 1_000_000  # slow and safe for a diagnostic read, not 24 MHz

CMD_RDDID = 0x04   # MIPI DCS "Read Display ID"
CMD_RDID4 = 0xD3   # ILI9341/9486/9488-family "Read ID4"

# Commonly-reported signatures for CMD_RDID4's 3 ID bytes (after the leading
# dummy byte). Source: values widely cited in hobbyist auto-detect tooling
# (e.g. MCUFRIEND_kbv), not independently re-verified against a datasheet
# in this pass. Treat as a lead, not a confirmed match.
KNOWN_ID4 = {
    (0x00, 0x93, 0x41): "ILI9341",
    (0x00, 0x94, 0x86): "ILI9486",
    (0x00, 0x94, 0x88): "ILI9488",
    (0x00, 0x80, 0x57): "HX8357 family (RDID4 not standard for this family — low-confidence match)",
}


def gpio_setup():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(DC_PIN, GPIO.OUT, initial=GPIO.HIGH)
    GPIO.setup(RESET_PIN, GPIO.OUT, initial=GPIO.HIGH)


def hardware_reset():
    """Pulse RESET low per the overlay's active-low reset-gpios flag."""
    GPIO.output(RESET_PIN, GPIO.HIGH)
    time.sleep(0.02)
    GPIO.output(RESET_PIN, GPIO.LOW)
    time.sleep(0.02)
    GPIO.output(RESET_PIN, GPIO.HIGH)
    time.sleep(0.15)  # panel needs time to come up after reset


def read_command(spi, cmd, nbytes):
    """Send one command byte (DC low), then read nbytes back (DC high)."""
    GPIO.output(DC_PIN, GPIO.LOW)
    spi.xfer2([cmd])
    GPIO.output(DC_PIN, GPIO.HIGH)
    return spi.xfer2([0x00] * nbytes)


def describe(label, raw):
    hexed = " ".join(f"0x{b:02X}" for b in raw)
    if all(b == 0x00 for b in raw) or all(b == 0xFF for b in raw):
        print(f"{label}: {hexed}  -> no real response (all-0x00/0xFF). "
              f"Most likely cause: this board's MISO line isn't wired back "
              f"to the host at all, not that the chip lacks an ID. This is "
              f"the common case for cheap write-only clone breakouts.")
        return
    print(f"{label}: {hexed}")
    if len(raw) == 4:
        key = tuple(raw[1:4])
        guess = KNOWN_ID4.get(key)
        if guess:
            print(f"  -> matches known signature: {guess}")
        else:
            print("  -> no match in the local lookup table; "
                  "look these bytes up against candidate datasheets by hand.")


def main():
    gpio_setup()
    spi = spidev.SpiDev()
    spi.open(SPI_BUS, SPI_DEVICE)
    spi.max_speed_hz = SPI_SPEED_HZ
    spi.mode = 0b00

    try:
        hardware_reset()
        rddid = read_command(spi, CMD_RDDID, 4)
        describe("RDDID  (cmd 0x04)", rddid)
        rdid4 = read_command(spi, CMD_RDID4, 4)
        describe("RDID4  (cmd 0xD3)", rdid4)
    finally:
        spi.close()
        GPIO.cleanup()


if __name__ == "__main__":
    main()
