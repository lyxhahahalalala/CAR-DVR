#include <rtthread.h>

#include "board.h"
#include "hpm_gpiom_drv.h"
#include "hpm_gpiom_soc_drv.h"
#include "hpm_gpio_drv.h"
#include "hpm_spi_drv.h"
#include "app_config.h"
#include "svc_lcd.h"
#include "svc_adc.h"
#include "u8g2_port.h"

/*
 * LCD 相关控制脚，按当前已打通的连线整理如下：
 * PA03 -> LCD_RSTB      硬件复位，低有效
 * PC16 -> LCD_LED_A     背光阳极，高电平开启
 * PA28 -> LCD_CSN       SPI 片选，低有效（GPIO 控制）
 * PA29 -> LCD_A0        命令/数据选择，0=命令，1=数据
 * PA30 -> LCD_SCK       SPI 时钟
 * PA31 -> LCD_SDA       SPI 数据
 */

#define LCD_RSTB_GPIO_CTRL      HPM_GPIO0
#define LCD_RSTB_GPIO_INDEX     GPIO_DO_GPIOA
#define LCD_RSTB_GPIO_OE        GPIO_OE_GPIOA
#define LCD_RSTB_PIN            3

#define LCD_BACKLIGHT_GPIO_CTRL  HPM_GPIO0
#define LCD_BACKLIGHT_GPIO_INDEX GPIO_DO_GPIOC
#define LCD_BACKLIGHT_GPIO_OE    GPIO_OE_GPIOC
#define LCD_BACKLIGHT_PIN        16

#define LCD_CSN_GPIO_CTRL       HPM_GPIO0
#define LCD_CSN_GPIO_INDEX      GPIO_DO_GPIOA
#define LCD_CSN_GPIO_OE         GPIO_OE_GPIOA
#define LCD_CSN_PIN             28

#define LCD_A0_GPIO_CTRL        HPM_GPIO0
#define LCD_A0_GPIO_INDEX       GPIO_DO_GPIOA
#define LCD_A0_GPIO_OE          GPIO_OE_GPIOA
#define LCD_A0_PIN              29

#define LCD_SCK_GPIO_CTRL       HPM_GPIO0
#define LCD_SCK_GPIO_INDEX      GPIO_DO_GPIOA
#define LCD_SCK_GPIO_OE         GPIO_OE_GPIOA
#define LCD_SCK_PIN             30

#define LCD_SDA_GPIO_CTRL       HPM_GPIO0
#define LCD_SDA_GPIO_INDEX      GPIO_DO_GPIOA
#define LCD_SDA_GPIO_OE         GPIO_OE_GPIOA
#define LCD_SDA_PIN             31

#define LCD_SW_SPI_SWAP_LINES   0
#define LCD_SW_SPI_IDLE_HIGH    1

#if LCD_SW_SPI_SWAP_LINES
#define LCD_CLK_GPIO_CTRL       LCD_SDA_GPIO_CTRL
#define LCD_CLK_GPIO_INDEX      LCD_SDA_GPIO_INDEX
#define LCD_CLK_PIN             LCD_SDA_PIN
#define LCD_DAT_GPIO_CTRL       LCD_SCK_GPIO_CTRL
#define LCD_DAT_GPIO_INDEX      LCD_SCK_GPIO_INDEX
#define LCD_DAT_PIN             LCD_SCK_PIN
#else
#define LCD_CLK_GPIO_CTRL       LCD_SCK_GPIO_CTRL
#define LCD_CLK_GPIO_INDEX      LCD_SCK_GPIO_INDEX
#define LCD_CLK_PIN             LCD_SCK_PIN
#define LCD_DAT_GPIO_CTRL       LCD_SDA_GPIO_CTRL
#define LCD_DAT_GPIO_INDEX      LCD_SDA_GPIO_INDEX
#define LCD_DAT_PIN             LCD_SDA_PIN
#endif

#define ST7567_CMD_DISPLAY_ON       0xAF
#define ST7567_CMD_SET_START_LINE   0x40
#define ST7567_CMD_SET_PAGE         0xB0
#define ST7567_CMD_SET_COL_HI       0x10
#define ST7567_CMD_SET_COL_LO       0x00
#define ST7567_CMD_SEG_NORMAL       0xA0
#define ST7567_CMD_SEG_REVERSE      0xA1
#define ST7567_CMD_INVERSE_OFF      0xA6
#define ST7567_CMD_ALL_PIXEL_OFF    0xA4
#define ST7567_CMD_BIAS_1_9         0xA2
#define ST7567_CMD_COM_REVERSE      0xC8
#define ST7567_CMD_POWER_ALL_ON     0x2F
#define ST7567_CMD_REG_RATIO        0x20
#define ST7567_CMD_SET_EV           0x81
#define ST7567_CMD_SET_BOOSTER      0xF8
#define ST7567_CMD_BOOSTER_X5       0x01
#define ST7567_CMD_COM_NORMAL       0xC0


