#pragma once
/*
 * Canonical tezOS font-role constants — defined once here, used by
 * every screen/widget that draws text. Prevents each new screen file
 * from redefining FONT_HEADER/FONT_BODY/etc. locally.
 *
 * See docs/design-system/PALETTE.md for the size/role rationale, and
 * src/core/fonts/ for the actual generated glyph data.
 */
#include "tezos_gfx.h"

extern const tezos_font_t TEZOS_FONT_HEADER; /* Press Start 2P 16px — app headers, logo */
extern const tezos_font_t TEZOS_FONT_HERO;   /* Press Start 2P 32px — idle clock / hero display */
extern const tezos_font_t TEZOS_FONT_BODY;   /* MonoMEK 20px — body text, menu items (primary reading size) */
extern const tezos_font_t TEZOS_FONT_SMALL;  /* MonoMEK 16px — status bar, softkeys, small labels */
extern const tezos_icon_font_t TEZOS_ICON_FONT; /* MEK-Dings 24px — see docs/design-system/ICONS.md for the codepoint mapping */
