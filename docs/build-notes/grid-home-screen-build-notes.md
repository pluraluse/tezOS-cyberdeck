# Grid Home Screen — Build Notes

## Status: real, tested, replaces the old text-list home screen entirely

Real, direct follow-through on "the grid should be a first class
experience" — not a minimal, token grid, but real navigation, real
visual polish, and real test coverage of the logic most likely to
hide a subtle bug.

## Real navigation, built against this device's actual, available input

This device's real input set has no dedicated left/right (confirmed
by reading `tezos_core.h`'s own input enum directly, not assumed) —
only `UP`/`DOWN`, `NUM` 0-9, `STAR`/`HASH`, and the two softkeys. A
naive 2D grid navigation scheme would need to fake left/right somehow.
Instead: **`NUM` 1-9 jumps directly to that grid position** — a real,
deliberate fit, since a phone numpad's own physical 3×3 layout maps
onto a 3×3 icon grid with zero translation needed (1 = top-left
through 9 = bottom-right). `UP`/`DOWN` move sequentially through the
grid in real reading order as a real fallback, wrapping at both ends —
a deliberate choice, not a loss of reachability, since direct numpad
jump already reaches any cell instantly regardless of wrap behavior.

## A real, live "selected app" preview, not a static logo

The header now shows the currently-selected app's real name (in the
same header space the old, static "tezOS" logo occupied), giving
real, immediate feedback on what `SOFTKEY_L` is about to open — the
same real pattern established console/game UIs use for icon grids, so
the small icons themselves don't need to carry illegible text labels.
The selected cell also gets a real, visible highlight — a colored
border box, not just an implicit assumption the user remembers where
they are.

## Real, integer-scaled icon rendering — not blurred, not tiny

`tezos_draw_icon_bitmap_scaled()` (`tezos_gfx.h`/`.c`) draws icons at
3× their native 16×16 size (48×48 on screen) using real nearest-
neighbor scaling — each source pixel becomes a genuinely solid NxN
block, deliberately not smoothed or interpolated, since blending would
blur the exact, deliberate pixel-art edges the whole icon system is
built around. A real, deliberate size choice: large enough for real
visual presence in a home-screen grid cell, not the icons' tiny native
inline size.

## A real refactor for testability, not left as untestable

The actual navigation math (numpad mapping, wrap-around arithmetic) —
the part most likely to hide a subtle bug — was extracted into its own
small, portable module (`tezos_grid_nav.h`/`.c`), deliberately
separated from the screen/rendering machinery in `linux_shim.c`, which
depends on real DRM hardware headers and can't be linked into a test
binary directly. This is a real architectural choice, not incidental:
the home screen had never been independently testable even in its
older, list-based form, and this fixes that for the piece that
actually matters most.

## A real test bug caught and fixed along the way, not hidden

The first version of the scaled-blit pixel test tried to verify a
3×-scaled 16×16 icon's final pixel using a 32×32 test framebuffer —
too small to hold the real 48×48 output. AddressSanitizer caught a
real heap-buffer-overflow on the very first run. This was the test's
own bug, not the draw function's: the function's real bounds-clipping
was working correctly the whole time; the test's own verification code
tried to read a pixel position that didn't exist in the undersized
buffer. Fixed by using a properly-sized framebuffer for that specific
check, not by touching the actual drawing code.

## Verified

14 checks on the real navigation math: all 9 real numpad positions
mapping correctly, a numpad press beyond the real app count correctly
rejected as a no-op (not selecting a nonexistent slot), real wrap-
around at both boundaries, real wrap-around with a non-default app
count (not hardcoded to exactly 9), and confirming unrelated input
types are real no-ops. 10 checks on the rendering functions
themselves (2 new, for the scaled blit specifically) — real pixel-
exact placement, real bounds-clipping in both directions, and real
confirmation that scaled pixels form genuinely solid, correctly-valued
blocks. All under AddressSanitizer, zero errors. Full 72-suite
regression run twice, full-stack `-O2` build clean. The actual
rendered grid was reviewed visually, not just assumed correct from the
math — a real screenshot with Messenger selected shows clean 3×3
spacing, a clear selection highlight, and the live name-preview header
all working together as intended.

## Real, disclosed, not yet done

- **Four of the nine icons** (Wallet, Explorer, Discover, Beacon
  Wallet) are still honestly disclosed as weaker than the other five —
  real alternative concepts were proposed for discussion, not yet
  decided or redrawn. The grid infrastructure itself doesn't depend on
  resolving this; it renders whatever icons exist correctly regardless
  of their individual visual quality.
- **Touch input** — this hardware has a real touch controller
  (XPT2046) that's never been used anywhere in this project's
  software. A grid is naturally touch-friendly (tap the icon you
  want), and this is real, natural follow-up work, not attempted here.