#define ST7567_INIT_EV              0x28
#define ST7567_INIT_REG_RATIO       0x07

#define LCD_COLS                    132
#define LCD_PAGES                   8
#define LCD_ROWS                    64

#define LCD_UI_MARGIN_LEFT          6
#define LCD_UI_MARGIN_RIGHT         6
#define LCD_UI_MARGIN_TOP           4
#define LCD_UI_MARGIN_BOTTOM        4
#define LCD_HW_COL_OFFSET           0//4

static uint8_t g_lcd_fb[LCD_PAGES][LCD_COLS];

/* Active LCD UI fonts */
#define LCD_FONT_ASCII_SMALL    u8g2_font_6x10_tf
#define LCD_FONT_CN_12          u8g2_font_wqy12_t_gb2312


typedef struct
{
    uint16_t speed_kmh;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t drive_hour;
    uint8_t drive_minute;
    uint8_t drive_second;
    char card_id[20];
} lcd_home_ui_data_t;

static lcd_home_ui_data_t g_lcd_home_ui = {
    .speed_kmh = 0U,
    .hour = 9U,
    .minute = 16U,
    .second = 45U,
    .drive_hour = 0U,
    .drive_minute = 0U,
    .drive_second = 0U,
    .card_id = "800000000000255304"
};

char speed_str[16];
char time_str[16];
char drive_time_str[16];

static const uint8_t g_icon_signal_12x12[24] = {
    0x00,0x00,0x00,0xe0,0x3e,0xe0,0x1e,0xc0,
    0x0c,0xc2,0x0c,0xc3,0x8c,0xc3,0xcc,0xc3,
    0xec,0xc3,0xfc,0x03,0x00,0x00,0x00,0xf0,
};

static const uint8_t g_icon_g_12x12[24] = {
    0xff,0x3f,0x03,0x3c,0x03,0x3c,0xf3,0xbd,
    0x1b,0xbc,0x9b,0xbd,0x1b,0xbd,0xfb,0x3d,
    0xf3,0x3c,0x03,0xfc,0xff,0xff,0xff,0x0f,
};

static const uint8_t g_icon_status_12x12[24] = {
    0x00,0xc0,0x0c,0x46,0xb4,0xc5,0xcc,0x84,
    0xa8,0x02,0x10,0x83,0xb8,0x83,0xc8,0x44,
    0xe4,0xc5,0x1c,0x06,0x00,0x00,0x00,0xf0,
};


static const uint16_t g_menu_title_u8g2[] = {
    0x83DC, /* 菜 */
    0x5355  /* 单 */
};

static const uint16_t g_menu_item_text_1[] = {
    0x884C, 0x9A76, 0x8BB0, 0x5F55 /* 行驶记录 */
};

static const uint16_t g_menu_item_text_2[] = {
    0x8BBE, 0x5907, 0x72B6, 0x6001 /* 设备状态 */
};

static const uint16_t g_menu_item_text_3[] = {
    0x7CFB, 0x7EDF, 0x8BBE, 0x7F6E /* 系统设置 */
};

static const uint16_t g_menu_item_text_4[] = {
    0x5B9A, 0x4F4D, 0x6A21, 0x5757, 0x72B6, 0x6001 /* 定位模块状态 */
};

static const uint16_t g_menu_item_text_5[] = {
    0x6574, 0x8F66, 0x72B6, 0x6001 /* 整车状态 */
};

static const uint16_t g_menu_item_text_6[] = {
    0x5B58, 0x50A8, 0x5668, 0x68C0, 0x6D4B /* 存储器检测 */
};

static const uint16_t g_menu_item_text_7[] = {
    0x5F55, 0x97F3, 0x5F55, 0x50CF, 0x68C0, 0x6D4B /* 录音录像检测 */
};

static const uint16_t *const g_menu_item_texts_u8g2[] = {
    g_menu_item_text_1,
    g_menu_item_text_2,
    g_menu_item_text_3,
    g_menu_item_text_4,
    g_menu_item_text_5,
    g_menu_item_text_6,
    g_menu_item_text_7
};

static const uint8_t g_menu_item_text_counts_u8g2[] = {4, 4, 4, 6, 4, 5, 6};



