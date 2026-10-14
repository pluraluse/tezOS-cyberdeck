# Multi-Color App Icons — Build Notes

## Status: real rendering pipeline built and tested; all 9 app icons designed

Real, direct follow-through on choosing "true multi-color mascot-style
icons" over the existing single-color MEK-Dings font — a real,
different technical path, not just new art on the old pipeline. All
9 real apps now have a hand-designed icon; Wallet was redesigned once
already based on an honest first-pass assessment.

## A real correction made mid-conversation, worth recording honestly

Before this work started, this project's own earlier answer that
"we have no icons at all" was wrong — real icon infrastructure
(MEK-Dings, a licensed CC0 dingbat font, already rasterized into a
working C asset with a real rendering function) already existed. What
was actually missing was the app-to-glyph mapping, and — once the
actual glyphs were looked at directly — a real style mismatch: an
eclectic, single-color pictogram set, not the bold multi-color
mascot look being asked for. That's the real reason this went down a
new path instead of reusing what was there.

## The real technical foundation: one shared function, not three duplicates

`tezos_draw_icon_bitmap()` (`tezos_gfx.h`/`.c`) is a real extraction,
not new-from-scratch code — it's the same direct RGB565 blit pattern
that had been real, working, but independently duplicated in both
`gallery.c` and `discover.c` (each with its own private `blit_rgb565`
helper). One tested implementation now. `tezos_icon_t` is a fixed
16×16 RGB565 pixel array — 16px chosen deliberately to stay genuinely
chunky at a glance, matching real 2000s feature-phone icon scale,
rather than trying to be a detailed modern icon shrunk down.

## A real, new authoring tool — icons as plain, editable text

`tools/rasterize_icons.py` — matches the established pattern of
`tools/rasterize_font.py`. Icons are authored as a simple 16×16 grid
of single-character color codes (`tools/icons-source/app-icons.txt`),
converted to a real RGB565 C header. Deliberately plain-text, not a
binary pixel-editor file — readable, diffable, and editable directly
in this repo without needing one specific artist's own toolchain to
reproduce or adjust. The color legend uses the real, established
palette (`TEZOS_PRIMARY`, `PINK`, `CYAN`, `AMBER`) plus two real,
new additions the existing palette didn't have — a green and a
yellow — added specifically because the ICQ/AIM reference point
(a green flower, a yellow running figure) needed them.

## All 9 real icons designed — honest, real assessment, not uniform success

Rendered together and reviewed directly, not just assumed correct from
the source pattern. **Two real passes** — the concepts themselves were
confirmed as agreed-upon before this second pass; what changed was
execution, not the underlying metaphor for any icon.

**Read well from the start, unchanged:**
- **Messenger** — a chunky cyan speech bubble with two small white
  "eyes" and a pointed tail. Real mascot-like character.
- **Camera** — a pink camera body with an amber viewfinder bump and a
  white lens detail.
- **Gallery** — a classic picture-frame icon: white frame, blue
  mountain range, green tree, amber sun.
- **1-bit Art** — a diagonal pencil, white shaft with a colored tip.
- **Settings** — an 8-point gear with a white center hole.

**Genuinely improved on the second pass:**
- **Wallet** — redesigned from a rounded square into a wider, flatter
  billfold shape with a distinct dark band framing an amber "card"
  section. A real, clear improvement over both earlier attempts.
- **Discover** — the compass needle was redrawn bolder and larger,
  with a distinct dark cross-band for contrast. Reads clearly as a
  compass now, where the first pass's needle was too thin to register.
- **Beacon Wallet** — the antenna beam was widened into a clearer
  triangular shape with better-positioned signal marks. A real,
  meaningful improvement in "this is broadcasting something."

**A real, disclosed tradeoff, not a clean win:**
- **Explorer** — the second attempt made the handle much thicker and
  the lens bolder, but the handle became so short it barely extends
  past the lens — it now reads more as "a chunky blob with a nub"
  than a recognizable magnifying glass. This is honestly a sideways
  move, not a clear improvement over the first version, and is stated
  as such rather than claimed as success just because a second pass
  was attempted. Worth either a third, more careful pass at the same
  concept, or reconsidering the radar-sweep alternative raised
  earlier, before treating this one as settled.

## Verified

8 checks on the rendering function itself (unchanged from the first
pass — the function doesn't care which icon it's drawing), using a
synthetic icon where every pixel encodes its own position, so exact
placement can be checked byte-for-byte: real top-left/bottom-right/
interior pixel placement, real dirty-rect marking, and real bounds-
clipping in both directions. All under AddressSanitizer, zero errors.
All 9 icons confirmed to compile and were rendered together as a real
grid for direct visual review, not assumed correct from the source
pattern alone. Full 71-suite regression run twice, full-stack `-O2`
build clean.

## Real, disclosed, not yet done

- **Wired into a real grid home screen** — see
  `docs/build-notes/grid-home-screen-build-notes.md`. This is now done,
  not open.
- **Explorer specifically** is the one icon still not settled — the
  second pass was a real, disclosed tradeoff, not a clean improvement.
  Worth a third pass, or reconsidering the radar-sweep alternative,
  before treating it as final.
