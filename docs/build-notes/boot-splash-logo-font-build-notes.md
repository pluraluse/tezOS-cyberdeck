---
date: 2026-10-15
project: tezos-cyberdeck
type: build-note
tags: [build, firmware, fonts, boot-splash, design]
---

# Build Note: Boot splash wordmark now uses GROUT Display

## Status: real, compiled, tested, verified on real hardware (the mirror/color fixes from the same session)

## Context

The boot splash (`docs/build-notes/boot-splash-build-notes.md`) drew its
"tezOS" wordmark in `TEZOS_FONT_HERO` — MonoMEK at 32px, the same font
used by the idle clock's hero display. The Owner sent three new font
deliveries from the same designer (Michael Alexander / MEK.txt) — MEK
Sans Regular, GROUT Display, and an updated MEK-Mono/MEK-Dings pair —
wanting a weightier face specifically for the opening logo.

## Action

### 1. Compared candidates before committing to one

Rendered "tezOS" at 72px in MEK Sans Regular, GROUT Display, and the
current MEK-Mono side by side rather than guessing from the font names.
MEK Sans Regular: bold pixel-sans, clearly weightier than MEK-Mono
already. GROUT Display: a heavy segmented/LCD-digital-readout style —
the boldest option, and a distinct "terminal readout" character that
fits this device's retro-terminal visual identity better than a
conventional sans. Owner picked GROUT Display.

### 2. Added a dedicated font role, not a `TEZOS_FONT_HERO` swap

`TEZOS_FONT_HERO` is also the idle clock's font — replacing it globally
would have changed a screen nobody asked to change. Added
`TEZOS_FONT_SPLASH_LOGO` instead:

- Rasterized GROUT Display at 32px with the existing
  `tools/rasterize_font.py` pipeline → `src/core/fonts/tezos_grout_display_32.h`
  (95 glyphs, 38KB of bitmap data).
- `TEZOS_FONT_SPLASH_LOGO` defined in `tezos_fonts.c`/`.h`, same pattern
  every other font role already follows.
- `tezos_boot_splash.c`: every `TEZOS_FONT_HERO` reference (5 call
  sites — width measurement, centering, per-character draw, beam
  height) swapped to `TEZOS_FONT_SPLASH_LOGO`. Nothing else in the file
  changed; the sweep/gleam logic is font-agnostic.
- Also fixed stale doc comments in `tezos_fonts.h` while in there:
  `TEZOS_FONT_HEADER`/`TEZOS_FONT_HERO` were still labeled "Press Start
  2P," a leftover from before `tezos_fonts.c`'s own comment says that
  font was retired in favor of MonoMEK.

### 3. Verification

- Full link against the real `core`/`chain`/`apps` tree: clean, 0 errors.
- `tests/run_all_tests.sh`: **72 passed, 0 failed** — the font swap
  didn't regress anything elsewhere (nothing else references
  `TEZOS_FONT_SPLASH_LOGO` or depended on `TEZOS_FONT_HERO`'s old call
  sites in `tezos_boot_splash.c`).
- Visually inspected `tests/splash_t1600.ppm` (the fully-lit frame) —
  GROUT Display renders correctly: centered, correct letterforms, no
  advance-width glitches.

### 4. Attribution

Added a **Fonts** line to `README.md`'s License section — CC0, Michael
Alexander (MEK.txt), crediting MonoMEK + MEK-Dings + GROUT Display
together (tezOS's entire visual voice runs on these three), linking
`linktr.ee/michaelmicasso` and `objkt.com/@micasso/created`.

## Result

| Check | Result |
|---|---|
| GROUT Display rasterized | ✅ 95 glyphs, 38KB |
| `TEZOS_FONT_SPLASH_LOGO` added, isolated from `TEZOS_FONT_HERO` | ✅ idle clock unaffected |
| Full link vs. real source tree | ✅ 0 errors |
| Full test suite | ✅ 72 passed, 0 failed |
| Visual check (`splash_t1600.ppm`) | ✅ correct, legible |
| README attribution | ✅ CC0, designer, both links |
| Confirmed on real hardware | Not yet this specific change — the mirror/color fixes earlier in this session were; this font swap hasn't had its own dedicated real-hardware boot-splash viewing yet |

## Follow-ups (not done yet)

1. Watch the actual sweep animation on real hardware with GROUT Display
   specifically — the letter-by-letter brightening effect was designed
   and verified against MonoMEK's solid-sans letterforms; a segmented
   display font's glyphs have more "gaps" within each character, which
   may read differently under the gleam than a solid sans does. Worth a
   real look, not just the static `.ppm` frames.
2. MEK Sans Regular wasn't used for anything — sent as a candidate,
   not picked. No action needed, just noting it's available in
   `/home/qwen/.claude/uploads/.../2f8ac9e1-MEKSans-Regular.otf.ttf` if a
   future role wants it (that path is ephemeral — would need re-sourcing
   from the designer if revisited later, same as every other font asset
   in this project, since no `.ttf`/`.otf` source files are committed to
   the repo, only the generated bitmap headers).