static void svc_lcd_spi_hw_init(void)
{
    HPM_IOC->PAD[IOC_PAD_PA30].FUNC_CTL = IOC_PA30_FUNC_CTL_GPIO_A_30;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_SCK_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_SCK_GPIO_CTRL, LCD_SCK_GPIO_OE, LCD_SCK_PIN);

    HPM_IOC->PAD[IOC_PAD_PA31].FUNC_CTL = IOC_PA31_FUNC_CTL_GPIO_A_31;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_SDA_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_SDA_GPIO_CTRL, LCD_SDA_GPIO_OE, LCD_SDA_PIN);

#if LCD_SW_SPI_IDLE_HIGH
    gpio_write_pin(LCD_CLK_GPIO_CTRL, LCD_CLK_GPIO_INDEX, LCD_CLK_PIN, 1);
#else
    gpio_write_pin(LCD_CLK_GPIO_CTRL, LCD_CLK_GPIO_INDEX, LCD_CLK_PIN, 0);
#endif
    gpio_write_pin(LCD_DAT_GPIO_CTRL, LCD_DAT_GPIO_INDEX, LCD_DAT_PIN, 0);
}

static void svc_lcd_ctrl_pins_init(void)
{
    HPM_IOC->PAD[IOC_PAD_PA03].FUNC_CTL = IOC_PA03_FUNC_CTL_GPIO_A_03;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_RSTB_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_OE, LCD_RSTB_PIN);

    HPM_IOC->PAD[IOC_PAD_PC16].FUNC_CTL = IOC_PC16_FUNC_CTL_GPIO_C_16;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, LCD_BACKLIGHT_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_BACKLIGHT_GPIO_CTRL, LCD_BACKLIGHT_GPIO_OE, LCD_BACKLIGHT_PIN);

    HPM_IOC->PAD[IOC_PAD_PA28].FUNC_CTL = IOC_PA28_FUNC_CTL_GPIO_A_28;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_CSN_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_CSN_GPIO_CTRL, LCD_CSN_GPIO_OE, LCD_CSN_PIN);

    HPM_IOC->PAD[IOC_PAD_PA29].FUNC_CTL = IOC_PA29_FUNC_CTL_GPIO_A_29;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_A0_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_A0_GPIO_CTRL, LCD_A0_GPIO_OE, LCD_A0_PIN);
}

static void lcd_spi_delay_us(void)
{
    board_delay_us(1);
}

static void lcd_sck_set(rt_bool_t high)
{
    gpio_write_pin(LCD_CLK_GPIO_CTRL, LCD_CLK_GPIO_INDEX, LCD_CLK_PIN, high ? 1 : 0);
}

static void lcd_sda_set(rt_bool_t high)
{
    gpio_write_pin(LCD_DAT_GPIO_CTRL, LCD_DAT_GPIO_INDEX, LCD_DAT_PIN, high ? 1 : 0);
}

void lcd_reset(void)
{
    gpio_write_pin(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_INDEX, LCD_RSTB_PIN, 0);
    rt_thread_mdelay(20);
    gpio_write_pin(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_INDEX, LCD_RSTB_PIN, 1);
    rt_thread_mdelay(5);
}

void lcd_rst_set(rt_bool_t active)
{
    gpio_write_pin(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_INDEX, LCD_RSTB_PIN, active ? 0 : 1);
}

void lcd_backlight_on(void)
{
    gpio_write_pin(LCD_BACKLIGHT_GPIO_CTRL, LCD_BACKLIGHT_GPIO_INDEX, LCD_BACKLIGHT_PIN, 1);
}

void lcd_backlight_off(void)
{
    gpio_write_pin(LCD_BACKLIGHT_GPIO_CTRL, LCD_BACKLIGHT_GPIO_INDEX, LCD_BACKLIGHT_PIN, 0);
}

void lcd_a0_set(rt_bool_t is_data)
{
    gpio_write_pin(LCD_A0_GPIO_CTRL, LCD_A0_GPIO_INDEX, LCD_A0_PIN, is_data ? 1 : 0);
}

void lcd_csn_set(rt_bool_t active)
{
    gpio_write_pin(LCD_CSN_GPIO_CTRL, LCD_CSN_GPIO_INDEX, LCD_CSN_PIN, active ? 0 : 1);
}

