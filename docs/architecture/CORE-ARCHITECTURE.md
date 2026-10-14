# tezOS Core Architecture

This documents `src/core/` — the actual window manager: how screens, apps,
input, and memory fit together. Code and this doc should stay in sync;
if you change one, update the other.

## The logical canvas — how a Pi + HDMI + 1080p monitor shows the real UI correctly

The render/state-machine layer was always described as "form-factor-
agnostic" because it works off `fb->width`/`fb->height` rather than
hardcoded values. That claim needed a real test, not just a description
— what actually happens when the *attached display* is a completely
different shape than the target panel, not just a different size? A
1920×1080 landscape monitor and a 320×480 portrait panel aren't
proportional versions of each other; naively stretching the UI to fill
whatever resolution DRM reports would look wrong and wouldn't represent
what the real device looks like.

**The fix**: `tezos_core` always renders into a fixed **logical canvas**
(320×480, the real target panel's resolution) regardless of what
physical display is actually attached. `src/shim-linux/linux_shim.c`
detects whether the physical DRM mode matches that logical size:
- **Matches exactly** (the real target panel) — the existing
  dirty-rect SPI-bandwidth-optimized fast path runs completely
  unchanged. This new layer costs nothing on the actual device.
- **Doesn't match** (e.g. dev/testing via HDMI to a 1080p monitor) —
  `tezos_fb_blit_letterboxed()` (new, `tezos_gfx.c`) scales the logical
  canvas by the largest integer factor that fits, centers it, and fills
  the remainder with the background color — a real letterboxed/
  pillarboxed image, not a distorted stretch. Full-frame blit every
  redraw, not dirty-rect partial updates — correct trade for a path
  where bandwidth is a non-issue (HDMI) and correctness/undistorted
  proportions are what actually matters for testing.

**A real bug got caught building this**: the initial version divided
`(x - offset) / scale` before checking whether the result was in
bounds. C's integer division truncates toward zero, not floors, so for
a pixel just outside the scaled region (e.g. one row above it), the
negative numerator produced `0` instead of the expected negative value
— incorrectly treating that boundary row as inside the content instead
of a letterbox bar. Caught by an explicit boundary-pixel test, not
found by inspection. Fixed by checking the relative position's sign
*before* dividing.

**Verified**: a same-resolution blit (the real-panel case) is pixel-
identical to its source — this new layer provably doesn't affect the
actual target hardware's behavior. The 320×480→1920×1080 case was
checked pixel-by-pixel (center shows content, corners show the bar
color, the exact computed boundary is correct) and confirmed visually
by rendering a real screen through the full pipeline.

**On the "are screens actually coordinate-agnostic" question** — this
was flagged as an open concern earlier in the project (absolute pixel
positions in `wallet.c` rather than values derived from `fb->height`).
Checked with real evidence this pass, not left as an assumption: all
four real screens (Wallet, Settings, Camera, the recovery-phrase import
flow) were rendered at the actual target size (320×480), a deliberately
shortened stress-test size (320×360), and a taller one (320×600).
**Result: no overlap, no clipping, anywhere.** The footer/softkey bar
pattern (`fb->height - 34`) and list-widget available-height calculation
(`fb->height - N`) are already used consistently across all four files
— the earlier concern was real to flag but turned out more pessimistic
than the actual code warranted. Top-anchored absolute Y-positions for
stacked text content (`y=44`, `y=62`, ...) remain, but that's a normal,
defensible layout pattern for fixed-line-height text, not the "assumes
exactly 480px" anti-pattern originally worried about — text lines have
fixed pixel heights regardless of screen size, so anchoring them to a
percentage of screen height would be the wrong design, not a fix.

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

## Popups/confirmation dialogs (`tezos_popup.h`/`.c`)

The most repeatedly-flagged open gap, now closed for the common case: a
Yes/No confirmation (e.g. "Send 1.5 tez to tz1...? This cannot be
undone.").

**No core changes needed** — a popup is just a regular pushed
`tezos_screen_t`. What makes it read as an overlay rather than a full
screen: its `render()` deliberately doesn't clear the framebuffer first,
unlike every other screen. Since nothing auto-clears the framebuffer
between renders, whatever the previous screen last drew stays visible
around the edges — the popup only draws its own centered box on top. No
alpha blending, no new primitive: just restraint about what not to draw.

**Results flow back through the existing `on_resume` callback** — it
already existed for exactly this "resumed after a child screen closed"
case (e.g. refreshing a balance after Send). The caller supplies a
pointer to its own `tezos_popup_result_t`; the popup writes
`TEZOS_POPUP_CONFIRMED`/`CANCELLED` into it right before popping, and the
caller's `on_resume()` checks it.

**Wired into a real flow, not just built and left idle**: Wallet's Send
screen previously signed immediately on one button press — pressing SIGN
now pushes a real confirmation popup first, and `perform_send()` only
runs if the popup reports `CONFIRMED`. Verified: pressing SIGN pushes the
popup (doesn't sign), cancelling pops back with nothing signed, and
confirming triggers the real sign — checked both structurally (the
screen-stack depth transitions) and by diffing the actual rendered output
between the cancelled and confirmed states (a real, non-empty difference
in exactly the result-display region, not a no-op).

**Scope**: covers the confirm/cancel case, which is what most of
Wallet's real flows need. A general "alert with just an OK button" or a
richer system-message style would layer on the same pattern (a
non-clearing pushed screen) if/when needed — not built separately since
the confirm/cancel version already demonstrates the mechanism.

**A real stack-use-after-return bug, latent across multiple callers,
found and fixed at the source.** `tezos_popup_confirm_get()`'s original
implementation stored the caller's `title`/`message`/`confirm_label`/
`cancel_label` pointers directly — never copied. Pushing a screen and
actually rendering it happen in separate steps of `tezos_core_tick`'s
dispatch, so any caller building `message` in a stack-local buffer
(exactly what a real confirmation message needs — it has to include the
actual amount, address, or entrypoint name) had that buffer go out of
scope before the popup ever read it. Caught by AddressSanitizer while
testing Explorer's new entrypoint-calling flow — but a direct check
afterward confirmed `messenger.c` and `scanner.c` build their own
confirmation messages the exact same way, meaning they carried the same
latent bug independently, whether or not ASan happened to catch it
there too (stack-use-after-return detection isn't fully deterministic).
Fixed once, at the source, rather than patched at each call site:
`popup_state_t` now owns fixed-size buffers and copies all four strings
via `safe_copy()` (a helper already in this file for an earlier,
unrelated reason). Sizes were set from the real callers' actual usage,
not guessed — `messenger.c`'s 160-byte message buffer was the largest
real one found. Reverified clean under AddressSanitizer afterward:
Explorer's own new test (the one that caught this), plus Messenger's
and Scanner's existing confirm-flow tests, all rerun and passing with
zero ASan errors — not just "compiles now."

## T9 text entry (`tezos_t9.h`/`.c`)

Multi-tap text entry, closing the gap that blocked Settings, Messenger,
Scanner, and Discover from having any real (non-hardcoded) text input.

**Three ways a cycling letter commits**, in the order they came up:
1. Pressing a *different* digit — commits the previous letter, starts
   cycling the new key.
2. Pressing Up — explicit "confirm now" fast-path. Also what makes
   typing two letters from the *same* key back to back (the double L in
   "hello") possible instantly, without waiting.
3. A short pause with no key pressed (`tezos_t9_tick`, `TEZOS_T9_COMMIT_DELAY_MS`
   = 800ms) — matches real phones, so typing doesn't *require* knowing
   about Up; it's a fast-path, not the only path.

(3) needed a small, genuine core-level addition, since `tezos_core`'s
dispatch previously only ran on real input events — there was no "time
passed with nothing pressed" event at all. Rather than bolt a timer onto
the input-event system, `tezos_screen_t` gained an **optional `on_tick`
callback** (nullable, matches the existing `on_resume` pattern) and a
new `tezos_core_on_tick(core, elapsed_ms)` entry point that calls it on
the active screen and renders if it requests a redraw. Any screen can
use this for anything time-based later — it's not T9-specific, T9 is
just the first thing that needed it. Screens that don't set `on_tick`
are completely unaffected (auto-`NULL` via designated initializers,
already the case for every existing screen).

`src/shim-linux/linux_shim.c`'s main loop now polls with a 50ms timeout
(previously blocked forever) specifically so this fires on real
hardware, tracking real elapsed time via `clock_gettime(CLOCK_MONOTONIC)`.

**A real bug got caught and fixed while wiring this up, not
worked around**: `tezos_core_tick()` clears `core->needs_render`
internally right after rendering, but the shim's loop was checking that
same flag *after* the call returned — meaning it was always already
false by then. In the committed code before this fix, the shim would
correctly render into the internal software framebuffer but never
actually flip it to the real display after the very first frame. Fixed
by having `tezos_core_tick()` and the new `tezos_core_on_tick()` both
*return* whether they rendered, and having the shim use that return
value instead of the stale flag.

Verified with three layers of tests: widget-level logic (multi-tap
spelling, case toggle, backspace-cancels-pending-vs-deletes-committed,
digit-cycle wraparound, DOWN correctly left unclaimed), delay-mechanism
logic (no premature commit, same-key retap correctly resets the timer,
"hello" typed using *only* delay-based commits with zero Up presses,
Up still works unaffected, tick-with-nothing-pending is a safe no-op),
and a full core-to-widget integration test using a real screen with
`on_tick` wired through the actual `tezos_core_on_tick` API (not the
widget called directly) — confirmed both the no-premature-render
behavior and the correct render firing exactly at the 800ms threshold,
checked against the actual rendered pixel output.

**Not yet wired into any app screen** — this is the widget (and the
core timer mechanism it needed) only. Settings/Messenger/Scanner/
Discover still need actual screens built that use it.

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
roles fine for now, this is optional polish not a gap), popups/system
messages/alerts
(layout and transitions, not just color), and real DRM/evdev platform
shim code (this is all software-framebuffer-tested today, not yet wired
to actual Pi Zero 2 W hardware — that's M3/M4 in ROADMAP.md). Every app's
actual screen (Wallet, Explorer, Scanner, Gallery, Discover, Messenger,
Camera, Art, Settings) still needs to be written — only Idle exists as
real code; the rest currently push the generic stub screen.
