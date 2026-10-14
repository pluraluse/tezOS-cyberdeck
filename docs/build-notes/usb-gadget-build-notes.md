# Headless Access — USB Gadget Mode & UART Console — Build Notes

## Status: real, researched, documented; real hardware attempts so far — boots, but no WiFi and no readable USB yet

**This is OS/config-level work, not application code** — nothing here
touches `src/`, and none of it can be verified from this sandbox (no
real Pi, no real USB host to plug it into). Written up with the same
discipline as everything else in this project regardless: real,
current sources checked before recommending anything, real reasoning
recorded, real open items disclosed plainly.

## Why this needs explicit configuration at all

The Pi Zero 2 W's data USB port doesn't automatically present as
anything recognizable to a connected computer — by default the port
can act as a USB *host* (for a keyboard/mouse/etc.), not a USB
*device*. Making a computer actually "see" it when plugged in requires
explicitly putting the USB controller into peripheral ("gadget") mode
and telling it what to present itself as.

## The real decision: `g_ether` alone, not a combined gadget

Two real, independent, converging sources drove this, not a guess:

1. **The Raspberry Pi Foundation's own official tool
   (`rpi-usb-gadget`, confirmed current — one real source dated
   January 2026) is deliberately Ethernet-only.** Direct quote from
   their own documentation: *"CDC-ACM (serial over USB) is not
   included, as Windows cannot bind both RNDIS/ECM and ACM to a single
   composite USB device using one .inf file without vendor-specific
   drivers."* This is the Pi Foundation explicitly choosing not to
   combine USB functions in their own supported tool, specifically
   because doing so breaks clean driver binding on Windows.
2. **An independent builder who actually built a composite
   Ethernet+mass-storage gadget reached the identical conclusion, for
   the identical reason** (Ben Hardill, real blog post): *"The best
   way that I can find is to use the g_ether module (as the composite
   module does not work under windows...)."* The same source also
   surfaced a second, separate real problem with mass-storage mode
   specifically: *"when you add a USB Mass Storage option it can not
   be safely mounted on both the Pi and the Host computer at the same
   time"* — a genuine data-corruption risk when both sides can touch
   the same backing storage, not just an inconvenience.

**Given this project's two real, stated needs — developer SSH access
over USB, and end-user file transfer (e.g., pulling Camera's captured
art off the device) — `g_ether` alone genuinely covers both.** Once
the Pi is reachable over USB via SSH, file transfer is already solved
via `scp`/`sftp`/`rsync` over that same USB-network link — with a real
advantage over mass-storage mode: it's the **live filesystem**, not a
frozen disk-image snapshot, and there's no simultaneous-mount
corruption risk to manage.

## The real, current setup (Debian 13/Trixie — matches this project's actual dev machine)

This project's own earlier Pi bring-up notes already confirmed the
real dev machine (`.63`) runs Debian 13. The current, officially
supported path for a Trixie-based Raspberry Pi OS is the
`rpi-usb-gadget` package (real, from the `raspberrypi` GitHub
organization):

```bash
sudo apt install rpi-usb-gadget
```

Per the package's own real documentation: it configures the kernel's
`g_ether` driver, auto-negotiates CDC-ECM vs. RNDIS based on what the
connected host supports (no manual mode selection needed), and runs a
background watcher that automatically switches between two real modes:

- **Client mode**: the Pi is a DHCP client of the host, used
  automatically when the host has Internet Connection Sharing enabled
  (recognizing common ICS gateway addresses like `192.168.137.1`,
  `192.168.2.1`, `10.42.0.1`).
- **Shared mode**: the Pi provides DHCP+NAT to the host itself, at
  `10.12.194.1/28`, when no ICS gateway is detected.

**Real, disclosed requirement**: per the package's own docs, this must
use the port labeled **USB**, not **PWR IN** — the power-only port has
no data lines at all, gadget mode is physically impossible on it.

## Fallback: the manual, still-valid path

If the package isn't available (an older image, or a deliberate choice
to avoid its automatic mode-switching), the real, traditional manual
configuration still works, confirmed across multiple current sources:

**`/boot/firmware/config.txt`** (append):
```
dtoverlay=dwc2,dr_mode=peripheral
```
Forcing `dr_mode=peripheral` explicitly (rather than leaving it in
auto-detect OTG mode) is the real, more reliable choice — auto-detect
depends on the cable/adapter correctly pulling the ID pin to signal
device-vs-host, which multiple real reports show frequently fails
silently (the port powers up, nothing enumerates, no error shown).