static void lcd_spi_write_byte(uint8_t byte)
{
    for (uint8_t bit = 0; bit < 8; bit++) {
#if LCD_SW_SPI_IDLE_HIGH
        lcd_sck_set(RT_TRUE);
        lcd_sda_set((byte & 0x80U) != 0U ? RT_TRUE : RT_FALSE);
        lcd_spi_delay_us();
        lcd_sck_set(RT_FALSE);
        lcd_spi_delay_us();
#else
        lcd_sck_set(RT_FALSE);
        lcd_sda_set((byte & 0x80U) != 0U ? RT_TRUE : RT_FALSE);
        lcd_spi_delay_us();
        lcd_sck_set(RT_TRUE);
        lcd_spi_delay_us();
#endif
        byte <<= 1;
    }

#if LCD_SW_SPI_IDLE_HIGH
    lcd_sck_set(RT_TRUE);
#else
    lcd_sck_set(RT_FALSE);
#endif
}

void lcd_spi_send_byte(uint8_t byte)
{
    lcd_spi_write_byte(byte);
}

static void lcd_write_cmd(uint8_t cmd)
{
    lcd_csn_set(RT_TRUE);
    lcd_a0_set(RT_FALSE);
    lcd_spi_write_byte(cmd);
    lcd_csn_set(RT_FALSE);
}

static void lcd_write_data_buf(const uint8_t *buf, uint16_t len)
{
    lcd_csn_set(RT_TRUE);
    lcd_a0_set(RT_TRUE);

    while (len-- > 0U) {
        lcd_spi_write_byte(*buf++);
    }

    lcd_csn_set(RT_FALSE);
}

static void lcd_set_page_col(uint8_t page, uint8_t col)
{
    uint8_t hw_col = (uint8_t)(col + LCD_HW_COL_OFFSET);

    if (hw_col >= LCD_COLS) {
        hw_col = (uint8_t)(hw_col - LCD_COLS);
    }

    lcd_write_cmd(ST7567_CMD_SET_PAGE | (page & 0x0F));
    lcd_write_cmd(ST7567_CMD_SET_COL_HI | ((hw_col >> 4) & 0x0F));
    lcd_write_cmd(ST7567_CMD_SET_COL_LO | (hw_col & 0x0F));
}

void lcd_clear(void)
{
    static const uint8_t zeros[LCD_COLS] = {0};
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        lcd_set_page_col(page, 0);
        lcd_write_data_buf(zeros, LCD_COLS);
    }
}

void lcd_fill_all(void)
{
    static uint8_t ones[LCD_COLS];
    for (uint8_t i = 0; i < LCD_COLS; i++) {
        ones[i] = 0xFF;
    }
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        lcd_set_page_col(page, 0);
        lcd_write_data_buf(ones, LCD_COLS);
    }
}

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
    bit = 7U - (y % 8U);

    if (on) {
        g_lcd_fb[page][x] |= (uint8_t)(1U << bit);
    } else {
        g_lcd_fb[page][x] &= (uint8_t)~(1U << bit);
    }
}


static void lcd_fb_hline(uint8_t x, uint8_t y, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        lcd_fb_set_pixel((uint8_t)(x + i), y, RT_TRUE);
    }
}

static void lcd_fb_vline(uint8_t x, uint8_t y, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        lcd_fb_set_pixel(x, (uint8_t)(y + i), RT_TRUE);
    }
}

static void lcd_fb_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    for (uint8_t yy = 0; yy < h; yy++) {
        for (uint8_t xx = 0; xx < w; xx++) {
            lcd_fb_set_pixel((uint8_t)(x + xx), (uint8_t)(y + yy), RT_TRUE);
        }
    }
}

static rt_bool_t g_lcd_menu_mode = RT_FALSE;
static uint8_t g_lcd_menu_index = 0;
static rt_bool_t g_lcd_need_redraw = RT_TRUE;


static void lcd_fb_flush(void);

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

//void lcd_fb_public_copy_pages(const uint8_t *src, uint16_t src_stride)
//{
//    uint8_t page;
//
//    if (src == RT_NULL) {
//        return;
//    }
//
//    for (page = 0; page < LCD_PAGES; page++) {
//        rt_memcpy(g_lcd_fb[page], src + page * src_stride, LCD_COLS);
//    }
//}

static uint8_t lcd_reverse_byte(uint8_t v)
{
    v = (uint8_t)(((v & 0xF0U) >> 4) | ((v & 0x0FU) << 4));
    v = (uint8_t)(((v & 0xCCU) >> 2) | ((v & 0x33U) << 2));
    v = (uint8_t)(((v & 0xAAU) >> 1) | ((v & 0x55U) << 1));
    return v;
}

