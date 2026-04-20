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

    return 0;
}

//static void u8g2_copy_to_lcd_fb(void)
//{
//    uint8_t *buf;
//    uint16_t page_stride;
//
//    buf = u8g2_GetBufferPtr(&g_u8g2);
//
//    /* u8g2 的 132x64 buffer 按 tile 宽度对齐到 17*8=136 字节每页 */
//    page_stride = (uint16_t)(u8g2_GetBufferTileWidth(&g_u8g2) * 8U);
//
//    lcd_fb_public_clear();
//    lcd_fb_public_copy_pages(buf, page_stride);
//    lcd_fb_public_flush();
//}

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
//    /* 先沿用当前屏已经验证过的扫描方向 */
//    u8x8_SendF(&g_u8g2.u8x8, "c", 0xA0); /* SEG_NORMAL */
//    u8x8_SendF(&g_u8g2.u8x8, "c", 0xC0); /* COM_NORMAL */
}




//void u8g2_port_test_draw(void)
//{
//    u8g2_ClearBuffer(&g_u8g2);
//    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
//    u8g2_DrawFrame(&g_u8g2, 0, 0, 132, 64);
//    u8g2_DrawStr(&g_u8g2, 2, 12, "HELLO");
//
//    u8g2_copy_to_lcd_fb();
//}

//void u8g2_port_test_draw(void)
//{
//    u8g2_ClearBuffer(&g_u8g2);
//
//    /* 绘制边框：距离屏幕边缘 2 像素，组成四周的框 */
//    u8g2_DrawFrame(&g_u8g2, 2, 2, 127, 59);  // x=2, y=2, 宽=127, 高=59
//
//    /* HELLO 在左上角 */
//    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
//    u8g2_DrawStr(&g_u8g2, 4, 12, "HELLO");
//
//    u8g2_copy_to_lcd_fb();
//}
void u8g2_port_test_draw(void)
{
    uint8_t i, j;

    u8g2_ClearBuffer(&g_u8g2);

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);

    /* 从上到下，第 i 行画 i+1 个 F */
    for (i = 0; i < 8; i++) {
        uint8_t y = (uint8_t)(i * 8U + 10);  // y 坐标，每行8像素，+10是字体基线偏移
        for (j = 0; j <= i; j++) {           // 画 i+1 个 F
            uint8_t x = (uint8_t)(j * 8U);   // x 坐标，每个F占8像素
            u8g2_DrawStr(&g_u8g2, x, y, "F");
        }
    }

    u8g2_copy_to_lcd_fb();
}




u8g2_t *u8g2_port_get(void)
{
    return &g_u8g2;
}

