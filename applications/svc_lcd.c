#include <rtthread.h>

#include "board.h"
#include "hpm_gpiom_drv.h"
#include "hpm_gpiom_soc_drv.h"
#include "hpm_gpio_drv.h"
#include "hpm_spi_drv.h"
#include "app_config.h"
#include "svc_lcd.h"

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

static const uint8_t g_cn12_lian[24] = {
    0x42,0x00,0xf4,0x07,0x44,0x00,0xa0,0x00,
    0xf7,0x03,0x84,0x00,0xf4,0x07,0x84,0x00,
    0x84,0x00,0x8a,0x00,0xf1,0x07,0x00,0x00,
};

static const uint8_t g_cn12_xu[24] = {
    0x84,0x00,0xe4,0x03,0x82,0x00,0xea,0x07,
    0x27,0x05,0x54,0x01,0x22,0x01,0xf7,0x07,
    0x80,0x00,0x4c,0x03,0x33,0x04,0x00,0x00,
};

static const uint8_t g_cn12_jia[24] = {
    0x04,0x00,0xbf,0x07,0xa4,0x04,0xba,0x07,
    0x01,0x00,0xfe,0x01,0x08,0x01,0xf8,0x07,
    0x00,0x04,0xff,0x04,0x00,0x03,0x00,0x00,
};

static const uint8_t g_cn12_shi[24] = {
    0x0f,0x01,0x08,0x01,0xea,0x07,0x2a,0x05,
    0x2a,0x05,0xfe,0x07,0x10,0x05,0x57,0x01,
    0x90,0x00,0x50,0x01,0x2c,0x06,0x00,0x00,
};

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



static const uint8_t *lcd_font5x7_get(char ch)
{
    static const uint8_t font_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t font_slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
    static const uint8_t font_colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t font_0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
    static const uint8_t font_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
    static const uint8_t font_2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
    static const uint8_t font_3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
    static const uint8_t font_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
    static const uint8_t font_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
    static const uint8_t font_6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
    static const uint8_t font_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
    static const uint8_t font_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t font_9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
    static const uint8_t font_G[5] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
    static const uint8_t font_k[5] = {0x7F, 0x10, 0x28, 0x44, 0x00};
    static const uint8_t font_m[5] = {0x7C, 0x04, 0x18, 0x04, 0x78};
    static const uint8_t font_h[5] = {0x7F, 0x08, 0x04, 0x04, 0x78};

    switch (ch) {
    case '0': return font_0;
    case '1': return font_1;
    case '2': return font_2;
    case '3': return font_3;
    case '4': return font_4;
    case '5': return font_5;
    case '6': return font_6;
    case '7': return font_7;
    case '8': return font_8;
    case '9': return font_9;
    case ':': return font_colon;
    case '/': return font_slash;
    case 'G': return font_G;
    case 'k': return font_k;
    case 'm': return font_m;
    case 'h': return font_h;
    case ' ': return font_space;
    default:  return font_space;
    }
}

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

static void lcd_fb_draw_char5x7(uint8_t x, uint8_t y, char ch)
{
    const uint8_t *glyph = lcd_font5x7_get(ch);

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (bits & (1U << row)) {
                lcd_fb_set_pixel((uint8_t)(x + col), (uint8_t)(y + row), RT_TRUE);
            }
        }
    }
}

static void lcd_fb_draw_string5x7(uint8_t x, uint8_t y, const char *str)
{
    while (*str != '\0') {
        lcd_fb_draw_char5x7(x, y, *str++);
        x = (uint8_t)(x + 6U);
    }
}

static void lcd_fb_draw_char5x7_scaled(uint8_t x, uint8_t y, char ch, uint8_t scale)
{
    const uint8_t *glyph = lcd_font5x7_get(ch);

    if (scale == 0U) {
        return;
    }

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (bits & (1U << row)) {
                lcd_fb_fill_rect((uint8_t)(x + col * scale),
                                 (uint8_t)(y + row * scale),
                                 scale,
                                 scale);
            }
        }
    }
}

static void lcd_fb_draw_string5x7_scaled(uint8_t x, uint8_t y, const char *str, uint8_t scale)
{
    while (*str != '\0') {
        lcd_fb_draw_char5x7_scaled(x, y, *str++, scale);
        x = (uint8_t)(x + (6U * scale));
    }
}