//void lcd_fb_public_copy_pages(const uint8_t *src, uint16_t src_stride)
//{
//    uint8_t src_page;
//    uint16_t x;
//
//    if (src == RT_NULL) {
//        return;
//    }
//
//    for (src_page = 0; src_page < LCD_PAGES; src_page++) {
//        uint8_t dst_page = (uint8_t)(src_page ^ 1U);
//        //uint8_t dst_page = src_page;  // 去掉翻转
//        const uint8_t *page_ptr = src + src_page * src_stride;
//
//        for (x = 0; x < LCD_COLS; x++) {
//            g_lcd_fb[dst_page][x] = lcd_reverse_byte(page_ptr[x]);
//        }
//    }
//}

//void lcd_fb_public_copy_pages(const uint8_t *src, uint16_t src_stride)
//{
//    uint8_t src_page;
//    uint16_t x;
//
//    if (src == RT_NULL) {
//        return;
//    }
//
//    for (src_page = 0; src_page < LCD_PAGES; src_page++) {
//        uint8_t dst_page = (uint8_t)((src_page + LCD_PAGES - 1U) % LCD_PAGES);
//        const uint8_t *page_ptr = src + src_page * src_stride;
//
//        for (x = 0; x < LCD_COLS; x++) {
//            g_lcd_fb[dst_page][x] = lcd_reverse_byte(page_ptr[x]);
//        }
//    }
//}

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

static uint16_t lcd_u8g2_draw_unicode_seq(u8g2_t *u8g2,
                                          uint16_t x,
                                          uint16_t y,
                                          const uint16_t *codes,
                                          uint8_t count)
{
    uint8_t i;

    for (i = 0; i < count; i++) {
        x += u8g2_DrawGlyph(u8g2, x, y, codes[i]);
    }

    return x;
}

static void lcd_u8g2_draw_menu_item(u8g2_t *u8g2,
                                    uint8_t index,
                                    uint8_t baseline_y,
                                    rt_bool_t selected)
{
    char prefix[4];

    if (selected == RT_TRUE) {
        u8g2_SetDrawColor(u8g2, 1);
        u8g2_DrawBox(u8g2, 0, (uint8_t)(baseline_y - 11U), LCD_COLS, 13);
        u8g2_SetDrawColor(u8g2, 0);
    } else {
        u8g2_SetDrawColor(u8g2, 1);
    }

    rt_snprintf(prefix, sizeof(prefix), "%u.", (unsigned int)(index + 1U));

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 2, baseline_y, prefix);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2,
                              16,
                              baseline_y,
                              g_menu_item_texts_u8g2[index],
                              g_menu_item_text_counts_u8g2[index]);

    u8g2_SetDrawColor(u8g2, 1);
}


//菜鸟
static void lcd_render_menu_ui(void)
{
    u8g2_t *u8g2;
    uint8_t page_start;
    uint8_t row_count;
    uint8_t row_y[4];
    uint8_t row;

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    if (g_lcd_menu_index < 3U) {
        page_start = 0U;
        row_count = 3U;

        row_y[0] = 28U;
        row_y[1] = 42U;
        row_y[2] = 56U;

        u8g2_SetFont(u8g2, LCD_FONT_CN_12);
        lcd_u8g2_draw_unicode_seq(u8g2, 2, 12, g_menu_title_u8g2, 2);

        for (row = 0; row < row_count; row++) {
            uint8_t item_index = (uint8_t)(page_start + row);
            lcd_u8g2_draw_menu_item(u8g2,
                                    item_index,
                                    row_y[row],
                                    (item_index == g_lcd_menu_index) ? RT_TRUE : RT_FALSE);
        }
    } else {
        page_start = 3U;
        row_count = 4U;

        row_y[0] = 14U;
        row_y[1] = 28U;
        row_y[2] = 42U;
        row_y[3] = 56U;

        for (row = 0; row < row_count; row++) {
            uint8_t item_index = (uint8_t)(page_start + row);
            lcd_u8g2_draw_menu_item(u8g2,
                                    item_index,
                                    row_y[row],
                                    (item_index == g_lcd_menu_index) ? RT_TRUE : RT_FALSE);
        }
    }

    u8g2_port_flush_buffer();
}