**`/boot/firmware/cmdline.txt`** (append after `rootwait`, same line,
space-separated, not a new line):
```
modules-load=dwc2,g_ether
```

Reboot, then connect via the real USB (not PWR IN) port. On the host,
this should show up as a new network interface (an RNDIS/Ethernet
Gadget on Windows, an `enx...`/`usb0`-style interface on Linux/macOS).

## Real hardware update — boots, but no WiFi, no readable USB yet

A real attempt was made on actual hardware: the device **boots
successfully** — a genuine, positive signal that the base image and
kernel fundamentals work. But WiFi did not connect, and nothing was
readable over the USB gadget setup. Combined, that means there's
currently no way to reach the device over any network at all — no
WiFi, no USB-ethernet. This is exactly the situation UART console
(below) exists to get around: a real, independent fallback that
doesn't depend on either of those working.

## A real, additional third option: `g_serial` (boot directly into a shell over USB)

Distinct from both UART (below) and `g_ether` (above): `g_serial` (USB
CDC-ACM) presents a virtual serial port over the same USB data cable,
giving a real login shell the instant it's plugged in — no separate
UART adapter, no WiFi, no SSH. Real, concrete setup files and
instructions: `config-txt-additions-for-usb-shell.txt` and
`BOOT-TO-USB-SHELL-SETUP.md` (in the project's outputs). Deliberately
kept as its own separate gadget function, not combined with `g_ether`
— same real reasoning as the "why `g_ether` alone" section above:
combining RNDIS/ECM (networking) with ACM (serial) is exactly what
breaks clean driver binding on Windows.

## UART serial console — a real, independent fallback

Doesn't depend on WiFi or USB gadget mode working at all — a real
login shell and real boot messages straight over three wires, useful
specifically *because* it's independent of the two things currently
not working.

**In `config.txt`**, add:
```
enable_uart=1
dtoverlay=disable-bt
```
The `disable-bt` line matters specifically on Zero-family boards: the
better, full-featured PL011 UART is shared with Bluetooth by default,
so without disabling it, console access may end up on the more
limited "Mini UART" instead, or the wrong device entirely — a real,
easy-to-miss trap, not an optional nicety.

**In `cmdline.txt`**: the serial console (`console=serial0,115200`) is
typically already present by default on a stock Raspberry Pi OS Lite
image — worth actually confirming it's there rather than assuming, but
likely nothing needs adding on top of it.

**Real wiring**: GPIO 14 (TXD, physical pin 8) and GPIO 15 (RXD,
physical pin 10) on the 40-pin header, to a real USB-to-TTL serial
adapter. **3.3V logic level, not 5V** — a real, common way to damage
the Pi if gotten wrong, not a hypothetical caution. Cross-connected:
adapter RX → Pi TXD, adapter TX → Pi RXD, plus a shared ground.

**Baud rate**: 115200.

**Once in**: a real shell over UART is exactly what's needed to
diagnose the WiFi and USB gadget problems from inside the device
itself, rather than guessing blind from outside it — real, common
next checks once there's a shell: the regulatory/country-code setting
for the WiFi radio (a real, frequent cause of a radio that simply
never transmits), which real config path applies on this specific
Debian 13/Trixie image (NetworkManager profiles vs. a legacy
`wpa_supplicant.conf`, since Trixie's default has moved toward
NetworkManager), and confirming the SSID/password were actually
written correctly for whichever mechanism is real for this image.

## Real, disclosed gaps — this is genuinely untested from this environment

- **Neither the USB gadget config nor the UART console setup above has
  been confirmed working on the real hardware yet** — the USB gadget
  attempt didn't produce readable results, and UART hasn't been tried
  yet at all. This document is real research and a real, reasoned next
  step, not a confirmed result.
- **Whether `rpi-usb-gadget` is actually installed/available on this
  project's specific image** hasn't been checked — worth confirming
  directly (`apt list --installed | grep rpi-usb-gadget` or similar)
  before assuming either the package path or the manual fallback is
  the right one to debug further.
- **Why WiFi didn't connect is still completely open** — no real
  diagnostic output (logs, `nmcli`/`wpa_cli` status, dmesg) has been
  reported yet, so this document can't yet narrow down whether it's a
  config-file mechanism mismatch, a credentials issue, a country-code/
  regulatory-domain problem, or something else. UART access is
  specifically what's needed to actually find out, not another guess.
- **No decision yet on whether to bake USB gadget mode into the
  device's real boot configuration by default**, versus documenting it
  as an optional, manually-applied step. Given it's purely additive
  and serves two real, stated needs, baking it in by default seems
  reasonable — but that's a real decision worth confirming, not
  assumed here.