static uint8_t lcd_string_width5x7(const char *str, uint8_t scale)
{
    uint8_t len = 0;

    while (*str != '\0') {
        len++;
        str++;
    }

    return (uint8_t)(len * 6U * scale);
}

static void lcd_fb_draw_string5x7_scaled_right(uint8_t right_x, uint8_t y, const char *str, uint8_t scale)
{
    uint8_t width;

    width = lcd_string_width5x7(str, scale);
    if (width > right_x) {
        lcd_fb_draw_string5x7_scaled(0, y, str, scale);
    } else {
        lcd_fb_draw_string5x7_scaled((uint8_t)(right_x - width), y, str, scale);
    }
}

static void lcd_fb_draw_bitmap12x12(uint8_t x, uint8_t y, const uint8_t glyph[24])
{
    for (uint8_t row = 0; row < 12; row++) {
        uint8_t src_row = (uint8_t)(11U - row);
        uint16_t bits = (uint16_t)glyph[src_row * 2]
                      | ((uint16_t)glyph[src_row * 2 + 1] << 8);
        uint8_t draw_y = (uint8_t)(y + (row & 0xF8U) + (7U - (row & 0x07U)));

        for (uint8_t col = 0; col < 12; col++) {
            if (bits & (uint16_t)(1U << col)) {
                lcd_fb_set_pixel((uint8_t)(x + col), draw_y, RT_TRUE);
            }
        }
    }
}



static void lcd_fb_draw_cn12_string_lxjs(uint8_t x, uint8_t y)
{
    lcd_fb_draw_bitmap12x12(x, y, g_cn12_lian);
    lcd_fb_draw_bitmap12x12((uint8_t)(x + 12), y, g_cn12_xu);
    lcd_fb_draw_bitmap12x12((uint8_t)(x + 24), y, g_cn12_jia);
    lcd_fb_draw_bitmap12x12((uint8_t)(x + 36), y, g_cn12_shi);
}

static void lcd_fb_draw_top_icons_12x12(uint8_t x, uint8_t y)
{
    lcd_fb_draw_bitmap12x12(x, y, g_icon_signal_12x12);
    lcd_fb_draw_bitmap12x12((uint8_t)(x + 18), y, g_icon_g_12x12);
    lcd_fb_draw_bitmap12x12((uint8_t)(x + 36), y, g_icon_status_12x12);
    lcd_fb_draw_string5x7((uint8_t)(x + 50), (uint8_t)(y + 1), "05");
}

static void lcd_fb_flush(void)
{
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        lcd_set_page_col(page, 0);
        lcd_write_data_buf(g_lcd_fb[page], LCD_COLS);
    }
}




static void lcd_render_home_ui(void)
{
    uint8_t safe_left = 0;
    uint8_t safe_right = (uint8_t)(LCD_COLS - 1U);
    uint8_t status_y = 0;
    uint8_t row1_y = 16;
    uint8_t row2_y = 32;
    uint8_t row3_y = 48;

    lcd_fb_clear();

    /* 第1行位置显示原第2行内容 */
    lcd_fb_draw_string5x7((uint8_t)(safe_left + 2), 1, "0 km/h");
    lcd_fb_draw_string5x7_scaled_right(safe_right, 1, "09:16:45", 1);


    /* 第2行位置显示原第1行内容 */
    lcd_fb_draw_top_icons_12x12(safe_left, (uint8_t)(row1_y ));


    /* 第3行位置显示原第4行内容 */
    lcd_fb_fill_rect((uint8_t)(safe_left + 1), (uint8_t)(row2_y + 1), 6, 6);
    lcd_fb_draw_string5x7((uint8_t)(safe_left + 10), row2_y, "800000000000255304");

    /* 第4行位置显示原第3行内容 */
    lcd_fb_draw_cn12_string_lxjs((uint8_t)(safe_left + 2), row3_y);
    lcd_fb_draw_string5x7_scaled_right((uint8_t)(safe_right - 2), row3_y, "00:00:00", 1);

    lcd_fb_flush();
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

    APP_NON_CAN_LOG("LCD ST7567 init done\r\n");
    return RT_EOK;
}

static void svc_lcd_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    lcd_backlight_on();
    lcd_render_home_ui();
    APP_NON_CAN_LOG("LCD: home ui rendered\r\n");

    while (1)
    {
        rt_thread_mdelay(1000);
    }
}

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
















