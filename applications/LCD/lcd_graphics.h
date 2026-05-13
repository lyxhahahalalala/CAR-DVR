#ifndef APPLICATIONS_LCD_LCD_GRAPHICS_H_
#define APPLICATIONS_LCD_LCD_GRAPHICS_H_

#include <rtthread.h>
#include <stdint.h>

void lcd_fb_public_clear(void);
void lcd_fb_public_set_pixel(uint8_t x, uint8_t y, rt_bool_t on);
void lcd_fb_public_flush(void);
void lcd_fb_public_copy_pages(const uint8_t *src, uint16_t src_stride);

#endif /* APPLICATIONS_LCD_LCD_GRAPHICS_H_ */
