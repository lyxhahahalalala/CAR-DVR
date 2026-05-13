#include "lcd_ui_draw.h"

uint16_t lcd_u8g2_draw_unicode_seq(u8g2_t *u8g2,
                                   uint16_t x,
                                   uint16_t y,
                                   const uint16_t *codes,
                                   uint8_t count)
{
    uint8_t i;

    for (i = 0U; i < count; i++) {
        x += u8g2_DrawGlyph(u8g2, x, y, codes[i]);
    }

    return x;
}

uint16_t lcd_u8g2_get_unicode_seq_width(u8g2_t *u8g2,
                                        const uint16_t *codes,
                                        uint8_t count)
{
    uint16_t width = 0U;
    uint8_t i;

    if ((u8g2 == 0) || (codes == 0)) {
        return 0U;
    }

    for (i = 0U; i < count; i++) {
        width += u8g2_GetGlyphWidth(u8g2, codes[i]);
    }

    return width;
}
