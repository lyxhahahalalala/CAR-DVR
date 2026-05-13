#include "lcd_graphics.h"

#include <string.h>

#include "lcd_drv.h"

#define LCD_COLS    132
#define LCD_PAGES   8
#define LCD_ROWS    64

static uint8_t g_lcd_fb[LCD_PAGES][LCD_COLS];

static void lcd_fb_clear(void)
{
    rt_memset(g_lcd_fb, 0, sizeof(g_lcd_fb));
}

static void lcd_fb_set_pixel(uint8_t x, uint8_t y, rt_bool_t on)
{
    uint8_t page;
    uint8_t bit;

    if ((x >= LCD_COLS) || (y >= LCD_ROWS)) {
        return;
    }

    page = (uint8_t)(y / 8U);
    bit = (uint8_t)(7U - (y % 8U));

    if (on) {
        g_lcd_fb[page][x] |= (uint8_t)(1U << bit);
    } else {
        g_lcd_fb[page][x] &= (uint8_t)~(uint8_t)(1U << bit);
    }
}

static uint8_t lcd_reverse_byte(uint8_t v)
{
    v = (uint8_t)(((v & 0xF0U) >> 4) | ((v & 0x0FU) << 4));
    v = (uint8_t)(((v & 0xCCU) >> 2) | ((v & 0x33U) << 2));
    v = (uint8_t)(((v & 0xAAU) >> 1) | ((v & 0x55U) << 1));
    return v;
}

static void lcd_fb_flush(void)
{
    uint8_t page;

    for (page = 0; page < LCD_PAGES; page++) {
        lcd_drv_write_page(page, 0, g_lcd_fb[page], LCD_COLS);
    }
}

void lcd_fb_public_clear(void)
{
    lcd_fb_clear();
}

void lcd_fb_public_set_pixel(uint8_t x, uint8_t y, rt_bool_t on)
{
    lcd_fb_set_pixel(x, y, on);
}

void lcd_fb_public_flush(void)
{
    lcd_fb_flush();
}

void lcd_fb_public_copy_pages(const uint8_t *src, uint16_t src_stride)
{
    uint8_t src_page;
    uint16_t x;

    if (src == RT_NULL) {
        return;
    }

    for (src_page = 0; src_page < LCD_PAGES; src_page++) {
        uint8_t dst_page = (uint8_t)(src_page ^ 3U);
        const uint8_t *page_ptr = src + src_page * src_stride;

        for (x = 0; x < LCD_COLS; x++) {
            g_lcd_fb[dst_page][x] = lcd_reverse_byte(page_ptr[x]);
        }
    }
}
