#include "tezos_fonts.h"
#include "fonts/tezos_pressstart2p_16.h"
#include "fonts/tezos_pressstart2p_32.h"
#include "fonts/tezos_mekmono_16.h"
#include "fonts/tezos_mekmono_20.h"
#include "fonts/tezos_mekdings_24.h"

const tezos_font_t TEZOS_FONT_HEADER = {
    tezos_pressstart2p_16_glyphs, TEZOS_PRESSSTART2P_16_FIRST_CHAR,
    TEZOS_PRESSSTART2P_16_LAST_CHAR, TEZOS_PRESSSTART2P_16_PIXEL_SIZE, TEZOS_PRESSSTART2P_16_LINE_HEIGHT
};
const tezos_font_t TEZOS_FONT_HERO = {
    tezos_pressstart2p_32_glyphs, TEZOS_PRESSSTART2P_32_FIRST_CHAR,
    TEZOS_PRESSSTART2P_32_LAST_CHAR, TEZOS_PRESSSTART2P_32_PIXEL_SIZE, TEZOS_PRESSSTART2P_32_LINE_HEIGHT
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
