# tezOS Core Architecture

This documents `src/core/` — the actual window manager: how screens, apps,
input, and memory fit together. Code and this doc should stay in sync;
if you change one, update the other.

## Proven working, not just designed

`src/core/demo_idle_screen.c` builds the real idle screen through the
real render API, real rasterized fonts, the shared list widget, and real
`tezos_core` navigation — verified end to end, not just compiled:
- The list widget's Up/Down scrolling and highlight are correct (checked
  pixel-for-pixel against expected output).
- Pressing SELECT genuinely pushes a screen (`core.depth` goes 1→2), not
  a no-op — this closed a real gap (SELECT previously did nothing).
- Pressing BACK genuinely pops back to idle (`core.depth` goes 2→1), and
  the idle screen's selection state is correctly preserved across the
  round trip (verified by diffing rendered frames: the post-pop frame is
  pixel-identical to the pre-push frame, and correctly *different* from
  the very first frame before any navigation happened).

This isn't a spec waiting to be implemented; the core loop, dirty-rect
tracking, text rendering, list scrolling, and push/pop navigation all
work today, in software, before any real DRM hardware is involved.

To rebuild and verify yourself:
```
cd src/core
gcc -Wall -Wextra -std=c11 -o demo_idle_screen demo_idle_screen.c tezos_core.c tezos_gfx.c tezos_fonts.c tezos_widgets.c -I.
./demo_idle_screen
# produces 01_idle.ppm, 02_idle_scanner_selected.ppm, 03_scanner_stub_pushed.ppm,
# 04_popped_back_to_idle.ppm — convert to PNG to view:
python3 -c "from PIL import Image; [Image.open(f'{n}.ppm').save(f'{n}.png') for n in ['01_idle','02_idle_scanner_selected','03_scanner_stub_pushed','04_popped_back_to_idle']]"
```

## The memory model (decided up front, not discovered as a bug)

**No malloc/free in the input-dispatch or render hot path.** Every screen
is a static singleton owned by its app — a fixed struct, not heap
allocated per navigation. The screen stack (`tezos_core_t.stack`) holds
**pointers** to these statics, never copies or heap instances.

This matters specifically because the device may run for weeks between
reboots. A heap-churning navigation model (allocate a screen on push,
free on pop) fragments slowly and unpredictably over that timescale —
exactly the kind of bug invisible in a short test session that shows up
as a field failure months later. A fixed stack depth
(`TEZOS_MAX_STACK_DEPTH`, currently 8) means "can't navigate deeper" is a
hard, safe, loggable failure — not silent memory corruption.

## The screen/app/input model

- **`tezos_screen_t`** — a vtable-style struct: `render()`,
  `handle_input()`, optional `on_resume()`, plus an `app_state` pointer
  the app owns. Apps are expected to define their screens as static
  globals (see `demo_idle_screen.c`'s `g_idle_screen` /
  `g_idle_state` for the pattern).
- **`tezos_core_t`** — owns the fixed-size navigation stack, the app
  registry (for the idle screen's numbered app list), the framebuffer,
  and the current frame's dirty-rect set.
- **Input dispatch**: `tezos_core_tick()` is called once per loop
  iteration by whatever's driving the platform (evdev on real hardware,
  a test harness here). It hands the current input event, if any, to the
  top-of-stack screen's `handle_input()`, which returns one of:
  - `TEZOS_ACTION_NONE` — handled in place, stack unchanged
  - `TEZOS_ACTION_PUSH` — drill into a new screen (e.g. selecting a menu item)
  - `TEZOS_ACTION_POP` — back, via the right softkey convention
  - `TEZOS_ACTION_REPLACE` — swap the current screen without growing the
    stack (idle → app home, so "back" from an app's home screen correctly
    returns to idle rather than to itself)
- **Redraw signaling**: `handle_input()` takes a `bool *needs_redraw`
  out-param. This exists because not every input event changes the
  screen (and not every state change is a stack action) — a menu
  highlight moving via Up/Down is a real redraw need with
  `TEZOS_ACTION_NONE`. Core doesn't guess; the screen says so explicitly.
- **Dirty-rect tracking**: `tezos_dirty_t` caps at
  `TEZOS_MAX_DIRTY_RECTS` (8) tracked rectangles per frame; exceeding that
  falls back to a full-frame redraw rather than tracking dozens of tiny
  rects. This exists because of the SPI panel bandwidth constraint (see
  `docs/build-notes/hardware-roadmap-build-notes.md`) — full-frame
  redraws on every change aren't viable, but tracking unlimited dirty
  rects isn't either.

## The render API (`tezos_gfx.h`/`.c`)

Operates on a plain RGB565 pixel buffer (`tezos_fb_t`). On real hardware
this buffer is a DRM dumb buffer mapped via libdrm; in the dev/test
harness it's a malloc'd block dumped to a PPM for visual verification.
**Same API either way** — this is what makes the SDL/software dev loop
described in `ROADMAP.md` actually work as a real development
environment rather than a diverged parallel implementation.

