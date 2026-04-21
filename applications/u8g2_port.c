#include "u8g2_port.h"
#include "svc_lcd.h"
#include <rtthread.h>

static u8g2_t g_u8g2;

static uint8_t u8x8_byte_st7567_bb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    uint8_t *data;

    switch (msg)
    {
    case U8X8_MSG_BYTE_INIT:
        return 1;

    case U8X8_MSG_BYTE_SET_DC:
        lcd_a0_set(arg_int ? RT_TRUE : RT_FALSE);
        return 1;

    case U8X8_MSG_BYTE_START_TRANSFER:
        lcd_csn_set(RT_TRUE);
        return 1;

    case U8X8_MSG_BYTE_END_TRANSFER:
        lcd_csn_set(RT_FALSE);
        return 1;

    case U8X8_MSG_BYTE_SEND:
        data = (uint8_t *)arg_ptr;
        while (arg_int--)
        {
            lcd_spi_send_byte(*data++);
        }
        return 1;
    }

    RT_UNUSED(u8x8);
    RT_UNUSED(arg_ptr);
    return 0;
}

static uint8_t u8x8_gpio_and_delay_st7567(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        return 1;

    case U8X8_MSG_DELAY_MILLI:
        rt_thread_mdelay(arg_int);
        return 1;

    case U8X8_MSG_GPIO_RESET:
        lcd_rst_set(arg_int ? RT_TRUE : RT_FALSE);
        return 1;

    case U8X8_MSG_GPIO_DC:
        lcd_a0_set(arg_int ? RT_TRUE : RT_FALSE);
        return 1;

    case U8X8_MSG_GPIO_CS:
        lcd_csn_set(arg_int ? RT_TRUE : RT_FALSE);
        return 1;
    }

    RT_UNUSED(u8x8);
    RT_UNUSED(arg_ptr);
    return 0;
}

static void u8g2_copy_to_lcd_fb(void)
{
    uint8_t *buf;
    uint16_t page_stride;

    buf = u8g2_GetBufferPtr(&g_u8g2);
    page_stride = (uint16_t)(u8g2_GetBufferTileWidth(&g_u8g2) * 8U);

    lcd_fb_public_clear();
    lcd_fb_public_copy_pages(buf, page_stride);
    lcd_fb_public_flush();
}

void u8g2_port_init(void)
{
    u8g2_Setup_st7567_pi_132x64_f(
        &g_u8g2,
        U8G2_R0,
        u8x8_byte_st7567_bb,
        u8x8_gpio_and_delay_st7567);

//    u8g2_InitDisplay(&g_u8g2);
//    u8g2_SetPowerSave(&g_u8g2, 0);
//
//    u8g2_ClearBuffer(&g_u8g2);
//    u8g2_copy_to_lcd_fb();
}

void u8g2_port_clear_buffer(void)
{
    u8g2_ClearBuffer(&g_u8g2);
}

void u8g2_port_flush_buffer(void)
{
    u8g2_copy_to_lcd_fb();
}

void u8g2_port_test_draw(void)
{
    uint8_t i, j;

    u8g2_ClearBuffer(&g_u8g2);
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);

    for (i = 0; i < 8; i++) {
        uint8_t y = (uint8_t)(i * 8U + 10U);
        for (j = 0; j <= i; j++) {
            uint8_t x = (uint8_t)(j * 8U);
            u8g2_DrawStr(&g_u8g2, x, y, "F");
        }
    }

    u8g2_copy_to_lcd_fb();
}

u8g2_t *u8g2_port_get(void)
{
    return &g_u8g2;
}
