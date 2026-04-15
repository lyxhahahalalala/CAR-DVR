#include <rtthread.h>

#include "board.h"
#include "hpm_gpiom_drv.h"
#include "hpm_gpiom_soc_drv.h"
#include "hpm_gpio_drv.h"
#include "hpm_spi_drv.h"
#include "app_config.h"
#include "svc_adc.h"
#include "svc_lcd.h"

/*
 * LCD 相关控制脚，按原理图整理如下：
 * PA03 -> LCD_RSTB      硬件复位，低有效
 * PC16 -> LCD_LED_A     背光阳极，高电平开启
 * PA28 -> LCD_CSN       SPI 片选，低有效（GPIO 控制）
 * PA29 -> LCD_A0        命令/数据选择，0=命令，1=数据
 * PA30 -> LCD_SCK       SPI 时钟（SPI0_SCLK）
 * PA31 -> LCD_SDA(MOSI) SPI 数据（SPI0_MOSI）
 *
 * LCD 驱动芯片：ST7567
 * 分辨率：132×64 点阵 STN
 * 接口：4 线 SPI（只写）
 * VDD：3.3V，背光 3.3V max 45mA
 */

/* ----- GPIO 宏定义 ----- */
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

/* ----- ST7567 命令字 ----- */
#define ST7567_CMD_DISPLAY_OFF      0xAE  /* 关闭显示 */
#define ST7567_CMD_DISPLAY_ON       0xAF  /* 打开显示 */
#define ST7567_CMD_SET_START_LINE   0x40  /* 设置起始行（|行号 0~63）*/
#define ST7567_CMD_SET_PAGE         0xB0  /* 设置页地址（|页号 0~8）*/
#define ST7567_CMD_SET_COL_HI       0x10  /* 设置列地址高 4 位 */
#define ST7567_CMD_SET_COL_LO       0x00  /* 设置列地址低 4 位 */
#define ST7567_CMD_SEG_NORMAL       0xA0  /* SEG 正向扫描（MX=0）*/
#define ST7567_CMD_SEG_REVERSE      0xA1  /* SEG 反向扫描（MX=1）*/
#define ST7567_CMD_INVERSE_OFF      0xA6  /* 正常显示（不反色）*/
#define ST7567_CMD_INVERSE_ON       0xA7  /* 反色显示 */
#define ST7567_CMD_ALL_PIXEL_OFF    0xA4  /* 正常显示（读 DDRAM）*/
#define ST7567_CMD_ALL_PIXEL_ON     0xA5  /* 全亮（不读 DDRAM）*/
#define ST7567_CMD_BIAS_1_9         0xA2  /* LCD Bias = 1/9（1/65 Duty 选此）*/
#define ST7567_CMD_BIAS_1_7         0xA3  /* LCD Bias = 1/7 */
#define ST7567_CMD_COM_NORMAL       0xC0  /* COM 正向扫描（MY=0）*/
#define ST7567_CMD_COM_REVERSE      0xC8  /* COM 反向扫描（MY=1）*/
#define ST7567_CMD_POWER_CTRL       0x28  /* 电源控制（|VB|VR|VF 3bit）*/
#define ST7567_CMD_POWER_ALL_ON     0x2F  /* VB=VR=VF=1，全部开启 */
#define ST7567_CMD_REG_RATIO        0x20  /* 内部电阻比（|RR2:RR0 3bit）*/
#define ST7567_CMD_SET_EV           0x81  /* 设置对比度（双字节指令）*/
#define ST7567_CMD_SET_BOOSTER      0xF8  /* 设置升压级数（双字节指令）*/
#define ST7567_CMD_BOOSTER_X5       0x01  /* 升压 ×5（3.3V 供电选此）*/
#define ST7567_CMD_SOFTWARE_RESET   0xE2  /* 软件复位 */
#define ST7567_CMD_NOP              0xE3  /* 空操作 */

/*
 * 对比度初始值（EV）：0x00~0x3F。
 * 数值偏大画面偏深，偏小画面偏浅。
 * 屏幕全白无内容时说明 V0 不够，需提高 EV 或 RR。
 * 调试顺序：0x1C → 0x28 → 0x30 → 0x38，观察是否出现黑色像素。
 */