static rt_bool_t lcd_home_ui_set_data(uint16_t speed_kmh,
                                      uint8_t hour,
                                      uint8_t minute,
                                      uint8_t second,
                                      uint8_t drive_hour,
                                      uint8_t drive_minute,
                                      uint8_t drive_second,
                                      const char *card_id)
{
    rt_bool_t changed = RT_FALSE;

    if (g_lcd_home_ui.speed_kmh != speed_kmh) {
        g_lcd_home_ui.speed_kmh = speed_kmh;
        changed = RT_TRUE;
    }

    if (g_lcd_home_ui.hour != hour) {
        g_lcd_home_ui.hour = hour;
        changed = RT_TRUE;
    }

    if (g_lcd_home_ui.minute != minute) {
        g_lcd_home_ui.minute = minute;
        changed = RT_TRUE;
    }

    if (g_lcd_home_ui.second != second) {
        g_lcd_home_ui.second = second;
        changed = RT_TRUE;
    }

    if (g_lcd_home_ui.drive_hour != drive_hour) {
        g_lcd_home_ui.drive_hour = drive_hour;
        changed = RT_TRUE;
    }

    if (g_lcd_home_ui.drive_minute != drive_minute) {
        g_lcd_home_ui.drive_minute = drive_minute;
        changed = RT_TRUE;
    }

    if (g_lcd_home_ui.drive_second != drive_second) {
        g_lcd_home_ui.drive_second = drive_second;
        changed = RT_TRUE;
    }

    if (card_id != RT_NULL) {
        if (rt_strncmp(g_lcd_home_ui.card_id, card_id, sizeof(g_lcd_home_ui.card_id)) != 0) {
            rt_strncpy(g_lcd_home_ui.card_id, card_id, sizeof(g_lcd_home_ui.card_id) - 1);
            g_lcd_home_ui.card_id[sizeof(g_lcd_home_ui.card_id) - 1] = '\0';
            changed = RT_TRUE;
        }
    }

    if ((changed == RT_TRUE) && (g_lcd_menu_mode == RT_FALSE)) {
        g_lcd_need_redraw = RT_TRUE;
    }

    return changed;
}


void svc_lcd_update_home_time(uint8_t hour, uint8_t minute, uint8_t second)
{
    (void)lcd_home_ui_set_data(g_lcd_home_ui.speed_kmh,
                               hour,
                               minute,
                               second,
                               g_lcd_home_ui.drive_hour,
                               g_lcd_home_ui.drive_minute,
                               g_lcd_home_ui.drive_second,
                               g_lcd_home_ui.card_id);
}




static void lcd_fb_flush(void)
{
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        lcd_set_page_col(page, 0);
        lcd_write_data_buf(g_lcd_fb[page], LCD_COLS);
    }
}



static void lcd_u8g2_draw_bitmap12x12(u8g2_t *u8g2,
                                      uint8_t x,
                                      uint8_t y,
                                      const uint8_t glyph[24])
{
    uint8_t xbmp[24];
    uint8_t row;
    uint8_t col;

    rt_memset(xbmp, 0, sizeof(xbmp));

    for (row = 0; row < 12; row++) {
        uint8_t src_row = row;
        uint16_t bits = (uint16_t)glyph[src_row * 2U]
                      | ((uint16_t)glyph[src_row * 2U + 1U] << 8);

        for (col = 0; col < 12; col++) {
            if (bits & (uint16_t)(1U << col)) {
                xbmp[row * 2U + (col >> 3)] |= (uint8_t)(1U << (col & 0x07U));
            }
        }
    }

    u8g2_DrawXBMP(u8g2, x, y, 12, 12, xbmp);
}

static void lcd_u8g2_draw_top_icons(u8g2_t *u8g2, uint8_t x, uint8_t y)
{
    lcd_u8g2_draw_bitmap12x12(u8g2, x, y, g_icon_signal_12x12);
    lcd_u8g2_draw_bitmap12x12(u8g2, (uint8_t)(x + 18U), y, g_icon_g_12x12);
    lcd_u8g2_draw_bitmap12x12(u8g2, (uint8_t)(x + 36U), y, g_icon_status_12x12);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, (uint8_t)(x + 50U), (uint8_t)(y + 10U), "05");
}


/*字模版的主界面UI*/
//static void lcd_render_home_ui(void)
//{
//    uint8_t safe_left = 0;
//    uint8_t safe_right = (uint8_t)(LCD_COLS - 1U);
//    uint8_t status_y = 0;
//    uint8_t row1_y = 16;
//    uint8_t row2_y = 32;
//    uint8_t row3_y = 48;
//
//    lcd_fb_clear();
//
//    /* 第2行内容 */
//    lcd_fb_draw_string5x7((uint8_t)(safe_left + 2), 1, "0 km/h");
//    lcd_fb_draw_string5x7_scaled_right(safe_right, 1, "09:16:45", 1);
//
//
//    /* 第1行内容 */
//    lcd_fb_draw_top_icons_12x12(safe_left, (uint8_t)(row1_y ));
//
//
//    /* 第4行内容 */
//    lcd_fb_fill_rect((uint8_t)(safe_left + 1), (uint8_t)(row2_y + 1), 6, 6);
//    lcd_fb_draw_string5x7((uint8_t)(safe_left + 10), row2_y, "800000000000255304");
//
//    /* 第3行内容 */
//    lcd_fb_draw_cn12_string_lxjs((uint8_t)(safe_left + 2), row3_y);
//    lcd_fb_draw_string5x7_scaled_right((uint8_t)(safe_right - 2), row3_y, "00:00:00", 1);
//
//    lcd_fb_flush();
//}




