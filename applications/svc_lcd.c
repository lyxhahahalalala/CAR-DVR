#include <rtthread.h>
#include <rtdevice.h>

#include "board.h"
#include "drv_spi.h"
#include "hpm_gpiom_drv.h"
#include "hpm_gpiom_soc_drv.h"
#include "hpm_gpio_drv.h"
#include "app_config.h"
#include "svc_lcd.h"

#define LCD_RSTB_GPIO_CTRL       HPM_GPIO0
#define LCD_RSTB_GPIO_INDEX      GPIO_DO_GPIOA
#define LCD_RSTB_GPIO_OE         GPIO_OE_GPIOA
#define LCD_RSTB_PIN             3

#define LCD_BACKLIGHT_GPIO_CTRL  HPM_GPIO0
#define LCD_BACKLIGHT_GPIO_INDEX GPIO_DO_GPIOC
#define LCD_BACKLIGHT_GPIO_OE    GPIO_OE_GPIOC
#define LCD_BACKLIGHT_PIN        16

#define LCD_CSN_GPIO_CTRL        HPM_GPIO0
#define LCD_CSN_GPIO_INDEX       GPIO_DO_GPIOA
#define LCD_CSN_GPIO_OE          GPIO_OE_GPIOA
#define LCD_CSN_PIN              28

#define LCD_A0_GPIO_CTRL         HPM_GPIO0
#define LCD_A0_GPIO_INDEX        GPIO_DO_GPIOA
#define LCD_A0_GPIO_OE           GPIO_OE_GPIOA
#define LCD_A0_PIN               29

#define LCD_CMD_SLEEP_OUT        0x11U
#define LCD_CMD_NORMAL_ON        0x13U
#define LCD_CMD_DISPLAY_ON       0x29U
#define LCD_CMD_COLUMN_ADDR      0x2AU
#define LCD_CMD_ROW_ADDR         0x2BU
#define LCD_CMD_MEMORY_WRITE     0x2CU
#define LCD_CMD_MADCTL           0x36U
#define LCD_CMD_PIXEL_FORMAT     0x3AU

#define LCD_FILL_CHUNK_PIXELS    64U
#define LCD_INIT_DATA_MAX_LEN    8U
#define LCD_COLOR_RGB565_BLACK   0x0000U
#define LCD_COLOR_RGB565_RED     0xF800U
#define LCD_COLOR_RGB565_GREEN   0x07E0U
#define LCD_COLOR_RGB565_BLUE    0x001FU
#define LCD_COLOR_RGB565_WHITE   0xFFFFU

typedef struct
{
    rt_uint8_t cmd;
    rt_uint8_t data[LCD_INIT_DATA_MAX_LEN];
    rt_uint8_t data_len;
    rt_uint16_t delay_ms;
} lcd_init_cmd_t;

static struct rt_spi_device *g_lcd_spi_dev;
static rt_bool_t g_lcd_panel_ready;
static rt_bool_t g_lcd_test_started;

static const lcd_init_cmd_t g_lcd_init_table[] = {
    { LCD_CMD_SLEEP_OUT,   { 0 },                0U,   120U },
    { LCD_CMD_MADCTL,      { 0x00U },            1U,     0U },
    { LCD_CMD_PIXEL_FORMAT,{ 0x55U },            1U,     0U },
    { LCD_CMD_COLUMN_ADDR, { 0x00U, 0x00U, 0x00U, (APP_LCD_WIDTH - 1U) }, 4U, 0U },
    { LCD_CMD_ROW_ADDR,    { 0x00U, 0x00U, (rt_uint8_t)(((APP_LCD_HEIGHT - 1U) >> 8) & 0xFFU), (rt_uint8_t)((APP_LCD_HEIGHT - 1U) & 0xFFU) }, 4U, 0U },
    { LCD_CMD_NORMAL_ON,   { 0 },                0U,    10U },
    { LCD_CMD_DISPLAY_ON,  { 0 },                0U,    20U },
};