#define ST7567_INIT_EV              0x28
/*
 * 内部电阻比（RR）: 0x00(3.0) ~ 0x07(6.5)，步长 0.5。
 * 公式：V0 = RR × [(99 + EV) / 162] × 2.1
 * 以 RR=6.5, EV=0x38=56: V0 = 6.5 × [(99+56)/162] × 2.1 ≈ 13.1V
 * 若画面过深（全黑），改回 0x06 或降低 EV。
 */
#define ST7567_INIT_REG_RATIO       0x07  /* 对应命令 0x20|0x07 = 0x27，RR=6.5 */

/* LCD 有效列宽 */
#define LCD_COLS    132
/* LCD 有效页数（每页 8 行，共 64 行 = 8 页；icon 行作为第 8 页不使用）*/
#define LCD_PAGES   8

/*
 * 按键分压测试阈值，按原理图标称电压的相邻中点划分：
 * S1 = 0V
 * S2 = 0.807V
 * S3 = 2.142V
 * S4 = 2.773V
 * 无按键 = 3.3V
 */
#define LCD_KEY_S1_MAX_MV           400U
#define LCD_KEY_S2_MAX_MV           1475U
#define LCD_KEY_S3_MAX_MV           2300U
#define LCD_KEY_S4_MAX_MV           2900U

static uint8_t g_lcd_fb[LCD_PAGES][LCD_COLS];

/* ----- 软件 SPI 初始化 ----- */
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

/* ----- GPIO 初始化 ----- */
static void svc_lcd_ctrl_pins_init(void)
{
    /* LCD_RSTB（PA03）*/
    HPM_IOC->PAD[IOC_PAD_PA03].FUNC_CTL = IOC_PA03_FUNC_CTL_GPIO_A_03;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_RSTB_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_OE, LCD_RSTB_PIN);

    /* LCD_LED_A（PC16）*/
    HPM_IOC->PAD[IOC_PAD_PC16].FUNC_CTL = IOC_PC16_FUNC_CTL_GPIO_C_16;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, LCD_BACKLIGHT_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_BACKLIGHT_GPIO_CTRL, LCD_BACKLIGHT_GPIO_OE, LCD_BACKLIGHT_PIN);

    /* LCD_CSN（PA28）*/
    HPM_IOC->PAD[IOC_PAD_PA28].FUNC_CTL = IOC_PA28_FUNC_CTL_GPIO_A_28;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_CSN_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_CSN_GPIO_CTRL, LCD_CSN_GPIO_OE, LCD_CSN_PIN);

    /* LCD_A0（PA29）*/
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

/* ----- GPIO 控制函数 ----- */
void lcd_reset(void)
{
    /* RSTB 低脉冲：低 ≥ 1μs（3.3V），实际给 20ms 裕量大 */
    gpio_write_pin(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_INDEX, LCD_RSTB_PIN, 0);
    rt_thread_mdelay(20);
    gpio_write_pin(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_INDEX, LCD_RSTB_PIN, 1);
    /* 复位完成后等待内部初始化 */
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
    /* is_data=RT_TRUE  → A0=1 → 数据模式
     * is_data=RT_FALSE → A0=0 → 命令模式 */
    gpio_write_pin(LCD_A0_GPIO_CTRL, LCD_A0_GPIO_INDEX, LCD_A0_PIN, is_data ? 1 : 0);
}

void lcd_csn_set(rt_bool_t active)
{
    /* 片选低有效：active=RT_TRUE 拉低，active=RT_FALSE 释放为高 */
    gpio_write_pin(LCD_CSN_GPIO_CTRL, LCD_CSN_GPIO_INDEX, LCD_CSN_PIN, active ? 0 : 1);
}

/* ----- SPI 字节写 ----- */
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

/* ----- 命令 / 数据写 ----- */
static void lcd_write_cmd(uint8_t cmd)
{
    lcd_csn_set(RT_TRUE);
    lcd_a0_set(RT_FALSE);   /* A0=0 → 命令 */
    lcd_spi_write_byte(cmd);
    lcd_csn_set(RT_FALSE);
}

static void lcd_write_data_buf(const uint8_t *buf, uint16_t len)
{
    lcd_csn_set(RT_TRUE);
    lcd_a0_set(RT_TRUE);    /* A0=1 → 数据 */

    while (len-- > 0U) {
        lcd_spi_write_byte(*buf++);
    }

    lcd_csn_set(RT_FALSE);
}