static void lcd_home_ui_test_update_every_second(void)
{
    static rt_tick_t last_tick = 0;
    static uint16_t speed_kmh = 0U;
    static uint8_t hour = 9U;
    static uint8_t minute = 16U;
    static uint8_t second = 45U;
    static uint8_t drive_hour = 0U;
    static uint8_t drive_minute = 0U;
    static uint8_t drive_second = 0U;
    static char card_id[20] = "800000000000255304";

    rt_tick_t now_tick;
    uint32_t card_num = 0;
    int i;

    now_tick = rt_tick_get();
    if ((last_tick != 0) && ((now_tick - last_tick) < RT_TICK_PER_SECOND)) {
        return;
    }
    last_tick = now_tick;

    speed_kmh++;

    second++;
    if (second >= 60U) {
        second = 0U;
        minute++;
        if (minute >= 60U) {
            minute = 0U;
            hour++;
            if (hour >= 24U) {
                hour = 0U;
            }
        }
    }

    drive_second++;
    if (drive_second >= 60U) {
        drive_second = 0U;
        drive_minute++;
        if (drive_minute >= 60U) {
            drive_minute = 0U;
            drive_hour++;
            if (drive_hour >= 100U) {
                drive_hour = 0U;
            }
        }
    }

    for (i = 0; i < 18 && card_id[i] != '\0'; i++) {
        if ((card_id[i] < '0') || (card_id[i] > '9')) {
            break;
        }
        card_num = card_num * 10U + (uint32_t)(card_id[i] - '0');
    }

    card_num++;
    rt_snprintf(card_id, sizeof(card_id), "%018lu", (unsigned long)card_num);

    lcd_home_ui_set_data(speed_kmh,
                         hour,
                         minute,
                         second,
                         drive_hour,
                         drive_minute,
                         drive_second,
                         card_id);
}




/*u8g2库的UI主界面*/
/* Home screen rendered with u8g2 resources. */
static void lcd_render_home_ui(void)
{
    u8g2_t *u8g2;
    static const uint16_t g_cn_lxjs[] = {
        0x8FDE, /* 连 */
        0x7EED, /* 续 */
        0x9A7E, /* 驾 */
        0x9A76  /* 驶 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();



    rt_snprintf(speed_str, sizeof(speed_str), "%u km/h",
                (unsigned int)g_lcd_home_ui.speed_kmh);

    rt_snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u",
                (unsigned int)g_lcd_home_ui.hour,
                (unsigned int)g_lcd_home_ui.minute,
                (unsigned int)g_lcd_home_ui.second);

    rt_snprintf(drive_time_str, sizeof(drive_time_str), "%02u:%02u:%02u",
                (unsigned int)g_lcd_home_ui.drive_hour,
                (unsigned int)g_lcd_home_ui.drive_minute,
                (unsigned int)g_lcd_home_ui.drive_second);




    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第1行：先不画顶部图标，避免旧 framebuffer 坐标打架 */
    lcd_u8g2_draw_top_icons(u8g2, 2, 2);

    /* 第2行：速度 + 时间 */
    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 10, 24, speed_str);
    u8g2_DrawStr(u8g2, 72, 24, time_str);

    /* 第3行：连续驾驶 + 时长 */
    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 8, 40, g_cn_lxjs, 4);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 72, 40, drive_time_str);

    /* 第4行：黑块 + ID */
    u8g2_DrawBox(u8g2, 2, 50, 6, 8);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 12, 58, g_lcd_home_ui.card_id);

    u8g2_port_flush_buffer();
}

static void lcd_render_boot_check_ui(void)
{
    u8g2_t *u8g2;
    static const uint16_t g_boot_check_text[] = {
        0x6B63, /* 正 */
        0x5728, /* 在 */
        0x81EA, /* 自 */
        0x68C0  /* 检 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    /* 居中偏上一点，接近你给的示意图 */
    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 28, 30, g_boot_check_text, 4);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 76, 30, "......");

    u8g2_port_flush_buffer();
}




