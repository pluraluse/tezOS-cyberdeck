#include "tezos_fonts.h"
#include "fonts/tezos_mekmono_16.h"
#include "fonts/tezos_mekmono_20.h"
#include "fonts/tezos_mekmono_32.h"
#include "fonts/tezos_mekdings_24.h"
#include "fonts/tezos_grout_display_32.h"

/* All four text-font roles now use MonoMEK (CC0, Michael Alexander /
   MEK.txt) at different pixel sizes — Press Start 2P has been fully
   retired as a placeholder now that MonoMEK is confirmed cleared. */
const tezos_font_t TEZOS_FONT_HEADER = {
    tezos_mekmono_16_glyphs, TEZOS_MEKMONO_16_FIRST_CHAR,
    TEZOS_MEKMONO_16_LAST_CHAR, TEZOS_MEKMONO_16_PIXEL_SIZE, TEZOS_MEKMONO_16_LINE_HEIGHT
};
const tezos_font_t TEZOS_FONT_HERO = {
    tezos_mekmono_32_glyphs, TEZOS_MEKMONO_32_FIRST_CHAR,
    TEZOS_MEKMONO_32_LAST_CHAR, TEZOS_MEKMONO_32_PIXEL_SIZE, TEZOS_MEKMONO_32_LINE_HEIGHT
};
const tezos_font_t TEZOS_FONT_BODY = {
    tezos_mekmono_20_glyphs, TEZOS_MEKMONO_20_FIRST_CHAR,
    TEZOS_MEKMONO_20_LAST_CHAR, TEZOS_MEKMONO_20_PIXEL_SIZE, TEZOS_MEKMONO_20_LINE_HEIGHT
};
const tezos_font_t TEZOS_FONT_SMALL = {
    tezos_mekmono_16_glyphs, TEZOS_MEKMONO_16_FIRST_CHAR,
    TEZOS_MEKMONO_16_LAST_CHAR, TEZOS_MEKMONO_16_PIXEL_SIZE, TEZOS_MEKMONO_16_LINE_HEIGHT
};
const tezos_icon_font_t TEZOS_ICON_FONT = {
    tezos_mekdings_24_glyphs, TEZOS_MEKDINGS_24_FIRST_CHAR,
    TEZOS_MEKDINGS_24_LAST_CHAR, TEZOS_MEKDINGS_24_PIXEL_SIZE, TEZOS_MEKDINGS_24_LINE_HEIGHT
};

/* GROUT Display (CC0, Michael Alexander / MEK.txt) — the boot splash
   wordmark only, deliberately its own role rather than replacing
   TEZOS_FONT_HERO, since the idle clock also uses TEZOS_FONT_HERO and
   wasn't part of this request. See
   docs/build-notes/boot-splash-logo-font-build-notes.md. */
const tezos_font_t TEZOS_FONT_SPLASH_LOGO = {
    tezos_grout_display_32_glyphs, TEZOS_GROUT_DISPLAY_32_FIRST_CHAR,
    TEZOS_GROUT_DISPLAY_32_LAST_CHAR, TEZOS_GROUT_DISPLAY_32_PIXEL_SIZE, TEZOS_GROUT_DISPLAY_32_LINE_HEIGHT
};