static rt_uint32_t lcd_key_raw_to_pin_mv(rt_uint32_t raw)
{
    rt_uint64_t pin_mv;

    pin_mv = (rt_uint64_t)raw * APP_ADC_VREF_MV;
    pin_mv /= APP_ADC_FULL_SCALE;

    return (rt_uint32_t)pin_mv;
}

static int lcd_key_decode(rt_uint32_t raw_key)
{
    rt_uint32_t pin_mv;

    pin_mv = lcd_key_raw_to_pin_mv(raw_key);

    if (pin_mv <= LCD_KEY_S1_MAX_MV) {
        return 1;
    }
    if (pin_mv <= LCD_KEY_S2_MAX_MV) {
        return 2;
    }
    if (pin_mv <= LCD_KEY_S3_MAX_MV) {
        return 3;
    }
    if (pin_mv <= LCD_KEY_S4_MAX_MV) {
        return 4;
    }

    return 0;
}

/* ----- 定位到指定页和列 ----- */
static void lcd_set_page_col(uint8_t page, uint8_t col)
{
    lcd_write_cmd(ST7567_CMD_SET_PAGE | (page & 0x0F));
    lcd_write_cmd(ST7567_CMD_SET_COL_HI | ((col >> 4) & 0x0F));
    lcd_write_cmd(ST7567_CMD_SET_COL_LO | (col & 0x0F));
}

/* ----- 清屏（DDRAM 全写 0x00）----- */
void lcd_clear(void)
{
    static const uint8_t zeros[LCD_COLS] = {0};
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        lcd_set_page_col(page, 0);
        lcd_write_data_buf(zeros, LCD_COLS);
    }
}

/* ----- 全亮测试（DDRAM 全写 0xFF）----- */
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

    if ((x >= LCD_COLS) || (y >= (LCD_PAGES * 8U))) {
        return;
    }

    page = y / 8U;
    bit = y % 8U;

    if (on) {
        g_lcd_fb[page][x] |= (uint8_t)(1U << bit);
    } else {
        g_lcd_fb[page][x] &= (uint8_t)~(1U << bit);
    }
}

static void lcd_fb_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t xx;
    uint8_t yy;

    for (yy = y; yy < (uint8_t)(y + h); yy++) {
        for (xx = x; xx < (uint8_t)(x + w); xx++) {
            lcd_fb_set_pixel(xx, yy, RT_TRUE);
        }
    }
}

static void lcd_fb_flush(void)
{
    uint8_t page;

    for (page = 0; page < LCD_PAGES; page++) {
        lcd_set_page_col(page, 0);
        lcd_write_data_buf(g_lcd_fb[page], LCD_COLS);
    }
}

static void lcd_fb_draw_digit_7seg(uint8_t digit)
{
    rt_bool_t seg_a = RT_FALSE;
    rt_bool_t seg_b = RT_FALSE;
    rt_bool_t seg_c = RT_FALSE;
    rt_bool_t seg_d = RT_FALSE;
    rt_bool_t seg_e = RT_FALSE;
    rt_bool_t seg_f = RT_FALSE;
    rt_bool_t seg_g = RT_FALSE;

    switch (digit) {
    case 1:
        seg_b = RT_TRUE;
        seg_c = RT_TRUE;
        break;
    case 2:
        seg_a = RT_TRUE;
        seg_b = RT_TRUE;
        seg_d = RT_TRUE;
        seg_e = RT_TRUE;
        seg_g = RT_TRUE;
        break;
    case 3:
        seg_a = RT_TRUE;
        seg_b = RT_TRUE;
        seg_c = RT_TRUE;
        seg_d = RT_TRUE;
        seg_g = RT_TRUE;
        break;
    case 4:
        seg_b = RT_TRUE;
        seg_c = RT_TRUE;
        seg_f = RT_TRUE;
        seg_g = RT_TRUE;
        break;
    default:
        return;
    }

    if (seg_a) {
        lcd_fb_fill_rect(44, 6, 38, 6);
    }
    if (seg_b) {
        lcd_fb_fill_rect(82, 12, 6, 18);
    }
    if (seg_c) {
        lcd_fb_fill_rect(82, 36, 6, 18);
    }
    if (seg_d) {
        lcd_fb_fill_rect(44, 54, 38, 6);
    }
    if (seg_e) {
        lcd_fb_fill_rect(38, 36, 6, 18);
    }
    if (seg_f) {
        lcd_fb_fill_rect(38, 12, 6, 18);
    }
    if (seg_g) {
        lcd_fb_fill_rect(44, 30, 38, 6);
    }
}

