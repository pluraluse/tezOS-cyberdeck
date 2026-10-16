---
date: 2026-10-15
project: tezos-cyberdeck
type: build-note
tags: [build, firmware, shim, display, fbdev]
---

# Build Note: `linux_shim.c` rewritten against fbdev, not DRM

## Status: real, compiles and links clean against the full production source tree — not yet run on real hardware

## Context

`linux_shim.c` was originally written against libdrm (DRM/KMS), on the
assumption that the `piscreen` overlay would bring up a modern
tinydrm-class DRM device (`/dev/dri/card0`). Real hardware bring-up
(`docs/build-notes/display-bringup-build-notes.md`) found that assumption
wrong on two counts: `piscreen` never worked at all, and the overlay that
*does* work on this board (`dtoverlay=fbtft,ili9481,...`) uses the Linux
kernel's legacy `fbtft` staging driver, confirmed via `dmesg` across every
trial this session — which creates a plain fbdev device (`/dev/fb1`).
There is no `/dev/dri/*` node anywhere in the working configuration. The
shim as written could never have worked against this hardware, regardless
of which panel overlay got picked.

## Action

### 1. Swapped the display backend, left everything above it alone

Replaced `drm_dev_t`/`drm_init()`/`drm_flip()`/`drm_cleanup()` with
`fbdev_dev_t`/`fbdev_init()`/`fbdev_flip()`/`fbdev_cleanup()`. Everything
above that layer — the `physical_fb`/`logical_fb` split, the
letterboxing path for mismatched test displays, the dirty-rect
bounding-box reduction, the boot splash hand-off logic in `main()`'s loop
— needed zero changes. It was already built against the panel-agnostic
`tezos_fb_t`, which is just `{uint16_t *pixels, int width, int height}`
with no DRM-specific assumptions baked in anywhere above the backend.

### 2. Two concrete decisions carried over directly from the bring-up session

- **`fbdev_flip()` uses `pwrite()`, not `mmap()`.** Every test pattern
  during bring-up was pushed to the panel with a plain `write()` to the
  fbdev character device and landed correctly. `mmap()` + the kernel's
  deferred-io framework (common for `fbtft`-class drivers) was never
  validated — timing between a raw memory write and when the driver
  actually pushes it over SPI is driver-specific and untested here. Not
  assumed; used the proven path instead.
