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

**Update: MonoMEK and MEK-Dings are in use.** MonoMEK (confirmed CC0,
licensed directly by creator Michael Alexander / MEK.txt) has replaced
VT323 as the real body/small font — rasterized at 16px and 20px in
`src/core/fonts/tezos_mekmono_16.h` / `_20.h`, wired into
`TEZOS_FONT_SMALL` / `TEZOS_FONT_BODY`, and verified rendering correctly
through the real pipeline. MEK-Dings turned out to be a dingbat/icon font
(pictogram per codepoint, not letters) — rasterized at 24px as the real
icon source (`tezos_mekdings_24.h`, `TEZOS_ICON_FONT`); see `ICONS.md` for
the specimen sheet and mapping task. MEKmode is still a candidate for the
display/header role (currently Press Start 2P) — "workaround in
progress" per project owner, not yet obtained.

Two-font system, since a single font can't cover both "vintage vibe" and
"readable at small sizes":
- **Display/headers**: a chunky 8-bit pixel font (currently Press Start
  2P; MEKmode is the candidate replacement, pending a workaround) — used
  sparingly, for the idle screen's clock/branding and section headers
  only. Illegible at small sizes, so never use it for body text.
- **Body/menu/lists**: **MonoMEK**, confirmed and in use — for menu
  items, numbers, and anything a user needs to actually read quickly.

## Open items

- Confirm contrast empirically against the actual ILI9486 panel once
  hardware bring-up reaches display work — SPI TFT color reproduction and
  backlight behavior vary panel to panel.
- Popups/alerts, icons, and the menu/softkey chrome treatment still need
  to be designed against this palette — this doc covers color and type
  only, not layout.

See `idle-screen-mockup.html` in this directory for a rendered example
using this palette.