Primitives: `tezos_draw_rect`, `tezos_draw_rect_outline`,
`tezos_draw_text` (blits pre-rasterized glyph bitmaps with alpha-coverage
blending against whatever's currently underneath — not just against a
fixed background, since popups/highlights draw text over non-background
fills too), `tezos_text_width` (for centering/right-alignment).

Palette colors (`TEZOS_BG`, `TEZOS_PRIMARY`, etc.) are defined once in
`tezos_gfx.c` as the canonical values — see
`docs/design-system/PALETTE.md` for the design rationale, this file for
the actual `tezos_color_t` constants apps should reference rather than
hardcoding hex.

## Shared widgets (`tezos_widgets.h`/`.c`)

The layer between raw `tezos_gfx` primitives and full app screens —
built specifically so each app doesn't re-solve the same problems:

- **`tezos_list_state_t` / `tezos_list_handle_input` / `tezos_list_render`**
  — a reusable scrolling/highlighting list. Handles Up/Down, keeps the
  selection in view via automatic scroll-offset adjustment, renders
  selected-vs-normal rows with the established highlight convention
  (`TEZOS_PRIMARY` background, `TEZOS_BG` text). Any app with a list —
  Wallet's transaction history, Explorer's Graveyard, Scanner's
  entrypoint list — builds on this rather than hand-rolling row logic.
  Does **not** handle softkeys/select itself — what "select" does
  (push a detail screen, trigger a send, etc.) is screen-specific, so
  that stays the screen's own `handle_input`.
- **Generic stub screen** (`tezos_stub_screen_get`) — a placeholder
  screen any not-yet-built app can push to, showing its name and "not
  built yet." This is what let PUSH/POP get proven end-to-end before any
  app's real screen exists — replace a call to
  `tezos_stub_screen_get("Wallet")` with a real entry-screen constructor
  as each app actually gets built.

## Shared fonts (`tezos_fonts.h`/`.c`)

Font-role constants (`TEZOS_FONT_HEADER`, `TEZOS_FONT_HERO`,
`TEZOS_FONT_BODY`, `TEZOS_FONT_SMALL`) are defined **once** here and
included by anything that draws text, rather than each screen file
redefining its own local copies (which is what the first version of
`demo_idle_screen.c` did — fixed when the widgets layer was added).

## Fonts (`src/core/fonts/`)

Four `tezOS default` bitmap font assets, generated by
`tools/rasterize_font.py` from the real MonoMEK and MEK-Dings font files
at the fixed pixel sizes established from the design mockups:

| Asset | Source | Size | Role |
|---|---|---|---|
| `tezos_mekmono_16` | MonoMEK (CC0, Michael Alexander/MEK.txt) | 16px | App headers, logo, status bar, softkeys, small labels |
| `tezos_mekmono_32` | MonoMEK (CC0, Michael Alexander/MEK.txt) | 32px | Hero display (idle clock) |
| `tezos_mekmono_20` | MonoMEK (CC0, Michael Alexander/MEK.txt) | 20px | Body text, menu items (primary reading size) |
| `tezos_mekdings_24` | MEK-Dings (Michael Alexander/MEK.txt) | 24px | Icon set — see `docs/design-system/ICONS.md` for the codepoint mapping (not yet assigned) |

Press Start 2P has been fully retired — all text roles use MonoMEK now
that it's confirmed CC0. MEKmode remains a candidate to take over
Header/Hero specifically if obtained later.

Each is **8-bit alpha-coverage**, not 1bpp — this lets `tezos_draw_text`
blend against any foreground color from the palette at draw time, rather
than baking one fixed color into the font asset itself. Covers printable
ASCII 32–126 only for now; special-character/extended-glyph support for
T9 punctuation entry is a known follow-up (see PALETTE.md's open items).

**Re-running the rasterizer** (e.g. to swap Header/Hero to MEKmode once
obtained):
```
python3 tools/rasterize_font.py <ttf_path> <pixel_size> <c_identifier> <out_header_path>
```

## What's still open (per ROADMAP.md's M5 checklist)

Closed by this work: bitmap font conversion, drawing-primitives API,
input-dispatch/state-machine design, redraw strategy, shared list/menu
widget, generic app-stub screen, real end-to-end PUSH/POP navigation,
real body/small fonts (MonoMEK, confirmed CC0), `draw_icon` primitive +
real icon bitmap asset (MEK-Dings).

Still open: the actual icon-to-app codepoint mapping (a design decision,
not a technical one — see `docs/design-system/ICONS.md`), MEKmode as a
possible future swap for Header/Hero specifically (MonoMEK covers all
roles fine for now, this is optional polish not a gap), T9 text-entry
widget (needed by Settings, Messenger, Scanner, Discover — doesn't exist
yet), popups/system messages/alerts
(layout and transitions, not just color), and real DRM/evdev platform
shim code (this is all software-framebuffer-tested today, not yet wired
to actual Pi Zero 2 W hardware — that's M3/M4 in ROADMAP.md). Every app's
actual screen (Wallet, Explorer, Scanner, Gallery, Discover, Messenger,
Camera, Art, Settings) still needs to be written — only Idle exists as
real code; the rest currently push the generic stub screen.