- **R/B channel swap happens in `fbdev_flip()`, in software**, not via
  the devicetree `bgr` flag. Bring-up found the flag's effect
  inconsistent across reboots on an identical config (most likely a
  marginal breadboard connection — not confirmed, not chased further,
  per that build note's "design around it" decision). This makes color
  correctness a property of the shim, not of a flag or a wire.

### 3. Fail loud on a format mismatch, not silently wrong

`fbdev_init()` checks `vinfo.bits_per_pixel == 16` and refuses to start
otherwise, with a message pointing at the bring-up build note. This shim
only knows how to drive 16bpp RGB565 (the confirmed working format); a
different value means `config.txt` has drifted from the bring-up
configuration, which needs reconciling, not a best-effort pixel-format
conversion bolted on top.

### 4. Verification

- `gcc -O2 -Wall -Wextra -std=c11 -c linux_shim.c` — zero warnings, zero
  errors (one cosmetic `-Wcomment` warning found and fixed along the way:
  a literal `/dev/dri/*` inside a block comment).
- **Full link against the real production source tree** — found and used
  the actual current build command from `scripts/install-tezos-service.sh`
  (not a guessed file list — the project's own history already
  documents getting bitten by that exact drift twice), including
  `../chain/*.c` and every real app in `../apps/`, linking against
  libsodium/curl/cjson/qrencode/zbar. **Zero link errors.** This is the
  first time this file has linked clean against the full real app suite,
  not just compiled in isolation.
- **Ran the resulting binary** on this dev host (no real panel here):
  correctly initializes all 9 registered apps, reaches `fbdev_init()`,
  fails cleanly with `open(/dev/fb1) failed: No such file or directory`,
  exits status 1. Same designed, correct failure shape the project's own
  M3 note describes for the old DRM backend in a sandbox with no real
  `/dev/dri/*` — not a defect, the expected result off real hardware.
- **`scripts/install-tezos-service.sh` updated to match** — dropped the
  now-unused `libdrm-dev` apt package and `pkg-config --cflags --libs
  libdrm` link flag, changed its hardware pre-flight check from
  `/dev/dri/cardN` to `/dev/fb1`. Catching this now rather than letting
  it become a third instance of the exact build-command-drift bug this
  script's own comments already warn about twice.

## Result

| Check | Result |
|---|---|
| `linux_shim.c` compiles standalone | ✅ 0 warnings, 0 errors |
| Full link vs. real `core`/`chain`/`apps` source tree | ✅ 0 errors — first time ever for this file |
| Binary runs, fails correctly without real hardware | ✅ matches the designed "no device, fail clean" shape |
| Boot splash code itself | Unchanged — already verified via `tests/test_boot_splash.c`'s `.ppm` output; blocked only by the backend above it, which is now fixed |
| Run on actual Pi hardware | ❌ not done this session — needs the device back, with the confirmed-working `config.txt` from the display bring-up session |
| Touch input (XPT2046) | Still not wired into the panel overlay at all — unrelated to this change, carried over from the bring-up build note's open items |

## Real hardware test (2026-10-15, same session) — it worked

Synced the rewritten source to the Pi over the network for the first time
(WiFi + the SSH key installed this session — see
`docs/build-notes/display-bringup-build-notes.md`'s WiFi finding), built
natively on-device with the exact command above, and ran it against the
real panel.

**Boot splash rendered on real hardware for the first time this project
has ever had real hardware access.** Owner-confirmed: *"that was
incredible!!!!"*

One real bug found immediately: **everything was mirrored left-right**
(readable only in a mirror; vertical axis correct, selected-icon position
and all relative layout otherwise correct — confirmed from a photo:
"Wallet" and "MENU"/"SELECT" text reversed, but default selection
correctly on index 0). Same category as the `bgr` color-order issue from
the display bring-up session, and fixed the same way: in software, inside
`fbdev_flip()`, reading each device column from its mirrored logical
column (`fb->width - 1 - x0 - i`) rather than trusting any devicetree
property. Verified correct by indexing from the *full* row rather than
the dirty-rect-offset pointer, so it's correct for partial dirty-rect
writes too, not just full-frame ones.

**Then went further than the boot splash** — no keypad is wired yet
(M2), so wrote a small `/dev/uinput`-based synthetic keyboard
(`/tmp/uinput_keypad.c`, not yet committed into the repo — see
Follow-ups) registering exactly the keys `translate_key()` understands,
symlinked it to the hardcoded `/dev/input/by-id/tca8418-keypad-event-kbd`
path, and drove real navigation through a FIFO:

- Idle grid → **ENTER** → **Wallet app opened and displayed a real
  balance** (Owner-confirmed) — the first real app screen beyond the
  idle grid ever rendered on this hardware, exercising the mock signer,
  HTTP client, and TZKT read path together, not just the render pipeline.
- **ESC** → correctly returned to the idle grid.
- **NUM 9** (jumps directly to grid position 9) → **ENTER** → **Settings
  app opened**, a dense text list (Device Name, Profiles, Lock/Passcode,
  Wi-Fi, Storage, Services, About, Import Recovery Phrase, RPC Node with
  a real URL, Name Registry, Network) — Owner-confirmed rendering
  correctly, right orientation, right colors. A meaningfully harder
  render test than the icon grid (far more individual glyphs), and it
  held up.

**Stability, same run, no restart between any of the above**: 11+ minutes
uptime, 0.0–0.2% CPU at idle (confirms the 50ms poll loop isn't
busy-spinning), ~9–12MB RSS, no crashes, no new errors beyond the one
expected and already-known "no touch device" warning.

## Follow-ups (not done yet)

1. `fbdev_init()` hardcodes `/dev/fb1`. Stable in every trial this
   session (fb0 is always the console framebuffer, fb1 is always the
   panel), but a `/sys/class/graphics/fb*/name` scan for `fb_ili9481`
   specifically would be more robust against future drift — not done,
   not blocking.
2. Real keypad/touch hardware (TCA8418/XPT2046) still isn't wired —
   tonight's navigation test used a synthetic `/dev/uinput` keyboard,
   not real input hardware. That's still M2's real work.
3. The synthetic-input test script (`/tmp/uinput_keypad.c` on both this
   host and the Pi) isn't committed anywhere — it's a genuinely useful
   dev tool (lets anyone exercise real navigation before the keypad
   overlay exists) and should move into `tools/` rather than stay a
   throwaway `/tmp` file, same lesson as losing `/tmp/orient.py` twice
   during display bring-up.
4. Longer soak test (this was ~11 minutes, not the 30-minute target
   ROADMAP's M7 eventually wants) and a check of whether RSS growth
   continues past ~12MB or plateaus — too short a window here to tell
   trend from noise.