static void st7567_init_seq(void)
{
    rt_thread_mdelay(10);
    lcd_reset();

    lcd_write_cmd(ST7567_CMD_BIAS_1_9);
    lcd_write_cmd(ST7567_CMD_SEG_NORMAL);
    lcd_write_cmd(ST7567_CMD_COM_NORMAL);
    lcd_write_cmd(ST7567_CMD_SET_START_LINE | 0x00);
    lcd_write_cmd(ST7567_CMD_REG_RATIO | ST7567_INIT_REG_RATIO);
    lcd_write_cmd(ST7567_CMD_SET_EV);
    lcd_write_cmd(ST7567_INIT_EV);
    lcd_write_cmd(ST7567_CMD_SET_BOOSTER);
    lcd_write_cmd(ST7567_CMD_BOOSTER_X5);
    lcd_write_cmd(ST7567_CMD_POWER_ALL_ON);
    rt_thread_mdelay(120);
    lcd_write_cmd(ST7567_CMD_INVERSE_OFF);
    lcd_write_cmd(ST7567_CMD_ALL_PIXEL_OFF);
    lcd_clear();
    lcd_write_cmd(ST7567_CMD_DISPLAY_ON);
}

int svc_lcd_init(void)
{
    svc_lcd_ctrl_pins_init();
    svc_lcd_spi_hw_init();

    lcd_csn_set(RT_FALSE);
    lcd_a0_set(RT_TRUE);
    lcd_backlight_off();

    st7567_init_seq();
    u8g2_port_init();
    APP_NON_CAN_LOG("LCD ST7567 init done\r\n");
    return RT_EOK;
}



static void svc_lcd_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    lcd_backlight_on();

    g_lcd_menu_mode = RT_FALSE;
    g_lcd_menu_index = 0U;
    g_lcd_need_redraw = RT_TRUE;

    //lcd_render_home_ui();
    //g_lcd_need_redraw = RT_FALSE;
    APP_NON_CAN_LOG("LCD: ui thread start\r\n");

    rt_thread_mdelay(APP_ADC_STARTUP_DELAY_MS + 100);

    /* 开机自检页：停留 5 秒 */
        lcd_render_boot_check_ui();
        rt_thread_mdelay(5000);

        /* 进入主界面前触发一次重绘 */
        g_lcd_need_redraw = RT_TRUE;


        while (1)
           {
           // lcd_home_ui_test_update_every_second();//测试验证
               if (svc_adc_consume_s1_event() == RT_TRUE) {
                   if (g_lcd_menu_mode == RT_FALSE) {
                       g_lcd_menu_mode = RT_TRUE;
                       g_lcd_menu_index = 0U;
                   } else {
                       g_lcd_menu_mode = RT_FALSE;
                   }
                   g_lcd_need_redraw = RT_TRUE;
               }

               if (g_lcd_menu_mode == RT_TRUE) {
                   if (svc_adc_consume_s2_event() == RT_TRUE) {
                       if (g_lcd_menu_index > 0U) {
                           g_lcd_menu_index--;
                           g_lcd_need_redraw = RT_TRUE;
                       }
                   }

                   if (svc_adc_consume_s3_event() == RT_TRUE) {
                       if (g_lcd_menu_index < 6U) {
                           g_lcd_menu_index++;
                           g_lcd_need_redraw = RT_TRUE;
                       }
                   }

               }

               if (g_lcd_need_redraw == RT_TRUE) {
                   if (g_lcd_menu_mode == RT_TRUE) {
                       lcd_render_menu_ui();
                   } else {
                       lcd_render_home_ui();
                   }
                   g_lcd_need_redraw = RT_FALSE;
               }

               rt_thread_mdelay(10);
           }
       }

//static void svc_lcd_thread_entry(void *arg)
//{
//    RT_UNUSED(arg);
//
//    lcd_backlight_on();
//    /* 初始化显示主界面 */
//        lcd_render_home_ui();
//    while (1)
//    {
//        rt_thread_mdelay(1000);
//    }
//}



int svc_lcd_task_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(APP_LCD_TASK_NAME,
                              svc_lcd_thread_entry,
                              RT_NULL,
                              APP_LCD_TASK_STACK_SIZE,
                              APP_LCD_TASK_PRIORITY,
                              APP_LCD_TASK_TICK);
    if (thread == RT_NULL)
    {
        APP_NON_CAN_LOG("lcd thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}
















