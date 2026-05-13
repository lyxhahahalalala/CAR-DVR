#include "lcd_drv.h"

#include "board.h"
#include "hpm_gpiom_drv.h"
#include "hpm_gpiom_soc_drv.h"
#include "hpm_gpio_drv.h"

/*
 * ST7567 LCD pin mapping:
 * PA03 -> LCD_RSTB
 * PC16 -> LCD_LED_A
 * PA28 -> LCD_CSN
 * PA29 -> LCD_A0
 * PA30 -> LCD_SCK
 * PA31 -> LCD_SDA
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
#define ST7567_CMD_INVERSE_OFF      0xA6
#define ST7567_CMD_ALL_PIXEL_OFF    0xA4
#define ST7567_CMD_BIAS_1_9         0xA2
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
#define LCD_HW_COL_OFFSET           0

static void lcd_drv_spi_hw_init(void)
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

static void lcd_drv_ctrl_pins_init(void)
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

static void lcd_drv_spi_delay_us(void)
{
    board_delay_us(1);
}

static void lcd_drv_sck_set(rt_bool_t high)
{
    gpio_write_pin(LCD_CLK_GPIO_CTRL, LCD_CLK_GPIO_INDEX, LCD_CLK_PIN, high ? 1 : 0);
}

static void lcd_drv_sda_set(rt_bool_t high)
{
    gpio_write_pin(LCD_DAT_GPIO_CTRL, LCD_DAT_GPIO_INDEX, LCD_DAT_PIN, high ? 1 : 0);
}

static void lcd_drv_spi_write_byte(uint8_t byte)
{
    uint8_t bit;

    for (bit = 0; bit < 8U; bit++) {
#if LCD_SW_SPI_IDLE_HIGH
        lcd_drv_sck_set(RT_TRUE);
        lcd_drv_sda_set((byte & 0x80U) != 0U ? RT_TRUE : RT_FALSE);
        lcd_drv_spi_delay_us();
        lcd_drv_sck_set(RT_FALSE);
        lcd_drv_spi_delay_us();
#else
        lcd_drv_sck_set(RT_FALSE);
        lcd_drv_sda_set((byte & 0x80U) != 0U ? RT_TRUE : RT_FALSE);
        lcd_drv_spi_delay_us();
        lcd_drv_sck_set(RT_TRUE);
        lcd_drv_spi_delay_us();
#endif
        byte <<= 1;
    }

#if LCD_SW_SPI_IDLE_HIGH
    lcd_drv_sck_set(RT_TRUE);
#else
    lcd_drv_sck_set(RT_FALSE);
#endif
}

static void lcd_drv_write_cmd(uint8_t cmd)
{
    lcd_csn_set(RT_TRUE);
    lcd_a0_set(RT_FALSE);
    lcd_drv_spi_write_byte(cmd);
    lcd_csn_set(RT_FALSE);
}

static void lcd_drv_write_data_buf(const uint8_t *buf, uint16_t len)
{
    lcd_csn_set(RT_TRUE);
    lcd_a0_set(RT_TRUE);

    while (len-- > 0U) {
        lcd_drv_spi_write_byte(*buf++);
    }

    lcd_csn_set(RT_FALSE);
}

static void lcd_drv_set_page_col(uint8_t page, uint8_t col)
{
    uint8_t hw_col = (uint8_t)(col + LCD_HW_COL_OFFSET);

    if (hw_col >= LCD_COLS) {
        hw_col = (uint8_t)(hw_col - LCD_COLS);
    }

    lcd_drv_write_cmd(ST7567_CMD_SET_PAGE | (page & 0x0FU));
    lcd_drv_write_cmd(ST7567_CMD_SET_COL_HI | ((hw_col >> 4) & 0x0FU));
    lcd_drv_write_cmd(ST7567_CMD_SET_COL_LO | (hw_col & 0x0FU));
}

static void lcd_drv_init_seq(void)
{
    rt_thread_mdelay(10);
    lcd_reset();

    lcd_drv_write_cmd(ST7567_CMD_BIAS_1_9);
    lcd_drv_write_cmd(ST7567_CMD_SEG_NORMAL);
    lcd_drv_write_cmd(ST7567_CMD_COM_NORMAL);
    lcd_drv_write_cmd(ST7567_CMD_SET_START_LINE | 0x00U);
    lcd_drv_write_cmd(ST7567_CMD_REG_RATIO | ST7567_INIT_REG_RATIO);
    lcd_drv_write_cmd(ST7567_CMD_SET_EV);
    lcd_drv_write_cmd(ST7567_INIT_EV);
    lcd_drv_write_cmd(ST7567_CMD_SET_BOOSTER);
    lcd_drv_write_cmd(ST7567_CMD_BOOSTER_X5);
    lcd_drv_write_cmd(ST7567_CMD_POWER_ALL_ON);
    rt_thread_mdelay(120);
    lcd_drv_write_cmd(ST7567_CMD_INVERSE_OFF);
    lcd_drv_write_cmd(ST7567_CMD_ALL_PIXEL_OFF);
    lcd_clear();
    lcd_drv_write_cmd(ST7567_CMD_DISPLAY_ON);
}

int lcd_drv_init(void)
{
    lcd_drv_ctrl_pins_init();
    lcd_drv_spi_hw_init();

    lcd_csn_set(RT_FALSE);
    lcd_a0_set(RT_TRUE);
    lcd_backlight_off();

    lcd_drv_init_seq();
    return RT_EOK;
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

void lcd_spi_send_byte(uint8_t byte)
{
    lcd_drv_spi_write_byte(byte);
}

void lcd_clear(void)
{
    static const uint8_t zeros[LCD_COLS] = {0};
    uint8_t page;

    for (page = 0; page < LCD_PAGES; page++) {
        lcd_drv_set_page_col(page, 0);
        lcd_drv_write_data_buf(zeros, LCD_COLS);
    }
}

void lcd_fill_all(void)
{
    static uint8_t ones[LCD_COLS];
    uint8_t page;
    uint8_t i;

    for (i = 0; i < LCD_COLS; i++) {
        ones[i] = 0xFFU;
    }

    for (page = 0; page < LCD_PAGES; page++) {
        lcd_drv_set_page_col(page, 0);
        lcd_drv_write_data_buf(ones, LCD_COLS);
    }
}

void lcd_drv_write_page(uint8_t page, uint8_t col, const uint8_t *buf, uint16_t len)
{
    if ((buf == RT_NULL) || (page >= LCD_PAGES) || (col >= LCD_COLS) || (len == 0U)) {
        return;
    }

    lcd_drv_set_page_col(page, col);
    lcd_drv_write_data_buf(buf, len);
}
