#ifndef LCD_UI_DRAW_H
#define LCD_UI_DRAW_H

#include <stdint.h>
#include "u8g2.h"

uint16_t lcd_u8g2_draw_unicode_seq(u8g2_t *u8g2,
                                   uint16_t x,
                                   uint16_t y,
                                   const uint16_t *codes,
                                   uint8_t count);
uint16_t lcd_u8g2_get_unicode_seq_width(u8g2_t *u8g2,
                                        const uint16_t *codes,
                                        uint8_t count);

#endif /* LCD_UI_DRAW_H */
