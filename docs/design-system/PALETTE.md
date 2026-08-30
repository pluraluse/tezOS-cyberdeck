# tezOS Palette & Typography — v1 proposal

Direction: Tezos brand blue as the anchor, vintage-80s/synthwave treatment,
readable on a small SPI TFT — not too dark to read outdoors, not blinding
at night.

## Color palette

| Role | Hex | Notes |
|---|---|---|
| Background | `#10143A` | Deep indigo-navy, not pure black. Close kin to Tezos's own "Lucky Point" brand navy (`#14266C`). Dark enough for contrast and CRT-terminal feel, light enough to not read as dead/flat. |
| Primary (Tezos Blue) | `#0F61FF` | The actual official Tezos brand blue (PMS 2174C). Headers, active nav state, primary actions. |
| Secondary accent (neon pink) | `#FF3D8A` | 80s synthwave accent. Alerts, selection highlights — used sparingly. |
| Tertiary accent (neon cyan) | `#2FE6E6` | Secondary highlights, confirmation/success states. |
| Warning/error (amber) | `#FFA53D` | Sunset-adjacent amber, fits the synthwave palette without breaking convention (still reads as "warning"). |
| Body text | `#E4EAFB` | Soft light blue-white — not pure white, avoids glare on a small backlit panel. |
| Muted/secondary text | `#8894C4` | Mid blue-gray, for labels and less prominent text. |

**Why not pure black / pure white**: pure black backgrounds on a cheap SPI
TFT tend to read as dead/flat rather than rich, and pure white text is
genuinely glare-inducing on a backlit panel used in low light. The chosen
background and text values keep real contrast (background is dark enough,
text is light enough) without hitting either extreme — check this
empirically once real hardware is in hand, since panel-specific gamma/
backlight behavior varies.

## Typography

**Update: all four text-font roles now use MonoMEK** (confirmed CC0,
Michael Alexander/MEK.txt) — Header, Hero, Body, and Small are all
MonoMEK at 16px/32px/20px/16px respectively. Press Start 2P has been
fully retired as a placeholder. MEK-Dings is the icon set (24px,
dingbat/pictogram font — see `ICONS.md`). MEKmode remains a candidate to
take over the Header/Hero roles specifically if obtained later — swap by
re-running `tools/rasterize_font.py` against it and updating
`src/core/tezos_fonts.c`'s two constants, same pattern already used
throughout that file.

One font, four sizes, since a single point size can't cover both
"vintage vibe at a glance" and "readable at small sizes" — larger sizes
(32px hero clock, 16px headers) read as more "display," smaller sizes
(20px body, 16px small labels) stay legible for dense content.

## Open items

- Confirm contrast empirically against the actual ILI9486 panel once
  hardware bring-up reaches display work — SPI TFT color reproduction and
  backlight behavior vary panel to panel.
- Popups/alerts, icons, and the menu/softkey chrome treatment still need
  to be designed against this palette — this doc covers color and type
  only, not layout.

See `idle-screen-mockup.html` in this directory for a rendered example
using this palette.