static void svc_lcd_ctrl_pins_init(void)
{
    board_init_spi_pins_with_gpio_as_cs(HPM_SPI0);

    HPM_IOC->PAD[IOC_PAD_PA03].FUNC_CTL = IOC_PA03_FUNC_CTL_GPIO_A_03;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_RSTB_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_OE, LCD_RSTB_PIN);

    HPM_IOC->PAD[IOC_PAD_PC16].FUNC_CTL = IOC_PC16_FUNC_CTL_GPIO_C_16;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, LCD_BACKLIGHT_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_BACKLIGHT_GPIO_CTRL, LCD_BACKLIGHT_GPIO_OE, LCD_BACKLIGHT_PIN);

    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_CSN_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_CSN_GPIO_CTRL, LCD_CSN_GPIO_OE, LCD_CSN_PIN);

    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_A0_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_A0_GPIO_CTRL, LCD_A0_GPIO_OE, LCD_A0_PIN);
}

static void lcd_spi_cs_callback(uint32_t value)
{
    lcd_csn_set(value == SPI_CS_TAKE ? RT_TRUE : RT_FALSE);
}

static int lcd_spi_prepare(void)
{
    struct rt_spi_configuration cfg;
    rt_err_t result;
    rt_device_t device;

    device = rt_device_find(APP_LCD_SPI_DEV_NAME);
    if (device == RT_NULL)
    {
        result = rt_hw_spi_device_attach(APP_LCD_SPI_BUS_NAME, APP_LCD_SPI_DEV_NAME, lcd_spi_cs_callback);
        if (result != RT_EOK)
        {
            rt_kprintf("LCD: attach %s on %s failed, ret=%d\r\n", APP_LCD_SPI_DEV_NAME, APP_LCD_SPI_BUS_NAME, result);
            return -RT_ERROR;
        }

        device = rt_device_find(APP_LCD_SPI_DEV_NAME);
    }

    if (device == RT_NULL)
    {
        rt_kprintf("LCD: device %s not found after attach\r\n", APP_LCD_SPI_DEV_NAME);
        return -RT_ERROR;
    }

    g_lcd_spi_dev = (struct rt_spi_device *)device;

    cfg.data_width = 8;
    cfg.mode = RT_SPI_MASTER | RT_SPI_MODE_0 | RT_SPI_MSB;
    cfg.max_hz = APP_LCD_SPI_MAX_HZ;

    result = rt_spi_configure(g_lcd_spi_dev, &cfg);
    if (result != RT_EOK)
    {
        rt_kprintf("LCD: configure spi failed, ret=%d\r\n", result);
        g_lcd_spi_dev = RT_NULL;
        return -RT_ERROR;
    }

    return RT_EOK;
}

static int lcd_write_bytes(rt_bool_t is_data, const rt_uint8_t *buf, rt_size_t len)
{
    if ((g_lcd_spi_dev == RT_NULL) || (buf == RT_NULL) || (len == 0U))
    {
        return -RT_ERROR;
    }

    lcd_a0_set(is_data);
    return (rt_spi_send(g_lcd_spi_dev, buf, len) == len) ? RT_EOK : -RT_ERROR;
}

static int lcd_write_u16_be(rt_uint16_t value)
{
    rt_uint8_t buf[2];

    buf[0] = (rt_uint8_t)(value >> 8);
    buf[1] = (rt_uint8_t)(value & 0xFFU);
    return lcd_write_data(buf, sizeof(buf));
}

static int lcd_set_address_window(rt_uint16_t x0, rt_uint16_t y0, rt_uint16_t x1, rt_uint16_t y1)
{
    if ((lcd_write_cmd(LCD_CMD_COLUMN_ADDR) != RT_EOK) ||
        (lcd_write_u16_be(x0) != RT_EOK) ||
        (lcd_write_u16_be(x1) != RT_EOK))
    {
        return -RT_ERROR;
    }

    if ((lcd_write_cmd(LCD_CMD_ROW_ADDR) != RT_EOK) ||
        (lcd_write_u16_be(y0) != RT_EOK) ||
        (lcd_write_u16_be(y1) != RT_EOK))
    {
        return -RT_ERROR;
    }

    return lcd_write_cmd(LCD_CMD_MEMORY_WRITE);
}

static int lcd_run_init_table(void)
{
    rt_size_t i;

    for (i = 0; i < sizeof(g_lcd_init_table) / sizeof(g_lcd_init_table[0]); i++)
    {
        const lcd_init_cmd_t *item = &g_lcd_init_table[i];

        if (lcd_write_cmd(item->cmd) != RT_EOK)
        {
            return -RT_ERROR;
        }

        if ((item->data_len > 0U) && (lcd_write_data(item->data, item->data_len) != RT_EOK))
        {
            return -RT_ERROR;
        }

        if (item->delay_ms > 0U)
        {
            rt_thread_mdelay(item->delay_ms);
        }
    }

    return RT_EOK;
}