/* ----- ST7567 初始化序列 ----- */
static void st7567_init_seq(void)
{
    /* 上电后等待电源稳定，然后硬件复位 */
    rt_thread_mdelay(10);
    lcd_reset();

    /* 1. Bias = 1/9（对应 1/65 Duty）*/
    lcd_write_cmd(ST7567_CMD_BIAS_1_9);

    /* 2. SEG 方向 */
    lcd_write_cmd(ST7567_CMD_SEG_NORMAL);

    /* 3. COM 方向 */
    lcd_write_cmd(ST7567_CMD_COM_REVERSE);

    /* 4. 起始行 = 0 */
    lcd_write_cmd(ST7567_CMD_SET_START_LINE | 0x00);

    /* 5. 内部电阻比 */
    lcd_write_cmd(ST7567_CMD_REG_RATIO | ST7567_INIT_REG_RATIO);

    /* 6. 电子音量（对比度） */
    lcd_write_cmd(ST7567_CMD_SET_EV);
    lcd_write_cmd(ST7567_INIT_EV);

    /* 7. 升压比 */
    lcd_write_cmd(ST7567_CMD_SET_BOOSTER);
    lcd_write_cmd(ST7567_CMD_BOOSTER_X5);

    /* 8. 电源控制：Booster + Regulator + Follower 全开 */
    lcd_write_cmd(ST7567_CMD_POWER_ALL_ON);

    rt_thread_mdelay(120);

    /* 9. 正常显示，不反色 */
    lcd_write_cmd(ST7567_CMD_INVERSE_OFF);

    /* 10. 恢复正常显示模式（读 DDRAM） */
    lcd_write_cmd(ST7567_CMD_ALL_PIXEL_OFF);

    /* 11. 清 DDRAM */
    lcd_clear();

    /* 12. 打开显示 */
    lcd_write_cmd(ST7567_CMD_DISPLAY_ON);
}

/* ----- svc_lcd_init：外部调用入口 ----- */
int svc_lcd_init(void)
{
    /* 1. GPIO / SPI 引脚配置 */
    svc_lcd_ctrl_pins_init();

    /* 2. SPI 引脚初始化 */
    svc_lcd_spi_hw_init();

    /* 3. 初始电平：CS 无效（高）、A0=数据、背光关 */
    lcd_csn_set(RT_FALSE);
    lcd_a0_set(RT_TRUE);
    lcd_backlight_off();

    /* 4. ST7567 初始化序列 */
    st7567_init_seq();

    APP_NON_CAN_LOG("LCD ST7567 init done\r\n");
    return RT_EOK;
}

/* ----- LCD 任务线程 ----- */
static void svc_lcd_thread_entry(void *arg)
{
    const app_adc_snapshot_t *adc_snapshot;
    int key_value;
    int last_key_value = -1;

    RT_UNUSED(arg);

    lcd_backlight_on();
    lcd_fb_clear();
    lcd_fb_flush();

    /* 等 ADC 线程完成首次采样，避免上电初始 0 被误判成 S1 */
    rt_thread_mdelay(APP_ADC_STARTUP_DELAY_MS + 200U);

    while (1)
    {
        adc_snapshot = svc_adc_get_snapshot();
        key_value = lcd_key_decode(adc_snapshot->raw_key);

        if (key_value != last_key_value) {
            lcd_fb_clear();
            if (key_value != 0) {
                lcd_fb_draw_digit_7seg((uint8_t)key_value);
            }
            lcd_fb_flush();

            APP_NON_CAN_LOG("LCD KEY: raw=%lu pin=%lumV show=%d\r\n",
                            adc_snapshot->raw_key,
                            lcd_key_raw_to_pin_mv(adc_snapshot->raw_key),
                            key_value);
            last_key_value = key_value;
        }

        rt_thread_mdelay(100);
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

