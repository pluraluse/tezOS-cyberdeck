# Icon mapping — MEK-Dings

`src/core/fonts/tezos_mekdings_24.h` is a rasterized 24px bitmap of every
printable-ASCII-mapped glyph in MEK-Dings (Michael Alexander / MEK.txt).
It's a dingbat font — each codepoint is a small pictogram, not a letter —
which makes it a real icon source rather than something to read as text.

See `mekdings-specimen-sheet.png` in this directory: a 10x10 grid of every
glyph (codepoints 32–126), each labeled with its codepoint number
underneath. Use that to pick which glyph reads best as which app's icon.

**This mapping is intentionally left for a human (or the artist) to
decide** — matching an abstract pictogram to "this means Wallet" is a
design judgment call, not something to guess at from a codepoint alone,
especially working with someone else's art. Fill in the table below once
you've picked:

| App | Codepoint | Notes |
|---|---|---|
| Wallet | | |
| Explorer | | |
| Scanner | | |
| Gallery | | |
| Discover | | |
| Messenger | | |
| Camera | | |
| Art (1-bit) | | |
| Settings | | |

Once filled in, add named constants to `tezos_fonts.h` (e.g.
`#define TEZOS_ICON_WALLET '...'`) so app code references a name, not a
raw codepoint — same pattern as `TEZOS_FONT_BODY` etc.

## Using an icon once mapped

```c
tezos_draw_icon(fb, dirty, x, y, &TEZOS_ICON_FONT, TEZOS_ICON_WALLET, TEZOS_PRIMARY);
```

`tezos_draw_icon` (in `tezos_gfx.h`/`.c`) reuses the same glyph-blit
machinery as text rendering — an icon font is just a font asset where
each glyph happens to be a pictogram, so no separate rendering path was
needed.