static int lcd_fill_color(rt_uint16_t color)
{
    rt_uint32_t i;
    rt_uint32_t total_pixels = APP_LCD_WIDTH * APP_LCD_HEIGHT;
    rt_uint8_t line_buf[LCD_FILL_CHUNK_PIXELS * 2U];

    for (i = 0; i < LCD_FILL_CHUNK_PIXELS; i++)
    {
        line_buf[i * 2U] = (rt_uint8_t)(color >> 8);
        line_buf[i * 2U + 1U] = (rt_uint8_t)(color & 0xFFU);
    }

    if (lcd_set_address_window(0U, 0U, APP_LCD_WIDTH - 1U, APP_LCD_HEIGHT - 1U) != RT_EOK)
    {
        return -RT_ERROR;
    }

    for (i = 0; i < total_pixels; i += LCD_FILL_CHUNK_PIXELS)
    {
        rt_uint32_t remain = total_pixels - i;
        rt_uint32_t chunk_pixels = (remain > LCD_FILL_CHUNK_PIXELS) ? LCD_FILL_CHUNK_PIXELS : remain;
        if (lcd_write_data(line_buf, chunk_pixels * 2U) != RT_EOK)
        {
            return -RT_ERROR;
        }
    }

    return RT_EOK;
}

static int lcd_panel_init(void)
{
    if (lcd_spi_prepare() != RT_EOK)
    {
        return -RT_ERROR;
    }

    lcd_backlight_off();
    lcd_reset();

    if (lcd_run_init_table() != RT_EOK)
    {
        rt_kprintf("LCD: init table transfer failed\r\n");
        return -RT_ERROR;
    }

    lcd_backlight_on();
    g_lcd_panel_ready = RT_TRUE;
    return RT_EOK;
}

void lcd_reset(void)
{
    gpio_write_pin(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_INDEX, LCD_RSTB_PIN, 0);
    rt_thread_mdelay(20);
    gpio_write_pin(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_INDEX, LCD_RSTB_PIN, 1);
    rt_thread_mdelay(120);
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

int lcd_write_cmd(rt_uint8_t cmd)
{
    return lcd_write_bytes(RT_FALSE, &cmd, 1U);
}

int lcd_write_data(const rt_uint8_t *data, rt_size_t len)
{
    return lcd_write_bytes(RT_TRUE, data, len);
}

static void svc_lcd_thread_entry(void *arg)
{
    static const rt_uint16_t test_colors[] = {
        LCD_COLOR_RGB565_RED,
        LCD_COLOR_RGB565_GREEN,
        LCD_COLOR_RGB565_BLUE,
        LCD_COLOR_RGB565_WHITE,
        LCD_COLOR_RGB565_BLACK,
    };
    static const char *test_names[] = {
        "red",
        "green",
        "blue",
        "white",
        "black",
    };
    rt_size_t color_index = 0U;

    RT_UNUSED(arg);

    if (lcd_panel_init() != RT_EOK)
    {
        rt_kprintf("LCD: panel init failed, keep retrying\r\n");
    }
    else
    {
        rt_kprintf("LCD: panel init ok, start color test\r\n");
    }

    while (1)
    {
        if (!g_lcd_panel_ready)
        {
            if (lcd_panel_init() == RT_EOK)
            {
                rt_kprintf("LCD: panel init ok, start color test\r\n");
            }
            rt_thread_mdelay(APP_LCD_TASK_PERIOD_MS);
            continue;
        }

        if ((lcd_fill_color(test_colors[color_index]) == RT_EOK) && !g_lcd_test_started)
        {
            g_lcd_test_started = RT_TRUE;
        }

        rt_kprintf("LCD: fill %s\r\n", test_names[color_index]);
        color_index = (color_index + 1U) % (sizeof(test_colors) / sizeof(test_colors[0]));
        rt_thread_mdelay(APP_LCD_COLOR_TEST_STEP_MS);
    }
}

int svc_lcd_init(void)
{
    svc_lcd_ctrl_pins_init();

    lcd_csn_set(RT_FALSE);
    lcd_a0_set(RT_TRUE);
    lcd_backlight_off();

    g_lcd_spi_dev = RT_NULL;
    g_lcd_panel_ready = RT_FALSE;
    g_lcd_test_started = RT_FALSE;

    return RT_EOK;
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
        rt_kprintf("lcd thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}
