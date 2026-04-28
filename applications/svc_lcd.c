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
#include "svc_vehicle_io.h"
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


/* Active LCD UI fonts */
#define LCD_FONT_ASCII_SMALL    u8g2_font_6x10_tf
#define LCD_FONT_CN_12          u8g2_font_wqy12_t_gb2312

static uint8_t g_lcd_fb[LCD_PAGES][LCD_COLS];



typedef struct
{
    uint16_t speed_kmh_x10;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t drive_hour;
    uint8_t drive_minute;
    uint8_t drive_second;
    uint8_t top_status_value;//卫星数据是否有效
    char card_id[20];
} lcd_home_ui_data_t;

static lcd_home_ui_data_t g_lcd_home_ui = {
    .speed_kmh_x10 = 0U,
    .hour = 9U,
    .minute = 16U,
    .second = 45U,
    .drive_hour = 0U,
    .drive_minute = 0U,
    .drive_second = 0U,
    .top_status_value = 0U,
    .card_id = "000000000000000000"
};

char speed_str[16];
char time_str[16];
char drive_time_str[16];

typedef enum
{
    LCD_PAGE_HOME = 0,
    LCD_PAGE_MAIN_MENU,
    LCD_PAGE_DRIVE_RECORD_MENU,
    LCD_PAGE_DEVICE_STATUS_MENU,
    LCD_PAGE_SYSTEM_SETTING_MENU,
    LCD_PAGE_DRIVE_RECORD_FATIGUE,
    LCD_PAGE_DRIVE_RECORD_LOCATION,
    LCD_PAGE_DRIVE_RECORD_MILEAGE,
    LCD_PAGE_DRIVE_RECORD_DRIVER_INFO,
    LCD_PAGE_DRIVE_RECORD_VEHICLE_INFO,
    LCD_PAGE_DRIVE_RECORD_LOAD_STATUS,
    LCD_PAGE_DRIVE_RECORD_LOAD_STATUS_OK,
    LCD_PAGE_DRIVE_RECORD_EXPORT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_BROADCAST,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EVENT_REPORT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EWAYBILL,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_DISPATCH,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_NORMAL,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_FAULT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_EMERGENCY,
    LCD_PAGE_DRIVE_RECORD_VIN,
    LCD_PAGE_DEVICE_STATUS_VERSION,
    LCD_PAGE_DEVICE_STATUS_GFRS,
    LCD_PAGE_DEVICE_STATUS_SELF_TEST,
    LCD_PAGE_DEVICE_STATUS_LOCATION_MODULE,
    LCD_PAGE_DEVICE_STATUS_VEHICLE,
    LCD_PAGE_DEVICE_STATUS_STORAGE,
    LCD_PAGE_DEVICE_STATUS_AV,
    LCD_PAGE_DEVICE_STATUS_SPEED,
    LCD_PAGE_DEVICE_STATUS_DRIVER_MONITOR,
    LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME,
    LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM,
    LCD_PAGE_SYSTEM_SETTING_INIT_MILEAGE,
    LCD_PAGE_SYSTEM_SETTING_REGISTER,
    LCD_PAGE_SYSTEM_SETTING_UNREGISTER,
    LCD_PAGE_SYSTEM_SETTING_REC_VER,
    LCD_PAGE_SYSTEM_SETTING_COMPONENT_VER,
    LCD_PAGE_MAX
} lcd_page_id_t;

typedef enum
{
    LCD_PAGE_KIND_VIEW = 0,
    LCD_PAGE_KIND_LIST,
    LCD_PAGE_KIND_ACTION_RESULT
} lcd_page_kind_t;

typedef struct
{
    lcd_page_id_t page_id;
    lcd_page_id_t parent_id;
    lcd_page_kind_t kind;
    const lcd_page_id_t *children;
    uint8_t child_count;
    uint8_t select_count;
    void (*render)(void);
    void (*on_confirm)(void);
    uint32_t auto_return_ms;
    lcd_page_id_t auto_return_target;
} lcd_page_node_t;

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
    g_menu_item_text_3
};

static const uint8_t g_menu_item_text_counts_u8g2[] = {4, 4, 4};
static const uint16_t g_drive_sub_item_1[] = {
    0x75B2, 0x52B3, 0x9A7E, 0x9A76 /* 疲劳驾驶 */
};

static const uint16_t g_drive_sub_item_2[] = {
    0x4F4D, 0x7F6E, 0x5B9A, 0x4F4D /* 位置定位 */
};

static const uint16_t g_drive_sub_item_3[] = {
    0x884C, 0x9A76, 0x8BB0, 0x5F55 /* 行驶记录 */
};

static const uint16_t g_drive_sub_item_4[] = {
    0x9A7E, 0x9A76, 0x5458, 0x4FE1, 0x606F /* 驾驶员信息 */
};

static const uint16_t g_drive_sub_item_5[] = {
    0x8F66, 0x8F86, 0x4FE1, 0x606F /* 车辆信息 */
};

static const uint16_t g_drive_sub_item_6[] = {
    0x8F66, 0x8F86, 0x8F7D, 0x91CD, 0x4FE1, 0x606F /* 车辆载重信息 */
};

static const uint16_t g_drive_sub_item_7[] = {
    0x6570, 0x636E, 0x5BFC, 0x51FA /* 数据导出 */
};

static const uint16_t g_drive_sub_item_8[] = {
    0x4FE1, 0x606F, 0x4E2D, 0x5FC3 /* 信息中心 */
};

static const uint16_t g_drive_sub_item_9[] = {
        0x901A, 0x8BDD, 0x8BB0, 0x5F55 /* 通话记录 */
};

static const uint16_t *const g_drive_sub_item_texts_u8g2[] = {
    g_drive_sub_item_1,
    g_drive_sub_item_2,
    g_drive_sub_item_3,
    g_drive_sub_item_4,
    g_drive_sub_item_5,
    g_drive_sub_item_6,
    g_drive_sub_item_7,
    g_drive_sub_item_8,
    g_drive_sub_item_9
};

static const uint8_t g_drive_sub_item_counts_u8g2[] = {
    4, 4, 4, 5, 4, 6, 4, 4, 4
};

static const uint16_t g_device_sub_item_1[] = {0x7248, 0x672C, 0x53F7, 0x67E5, 0x8BE2};
static const uint16_t g_device_sub_item_2[] = {0x0047, 0x0046, 0x0052, 0x0053, 0x4FE1, 0x606F};
static const uint16_t g_device_sub_item_3[] = {0x72B6, 0x6001, 0x81EA, 0x68C0};
static const uint16_t g_device_sub_item_4[] = {0x5B9A, 0x4F4D, 0x6A21, 0x5757, 0x72B6, 0x6001};
static const uint16_t g_device_sub_item_5[] = {0x6574, 0x8F66, 0x72B6, 0x6001};
static const uint16_t g_device_sub_item_6[] = {0x5B58, 0x50A8, 0x5668, 0x68C0, 0x6D4B};
static const uint16_t g_device_sub_item_7[] = {0x5F55, 0x97F3, 0x5F55, 0x50CF, 0x68C0, 0x6D4B};
static const uint16_t g_device_sub_item_8[] = {0x901F, 0x5EA6, 0x67E5, 0x8BE2};
static const uint16_t g_device_sub_item_9[] = {0x9A7E, 0x9A76, 0x5458, 0x72B6, 0x6001, 0x76D1, 0x63A7};

static const uint16_t g_info_center_item_1[] = {
    0x6587, 0x672C, 0x4FE1, 0x606F /* 文本信息 */
};

static const uint16_t g_info_center_item_2[] = {
    0x4FE1, 0x606F, 0x70B9, 0x64AD /* 信息点播 */
};

static const uint16_t g_info_center_item_3[] = {
    0x4E8B, 0x4EF6, 0x4E0A, 0x62A5 /* 事件上报 */
};

static const uint16_t g_info_center_item_4[] = {
    0x7535, 0x5B50, 0x8FD0, 0x5355 /* 电子运单 */
};

static const uint16_t *const g_info_center_item_texts_u8g2[] = {
    g_info_center_item_1,
    g_info_center_item_2,
    g_info_center_item_3,
    g_info_center_item_4
};

static const uint8_t g_info_center_item_counts_u8g2[] = {
    4, 4, 4, 4
};

static const uint16_t g_info_text_item_1[] = {
    0x8C03, 0x5EA6, 0x4FE1, 0x606F /* 调度信息 */
};

static const uint16_t g_info_text_item_2[] = {
    0x666E, 0x901A, 0x4FE1, 0x606F /* 普通信息 */
};

static const uint16_t g_info_text_item_3[] = {
    0x6545, 0x969C, 0x4FE1, 0x606F /* 故障信息 */
};

static const uint16_t g_info_text_item_4[] = {
    0x7D27, 0x6025, 0x4FE1, 0x606F /* 紧急信息 */
};

static const uint16_t *const g_info_text_item_texts_u8g2[] = {
    g_info_text_item_1,
    g_info_text_item_2,
    g_info_text_item_3,
    g_info_text_item_4
};

static const uint8_t g_info_text_item_counts_u8g2[] = {
    4, 4, 4, 4
};


static const uint16_t *const g_device_sub_item_texts_u8g2[] = {
    g_device_sub_item_1, g_device_sub_item_2, g_device_sub_item_3,
    g_device_sub_item_4, g_device_sub_item_5, g_device_sub_item_6,
    g_device_sub_item_7, g_device_sub_item_8, g_device_sub_item_9
};

static const uint8_t g_device_sub_item_counts_u8g2[] = {5, 6, 4, 6, 4, 5, 6, 4, 7};

static const uint16_t g_system_sub_item_1[] = {0x6309, 0x952E, 0x97F3, 0x91CF, 0x8BBE, 0x7F6E};
static const uint16_t g_system_sub_item_2[] = {0x5C4F, 0x5E55, 0x4EAE, 0x5EA6, 0x8BBE, 0x7F6E};
static const uint16_t g_system_sub_item_3[] = {0x8F66, 0x8F86, 0x4FE1, 0x606F, 0x8BBE, 0x7F6E};
static const uint16_t g_system_sub_item_4[] = {0x4E3B, 0x673A, 0x53C2, 0x6570, 0x8BBE, 0x7F6E};
static const uint16_t g_system_sub_item_5[] = {0x521D, 0x59CB, 0x91CC, 0x7A0B};
static const uint16_t g_system_sub_item_6[] = {0x4E3B, 0x673A, 0x6CE8, 0x518C};
static const uint16_t g_system_sub_item_7[] = {0x4E3B, 0x673A, 0x6CE8, 0x9500};
static const uint16_t g_system_sub_item_8[] = {0x8BB0, 0x5F55, 0x4EEA, 0x8F6F, 0x4EF6, 0x7248, 0x672C};
static const uint16_t g_system_sub_item_9[] = {0x5143, 0x5668, 0x4EF6, 0x8F6F, 0x4EF6, 0x7248, 0x672C};

static const uint16_t *const g_system_sub_item_texts_u8g2[] = {
    g_system_sub_item_1, g_system_sub_item_2, g_system_sub_item_3,
    g_system_sub_item_4, g_system_sub_item_5, g_system_sub_item_6,
    g_system_sub_item_7, g_system_sub_item_8, g_system_sub_item_9
};

static const uint8_t g_system_sub_item_counts_u8g2[] = {6, 6, 6, 6, 4, 4, 4, 7, 7};



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
static lcd_page_id_t g_lcd_current_page_id = LCD_PAGE_HOME;
static uint8_t g_lcd_page_selected[LCD_PAGE_MAX];
static rt_tick_t g_lcd_page_enter_tick = 0U;
static rt_bool_t g_lcd_need_redraw = RT_TRUE;
static rt_tick_t g_lcd_vehicle_status_check_tick = 0U;
static uint16_t g_lcd_vehicle_status_last_bits = 0xFFFFU;
static uint16_t g_lcd_overtime_drive_count = 0U;
static uint32_t g_lcd_total_mileage_km = 0U;
static uint16_t g_lcd_total_mileage_rem_m = 0U;
static uint8_t g_lcd_load_status_index = 0U;   /* 0=空载 1=半载 2=满载 */
static rt_tick_t g_lcd_load_status_ok_tick = 0U;




static uint8_t lcd_page_get_depth(lcd_page_id_t page_id);
static rt_bool_t lcd_get_list_page_resources(lcd_page_id_t page_id,
                                             const uint16_t *const **item_texts,
                                             const uint8_t **item_counts,
                                             uint8_t *item_count,
                                             const uint16_t **title_text,
                                             uint8_t *title_count);
static void lcd_render_home_ui(void);
static void lcd_render_menu_ui(void);
static void lcd_render_drive_record_submenu_ui(void);
static void lcd_render_submenu_ui(void);
static void lcd_render_fatigue_drive_record_ui(void);
static void lcd_render_drive_mileage_ui(void);
static void lcd_render_driver_info_ui(void);
static void lcd_render_vehicle_info_ui(void);
static void lcd_render_vehicle_status_ui(void);
static void lcd_render_vehicle_load_status_ui(void);
static void lcd_render_vehicle_load_status_ok_ui(void);
static void lcd_render_location_status_ui(void);
static void lcd_render_device_version_ui(void);
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

static void lcd_u8g2_draw_submenu_item(u8g2_t *u8g2,
                                       uint8_t index,
                                       uint8_t baseline_y,
                                       rt_bool_t selected)
{
    char prefix[4];
    const uint16_t *const *submenu_texts = RT_NULL;
    const uint8_t *submenu_counts = RT_NULL;
    uint8_t submenu_item_count = 0U;
    const uint16_t *title_text = RT_NULL;
    uint8_t title_count = 0U;

    if (lcd_get_list_page_resources(g_lcd_current_page_id,
                                    &submenu_texts,
                                    &submenu_counts,
                                    &submenu_item_count,
                                    &title_text,
                                    &title_count) != RT_TRUE) {
        u8g2_SetDrawColor(u8g2, 1);
        return;
    }

    (void)title_text;
    (void)title_count;

    if ((submenu_texts == RT_NULL) || (submenu_counts == RT_NULL) || (index >= submenu_item_count)) {
        u8g2_SetDrawColor(u8g2, 1);
        return;
    }

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
                              submenu_texts[index],
                              submenu_counts[index]);

    u8g2_SetDrawColor(u8g2, 1);
}





//菜单
static void lcd_render_menu_ui(void)
{
    u8g2_t *u8g2;
    uint8_t selected_index;
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

    selected_index = g_lcd_page_selected[LCD_PAGE_MAIN_MENU];

    if (selected_index < 3U) {
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
                                    (item_index == selected_index) ? RT_TRUE : RT_FALSE);
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
                                    (item_index == selected_index) ? RT_TRUE : RT_FALSE);
        }
    }

    u8g2_port_flush_buffer();
}

static void lcd_render_drive_record_submenu_ui(void)
{
    u8g2_t *u8g2;
    uint8_t selected_index;
    const uint16_t *title_text = RT_NULL;
    const uint16_t *const *submenu_texts = RT_NULL;
    const uint8_t *submenu_counts = RT_NULL;
    uint8_t submenu_item_count = 0U;
    uint8_t title_count = 0U;
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

    if (lcd_get_list_page_resources(g_lcd_current_page_id,
                                    &submenu_texts,
                                    &submenu_counts,
                                    &submenu_item_count,
                                    &title_text,
                                    &title_count) != RT_TRUE) {
        lcd_render_submenu_ui();
        return;
    }

    selected_index = g_lcd_page_selected[g_lcd_current_page_id];
    (void)submenu_texts;
    (void)submenu_counts;
    (void)submenu_item_count;

    if (selected_index < 3U) {
        page_start = 0U;
        row_count = (submenu_item_count < 3U) ? submenu_item_count : 3U;

        row_y[0] = 28U;
        row_y[1] = 42U;
        row_y[2] = 56U;

        /* 标题仍显示“行驶记录” */
        u8g2_SetFont(u8g2, LCD_FONT_CN_12);
        lcd_u8g2_draw_unicode_seq(u8g2, 2, 12, title_text, title_count);

        for (row = 0; row < row_count; row++) {
            uint8_t item_index = (uint8_t)(page_start + row);
            lcd_u8g2_draw_submenu_item(u8g2,
                                       item_index,
                                       row_y[row],
                                       (item_index == selected_index) ? RT_TRUE : RT_FALSE);
        }
    } else if ((selected_index < 7U) && (submenu_item_count > 3U)) {
        page_start = 3U;
        row_count = (uint8_t)(submenu_item_count - 3U);
        if (row_count > 4U) {
            row_count = 4U;
        }

        row_y[0] = 14U;
        row_y[1] = 28U;
        row_y[2] = 42U;
        row_y[3] = 56U;

        for (row = 0; row < row_count; row++) {
            uint8_t item_index = (uint8_t)(page_start + row);
            lcd_u8g2_draw_submenu_item(u8g2,
                                       item_index,
                                       row_y[row],
                                       (item_index == selected_index) ? RT_TRUE : RT_FALSE);
        }
    } else if (submenu_item_count > 7U) {
        page_start = 7U;
        row_count = (uint8_t)(submenu_item_count - 7U);
        if (row_count > 2U) {
            row_count = 2U;
        }

        row_y[0] = 28U;
        row_y[1] = 42U;

        for (row = 0; row < row_count; row++) {
            uint8_t item_index = (uint8_t)(page_start + row);
            lcd_u8g2_draw_submenu_item(u8g2,
                                       item_index,
                                       row_y[row],
                                       (item_index == selected_index) ? RT_TRUE : RT_FALSE);
        }
    }

    u8g2_port_flush_buffer();
}

static void lcd_render_submenu_ui(void)
{
    u8g2_t *u8g2;
    char level_str[12];

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    rt_snprintf(level_str, sizeof(level_str), "LEVEL %u",
                (unsigned int)lcd_page_get_depth(g_lcd_current_page_id));

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 18, 24, "SUBMENU");
    u8g2_DrawStr(u8g2, 18, 40, level_str);

    u8g2_port_flush_buffer();
}

static void lcd_render_fatigue_drive_record_ui(void)
{
    u8g2_t *u8g2;
    char count_str[12];
    static const uint16_t g_title_text[] = {
        0x8D85, 0x65F6, 0x9A7E, 0x9A76, 0x8BB0, 0x5F55 /* 超时驾驶记录 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 16, g_title_text, 6);
    u8g2_DrawStr(u8g2, 74, 16, ":");

    rt_snprintf(count_str, sizeof(count_str), "%u",
                (unsigned int)g_lcd_overtime_drive_count);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 86, 16, count_str);

    u8g2_port_flush_buffer();
}

static void lcd_render_drive_mileage_ui(void)
{
    u8g2_t *u8g2;
    char mileage_str[24];
    static const uint16_t g_title_text[] = {
        0x7D2F, 0x8BA1, 0x884C, 0x9A76, 0x91CC, 0x7A0B /* 累计行驶里程 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 16, g_title_text, 6);

    rt_snprintf(mileage_str, sizeof(mileage_str), "%lu.%03u km",
                (unsigned long)g_lcd_total_mileage_km,
                (unsigned int)g_lcd_total_mileage_rem_m);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 2, 38, mileage_str);

    u8g2_port_flush_buffer();
}

static void lcd_render_driver_info_ui(void)
{
    u8g2_t *u8g2;
    rt_bool_t all_zero = RT_TRUE;
    uint8_t i;
    static const uint16_t g_title_text[] = {
        0x9A7E, 0x9A76, 0x5458, 0x8BC1, 0x53F7 /* 驾驶员证号 */
    };
    static const uint16_t g_login_text[] = {
        0x5DF2, 0x767B, 0x5F55 /* 已登录 */
    };
    static const uint16_t g_logout_text[] = {
        0x672A, 0x767B, 0x5F55 /* 未登录 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    for (i = 0U; i < 18U; i++) {
        if (g_lcd_home_ui.card_id[i] != '0') {
            all_zero = RT_FALSE;
            break;
        }
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 14, g_title_text, 5);
    u8g2_DrawStr(u8g2, 64, 14, ":");

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 2, 32, g_lcd_home_ui.card_id);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    if (all_zero == RT_TRUE) {
        lcd_u8g2_draw_unicode_seq(u8g2, 2, 50, g_logout_text, 3);
    } else {
        lcd_u8g2_draw_unicode_seq(u8g2, 2, 50, g_login_text, 3);
    }

    u8g2_port_flush_buffer();
}

static void lcd_render_info_center_dispatch_ui(void)
{
    u8g2_t *u8g2;
    static const uint16_t g_dispatch_text[] = {
        0x8C03, 0x5EA6, 0x4FE1, 0x606F /* 调度信息 */
    };
    static const uint16_t g_time_text[] = {
        0x65F6, 0x95F4 /* 时间 */
    };
    static const uint16_t g_none_text[] = {
        0x65E0 /* 无 */
    };
    static const uint16_t g_no_text_text[] = {
        0x65E0, 0x6587, 0x672C /* 无文本 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 16, g_dispatch_text, 4);
    u8g2_DrawStr(u8g2, 50, 16, ":");
    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 58, 16, "0");

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 70, 16, g_time_text, 2);
    u8g2_DrawStr(u8g2, 94, 16, ":");
    lcd_u8g2_draw_unicode_seq(u8g2, 102, 16, g_none_text, 1);

    lcd_u8g2_draw_unicode_seq(u8g2, 2, 36, g_no_text_text, 3);

    u8g2_port_flush_buffer();
}


static void lcd_render_vehicle_info_ui(void)
{
    u8g2_t *u8g2;
    static const uint16_t g_plate_text[] = {
        0x8F66, 0x724C, 0x53F7 /* 车牌号 */
    };
    static const uint16_t g_type_text[] = {
        0x8F66, 0x8F86, 0x5206, 0x7C7B /* 车辆分类 */
    };
    static const uint16_t g_tid_text[] = {
        0x7EC8, 0x7AEF, 0x0049, 0x0044 /* 终端ID */
    };
    static const uint16_t g_pulse_text[] = {
        0x8109, 0x51B2, 0x7CFB, 0x6570 /* 脉冲系数 */
    };
    static const uint16_t g_vin_text[] = {
        0x0056, 0x0049, 0x004E, 0x7801 /* VIN码 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    lcd_u8g2_draw_unicode_seq(u8g2, 2, 12, g_plate_text, 3);
    u8g2_DrawStr(u8g2, 38, 12, ":");

    lcd_u8g2_draw_unicode_seq(u8g2, 2, 24, g_type_text, 4);
    u8g2_DrawStr(u8g2, 50, 24, ":");

    lcd_u8g2_draw_unicode_seq(u8g2, 2, 36, g_tid_text, 4);
    u8g2_DrawStr(u8g2, 46, 36, ":");

    lcd_u8g2_draw_unicode_seq(u8g2, 2, 48, g_pulse_text, 4);
    u8g2_DrawStr(u8g2, 50, 48, ":");
    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 56, 48, "3600 P/Km");

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 60, g_vin_text, 4);
    u8g2_DrawStr(u8g2, 38, 60, ":");

    u8g2_port_flush_buffer();
}

//static void lcd_vehicle_draw_symbol(u8g2_t *u8g2, uint8_t x, uint8_t y, rt_bool_t ok)
//{
//    static const uint16_t g_ok_text[] = {0x2713};     /* √ */
//    static const uint16_t g_fail_text[] = {0x00D7};   /* × */
//
//    lcd_u8g2_draw_unicode_seq(u8g2, x, y,
//                              (ok == RT_TRUE) ? g_ok_text : g_fail_text,
//                              1U);
//}
static void lcd_vehicle_draw_symbol(u8g2_t *u8g2, uint8_t x, uint8_t y, rt_bool_t ok)
{
    if (ok == RT_TRUE) {
        /* Draw check mark, no font dependency. */
        u8g2_DrawLine(u8g2, x + 1U, y - 5U, x + 4U, y - 2U);
        u8g2_DrawLine(u8g2, x + 4U, y - 2U, x + 10U, y - 10U);

        /* Thicken by one pixel. */
        u8g2_DrawLine(u8g2, x + 1U, y - 4U, x + 4U, y - 1U);
        u8g2_DrawLine(u8g2, x + 4U, y - 1U, x + 10U, y - 9U);
    } else {
        /* Draw X, no font dependency. */
        u8g2_DrawLine(u8g2, x + 1U, y - 10U, x + 9U, y - 2U);
        u8g2_DrawLine(u8g2, x + 9U, y - 10U, x + 1U, y - 2U);

        /* Thicken by one pixel. */
        u8g2_DrawLine(u8g2, x + 2U, y - 10U, x + 10U, y - 2U);
        u8g2_DrawLine(u8g2, x + 10U, y - 10U, x + 2U, y - 2U);
    }
}



static void lcd_vehicle_draw_cn_left(u8g2_t *u8g2,
                                     uint8_t y,
                                     const uint16_t *label,
                                     uint8_t label_count,
                                     rt_bool_t ok)
{
    lcd_u8g2_draw_unicode_seq(u8g2, 2, y, label, label_count);
    lcd_vehicle_draw_symbol(u8g2, (uint8_t)(2U + label_count * 12U + 2U), y, ok);
}

static void lcd_vehicle_draw_cn_right(u8g2_t *u8g2,
                                      uint8_t y,
                                      const uint16_t *label,
                                      uint8_t label_count,
                                      rt_bool_t ok)
{
    uint8_t symbol_x = 118U;
    uint8_t label_x = (uint8_t)(symbol_x - 2U - label_count * 12U);

    lcd_u8g2_draw_unicode_seq(u8g2, label_x, y, label, label_count);
    lcd_vehicle_draw_symbol(u8g2, symbol_x, y, ok);
}

static void lcd_vehicle_draw_ascii_left(u8g2_t *u8g2,
                                        uint8_t y,
                                        const char *label,
                                        rt_bool_t ok)
{
    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 2, y, label);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_vehicle_draw_symbol(u8g2, 32, y, ok);
}

static void lcd_vehicle_draw_ascii_right(u8g2_t *u8g2,
                                         uint8_t y,
                                         const char *label,
                                         rt_bool_t ok)
{
    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 84, y, label);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_vehicle_draw_symbol(u8g2, 118, y, ok);
}

static void lcd_render_vehicle_status_ui(void)
{
    u8g2_t *u8g2;
    const app_vehicle_io_state_t *state;
    uint8_t page;

    static const uint16_t g_brake_text[] = {
        0x5236, 0x52A8 /* 制动 */
    };
    static const uint16_t g_left_turn_text[] = {
        0x5DE6, 0x8F6C, 0x5411, 0x706F /* 左转向灯 */
    };
    static const uint16_t g_right_turn_text[] = {
        0x53F3, 0x8F6C, 0x5411, 0x706F /* 右转向灯 */
    };
    static const uint16_t g_small_light_text[] = {
        0x5C0F, 0x706F /* 小灯 */
    };
    static const uint16_t g_high_beam_text[] = {
        0x8FDC, 0x5149, 0x706F /* 远光灯 */
    };
    static const uint16_t g_low_beam_text[] = {
        0x8FD1, 0x5149, 0x706F /* 近光灯 */
    };
    static const uint16_t g_rear_fog_text[] = {
        0x540E, 0x96FE, 0x706F /* 后雾灯 */
    };
    static const uint16_t g_reverse_text[] = {
        0x5012, 0x8F66 /* 倒车 */
    };
    static const uint16_t g_door_text[] = {
        0x8F66, 0x95E8 /* 车门 */
    };
    static const uint16_t g_seat_belt_text[] = {
        0x5EA7, 0x6905, 0x5B89, 0x5168, 0x5E26 /* 座椅安全带 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    state = svc_vehicle_io_get_state();
    if (state == RT_NULL) {
        return;
    }

    page = g_lcd_page_selected[LCD_PAGE_DEVICE_STATUS_VEHICLE];

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    if (page == 0U) {
        lcd_vehicle_draw_ascii_left(u8g2, 14, "ACC",
                                    (state->wk_acc == 0U) ? RT_TRUE : RT_FALSE);
        lcd_vehicle_draw_ascii_right(u8g2, 14, "ON",
                                     (state->wk_on == 0U) ? RT_TRUE : RT_FALSE);

        lcd_vehicle_draw_cn_left(u8g2, 34, g_brake_text, 2U,
                                 (state->sw_kl2 != 0U) ? RT_TRUE : RT_FALSE);
        lcd_vehicle_draw_cn_right(u8g2, 34, g_left_turn_text, 4U,
                                  (state->sw_kl3 != 0U) ? RT_TRUE : RT_FALSE);

        lcd_vehicle_draw_cn_left(u8g2, 54, g_right_turn_text, 4U,
                                 (state->sw_kl4 != 0U) ? RT_TRUE : RT_FALSE);
        lcd_vehicle_draw_cn_right(u8g2, 54, g_small_light_text, 2U,
                                  (state->sw_kl1 != 0U) ? RT_TRUE : RT_FALSE);
    } else if (page == 1U) {
        lcd_vehicle_draw_ascii_left(u8g2, 14, "SOS", RT_TRUE);
        lcd_vehicle_draw_cn_right(u8g2, 14, g_high_beam_text, 3U,
                                  (state->sw_kl5 != 0U) ? RT_TRUE : RT_FALSE);

        lcd_vehicle_draw_cn_left(u8g2, 34, g_low_beam_text, 3U,
                                 (state->sw_kl6 != 0U) ? RT_TRUE : RT_FALSE);
        lcd_vehicle_draw_cn_right(u8g2, 34, g_rear_fog_text, 3U,
                                  (state->sw_kl7 != 0U) ? RT_TRUE : RT_FALSE);

        lcd_vehicle_draw_cn_left(u8g2, 54, g_reverse_text, 2U,
                                 (state->sw_kl8 != 0U) ? RT_TRUE : RT_FALSE);
        lcd_vehicle_draw_cn_right(u8g2, 54, g_door_text, 2U,
                                  (state->sw_kl10 == 0U) ? RT_TRUE : RT_FALSE);
    } else {
        lcd_vehicle_draw_cn_left(u8g2, 24, g_seat_belt_text, 5U,
                                 (state->sw_kl9 == 0U) ? RT_TRUE : RT_FALSE);
    }

    u8g2_port_flush_buffer();
}



static void lcd_render_vehicle_load_status_ui(void)
{
    u8g2_t *u8g2;
    static const uint16_t g_item_1[] = {
        0x8BBE, 0x4E3A, 0x7A7A, 0x8F7D /* 设为空载 */
    };
    static const uint16_t g_item_2[] = {
        0x8BBE, 0x4E3A, 0x534A, 0x8F7D /* 设为半载 */
    };
    static const uint16_t g_item_3[] = {
        0x8BBE, 0x4E3A, 0x6EE1, 0x8F7D /* 设为满载 */
    };
    const uint16_t *texts[3] = {
        g_item_1, g_item_2, g_item_3
    };
    uint8_t i;

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    for (i = 0U; i < 3U; i++) {
        uint8_t y = (uint8_t)(18U + i * 16U);

        if (g_lcd_page_selected[LCD_PAGE_DRIVE_RECORD_LOAD_STATUS] == i) {
            u8g2_DrawBox(u8g2, 0, (uint8_t)(y - 11U), LCD_COLS, 13U);
            u8g2_SetDrawColor(u8g2, 0);
        }

        u8g2_SetFont(u8g2, LCD_FONT_CN_12);
        lcd_u8g2_draw_unicode_seq(u8g2, 10, y, texts[i], 4);

        u8g2_SetDrawColor(u8g2, 1);
    }

    u8g2_port_flush_buffer();
}

static void lcd_render_vehicle_load_status_ok_ui(void)
{
    u8g2_t *u8g2;
    static const uint16_t g_ok_text[] = {
        0x8BBE, 0x7F6E, 0x6210, 0x529F /* 设置成功 */
    };
    static const uint16_t g_confirm_text[] = {
        0x786E, 0x8BA4 /* 确认 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 34, 28, g_ok_text, 4);
    lcd_u8g2_draw_unicode_seq(u8g2, 52, 58, g_confirm_text, 2);

    u8g2_port_flush_buffer();
}


static void lcd_render_location_status_ui(void)
{
    u8g2_t *u8g2;
    char satellite_str[8];
    char speed_value_str[16];
    static const uint16_t g_satellite_text[] = {
        0x536B, 0x661F /* 卫星 */
    };
    static const uint16_t g_loc_ok_text[] = {
        0x5DF2, 0x5B9A, 0x4F4D /* 已定位 */
    };
    static const uint16_t g_loc_ng_text[] = {
        0x672A, 0x5B9A, 0x4F4D /* 未定位 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    rt_snprintf(satellite_str, sizeof(satellite_str), "%02u",
                (unsigned int)g_lcd_home_ui.top_status_value);

    rt_snprintf(speed_value_str, sizeof(speed_value_str), "%u.%u km/h",
                (unsigned int)(g_lcd_home_ui.speed_kmh_x10 / 10U),
                (unsigned int)(g_lcd_home_ui.speed_kmh_x10 % 10U));

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 16, g_satellite_text, 2);
    u8g2_DrawStr(u8g2, 28, 16, ":");

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 36, 16, satellite_str);
    u8g2_DrawStr(u8g2, 72, 16, speed_value_str);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    if (g_lcd_home_ui.top_status_value == 0U) {
        lcd_u8g2_draw_unicode_seq(u8g2, 2, 38, g_loc_ng_text, 3);
    } else {
        lcd_u8g2_draw_unicode_seq(u8g2, 2, 38, g_loc_ok_text, 3);
    }

    u8g2_port_flush_buffer();
}

static void lcd_render_device_version_ui(void)
{
    u8g2_t *u8g2;
    static const uint16_t g_sw_version_text[] = {
        0x8F6F, 0x4EF6, 0x7248, 0x672C /* 软件版本 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 18, g_sw_version_text, 4);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 54, 18, ":");
    u8g2_DrawStr(u8g2, 62, 18, APP_SOFTWARE_VERSION);

    u8g2_port_flush_buffer();
}

static void lcd_page_confirm_load_status(void);

static const lcd_page_id_t g_main_menu_children[] = {
    LCD_PAGE_DRIVE_RECORD_MENU,
    LCD_PAGE_DEVICE_STATUS_MENU,
    LCD_PAGE_SYSTEM_SETTING_MENU
};

static const lcd_page_id_t g_drive_record_children[] = {
    LCD_PAGE_DRIVE_RECORD_FATIGUE,
    LCD_PAGE_DRIVE_RECORD_LOCATION,
    LCD_PAGE_DRIVE_RECORD_MILEAGE,
    LCD_PAGE_DRIVE_RECORD_DRIVER_INFO,
    LCD_PAGE_DRIVE_RECORD_VEHICLE_INFO,
    LCD_PAGE_DRIVE_RECORD_LOAD_STATUS,
    LCD_PAGE_DRIVE_RECORD_EXPORT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER,
    LCD_PAGE_DRIVE_RECORD_VIN
};

static const lcd_page_id_t g_device_status_children[] = {
    LCD_PAGE_DEVICE_STATUS_VERSION,
    LCD_PAGE_DEVICE_STATUS_GFRS,
    LCD_PAGE_DEVICE_STATUS_SELF_TEST,
    LCD_PAGE_DEVICE_STATUS_LOCATION_MODULE,
    LCD_PAGE_DEVICE_STATUS_VEHICLE,
    LCD_PAGE_DEVICE_STATUS_STORAGE,
    LCD_PAGE_DEVICE_STATUS_AV,
    LCD_PAGE_DEVICE_STATUS_SPEED,
    LCD_PAGE_DEVICE_STATUS_DRIVER_MONITOR
};

static const lcd_page_id_t g_info_center_children[] = {
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_BROADCAST,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EVENT_REPORT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EWAYBILL
};

static const lcd_page_id_t g_info_center_text_children[] = {
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_DISPATCH,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_NORMAL,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_FAULT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_EMERGENCY
};


static const lcd_page_id_t g_system_setting_children[] = {
    LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME,
    LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM,
    LCD_PAGE_SYSTEM_SETTING_INIT_MILEAGE,
    LCD_PAGE_SYSTEM_SETTING_REGISTER,
    LCD_PAGE_SYSTEM_SETTING_UNREGISTER,
    LCD_PAGE_SYSTEM_SETTING_REC_VER,
    LCD_PAGE_SYSTEM_SETTING_COMPONENT_VER
};

static const lcd_page_node_t g_lcd_pages[LCD_PAGE_MAX] = {
    [LCD_PAGE_HOME] = {
        LCD_PAGE_HOME, LCD_PAGE_MAX, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, lcd_render_home_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_MAIN_MENU] = {
        LCD_PAGE_MAIN_MENU, LCD_PAGE_HOME, LCD_PAGE_KIND_LIST,
        g_main_menu_children, 3U, 3U, lcd_render_menu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_MENU] = {
        LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_MAIN_MENU, LCD_PAGE_KIND_LIST,
        g_drive_record_children, 9U, 9U, lcd_render_drive_record_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DEVICE_STATUS_MENU] = {
        LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_MAIN_MENU, LCD_PAGE_KIND_LIST,
        g_device_status_children, 9U, 9U, lcd_render_drive_record_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_MENU] = {
        LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_MAIN_MENU, LCD_PAGE_KIND_LIST,
        g_system_setting_children, 9U, 9U, lcd_render_drive_record_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_FATIGUE] = {
        LCD_PAGE_DRIVE_RECORD_FATIGUE, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, lcd_render_fatigue_drive_record_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_LOCATION] = {
        LCD_PAGE_DRIVE_RECORD_LOCATION, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, lcd_render_location_status_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_MILEAGE] = {
        LCD_PAGE_DRIVE_RECORD_MILEAGE, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, lcd_render_drive_mileage_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_DRIVER_INFO] = {
        LCD_PAGE_DRIVE_RECORD_DRIVER_INFO, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, lcd_render_driver_info_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_VEHICLE_INFO] = {
        LCD_PAGE_DRIVE_RECORD_VEHICLE_INFO, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, lcd_render_vehicle_info_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_LOAD_STATUS] = {
        LCD_PAGE_DRIVE_RECORD_LOAD_STATUS, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_LIST,
        RT_NULL, 0U, 3U, lcd_render_vehicle_load_status_ui, lcd_page_confirm_load_status, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_LOAD_STATUS_OK] = {
        LCD_PAGE_DRIVE_RECORD_LOAD_STATUS_OK, LCD_PAGE_DRIVE_RECORD_LOAD_STATUS, LCD_PAGE_KIND_ACTION_RESULT,
        RT_NULL, 0U, 0U, lcd_render_vehicle_load_status_ok_ui, RT_NULL, 3000U, LCD_PAGE_DRIVE_RECORD_LOAD_STATUS
    },
    [LCD_PAGE_DRIVE_RECORD_EXPORT] = {
        LCD_PAGE_DRIVE_RECORD_EXPORT, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_INFO_CENTER] = {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER,
        LCD_PAGE_DRIVE_RECORD_MENU,
        LCD_PAGE_KIND_LIST,
        g_info_center_children,
        4U,
        4U,
        lcd_render_drive_record_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_INFO_CENTER_BROADCAST] = {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_BROADCAST,
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        lcd_render_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },

    [LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EVENT_REPORT] = {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EVENT_REPORT,
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        lcd_render_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },

    [LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EWAYBILL] = {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EWAYBILL,
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        lcd_render_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },

    [LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT] = {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT,
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER,
        LCD_PAGE_KIND_LIST,
        g_info_center_text_children,
        4U,
        4U,
        lcd_render_drive_record_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_DISPATCH] = {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_DISPATCH,
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        lcd_render_info_center_dispatch_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_NORMAL] = {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_NORMAL,
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        lcd_render_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_FAULT] = {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_FAULT,
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        lcd_render_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_EMERGENCY] = {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_EMERGENCY,
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        lcd_render_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },

    [LCD_PAGE_DRIVE_RECORD_VIN] = {
        LCD_PAGE_DRIVE_RECORD_VIN, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DEVICE_STATUS_VERSION] = { LCD_PAGE_DEVICE_STATUS_VERSION, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_device_version_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_GFRS] = { LCD_PAGE_DEVICE_STATUS_GFRS, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_SELF_TEST] = { LCD_PAGE_DEVICE_STATUS_SELF_TEST, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_LOCATION_MODULE] = { LCD_PAGE_DEVICE_STATUS_LOCATION_MODULE, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_VEHICLE] = { LCD_PAGE_DEVICE_STATUS_VEHICLE, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_LIST, RT_NULL, 0U, 3U, lcd_render_vehicle_status_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_STORAGE] = { LCD_PAGE_DEVICE_STATUS_STORAGE, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_AV] = { LCD_PAGE_DEVICE_STATUS_AV, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_SPEED] = { LCD_PAGE_DEVICE_STATUS_SPEED, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_DRIVER_MONITOR] = { LCD_PAGE_DEVICE_STATUS_DRIVER_MONITOR, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME] = { LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS] = { LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO] = { LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_HOST_PARAM] = { LCD_PAGE_SYSTEM_SETTING_HOST_PARAM, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_INIT_MILEAGE] = { LCD_PAGE_SYSTEM_SETTING_INIT_MILEAGE, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_REGISTER] = { LCD_PAGE_SYSTEM_SETTING_REGISTER, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_UNREGISTER] = { LCD_PAGE_SYSTEM_SETTING_UNREGISTER, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_REC_VER] = { LCD_PAGE_SYSTEM_SETTING_REC_VER, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_COMPONENT_VER] = { LCD_PAGE_SYSTEM_SETTING_COMPONENT_VER, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX }
};

static const lcd_page_node_t *lcd_get_page_node(lcd_page_id_t page_id)
{
    if (page_id >= LCD_PAGE_MAX) {
        return RT_NULL;
    }

    return &g_lcd_pages[page_id];
}

static uint8_t lcd_page_get_depth(lcd_page_id_t page_id)
{
    uint8_t depth = 0U;

    while ((page_id < LCD_PAGE_MAX) && (g_lcd_pages[page_id].parent_id < LCD_PAGE_MAX)) {
        depth++;
        page_id = g_lcd_pages[page_id].parent_id;
    }

    return depth;
}

static uint8_t lcd_page_get_select_count(lcd_page_id_t page_id)
{
    const lcd_page_node_t *page = lcd_get_page_node(page_id);

    if (page == RT_NULL) {
        return 0U;
    }

    if (page->select_count != 0U) {
        return page->select_count;
    }

    return page->child_count;
}

static void lcd_page_enter(lcd_page_id_t page_id)
{
    if (page_id >= LCD_PAGE_MAX) {
        return;
    }

    g_lcd_current_page_id = page_id;
    g_lcd_menu_mode = (page_id != LCD_PAGE_HOME) ? RT_TRUE : RT_FALSE;
    g_lcd_page_enter_tick = rt_tick_get();
    g_lcd_need_redraw = RT_TRUE;
}

static void lcd_page_confirm_load_status(void)
{
    lcd_page_enter(LCD_PAGE_DRIVE_RECORD_LOAD_STATUS_OK);
}

static void lcd_page_handle_back(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);

    if (g_lcd_current_page_id == LCD_PAGE_HOME) {
        lcd_page_enter(LCD_PAGE_MAIN_MENU);
        return;
    }

    if ((page != RT_NULL) && (page->parent_id < LCD_PAGE_MAX)) {
        lcd_page_enter(page->parent_id);
    } else {
        lcd_page_enter(LCD_PAGE_HOME);
    }
}

static void lcd_page_handle_nav(int8_t delta)
{
    uint8_t select_count = lcd_page_get_select_count(g_lcd_current_page_id);
    uint8_t *selected = &g_lcd_page_selected[g_lcd_current_page_id];

    if (select_count == 0U) {
        return;
    }

    if ((delta < 0) && (*selected > 0U)) {
        (*selected)--;
        g_lcd_need_redraw = RT_TRUE;
    } else if ((delta > 0) && (*selected + 1U < select_count)) {
        (*selected)++;
        g_lcd_need_redraw = RT_TRUE;
    }
}

static void lcd_page_handle_confirm(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);
    uint8_t selected_index;

    if (page == RT_NULL) {
        return;
    }

    if (page->kind == LCD_PAGE_KIND_ACTION_RESULT) {
        if (page->auto_return_target < LCD_PAGE_MAX) {
            lcd_page_enter(page->auto_return_target);
        }
        return;
    }

    if (page->on_confirm != RT_NULL) {
        page->on_confirm();
        return;
    }

    if ((page->kind == LCD_PAGE_KIND_LIST) &&
        (page->children != RT_NULL) &&
        (page->child_count > 0U)) {
        selected_index = g_lcd_page_selected[g_lcd_current_page_id];
        if (selected_index < page->child_count) {
            lcd_page_enter(page->children[selected_index]);
        }
    }
}

static void lcd_page_handle_auto_return(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);

    if ((page == RT_NULL) || (page->auto_return_ms == 0U) || (page->auto_return_target >= LCD_PAGE_MAX)) {
        return;
    }

    if ((rt_tick_get() - g_lcd_page_enter_tick) >= rt_tick_from_millisecond(page->auto_return_ms)) {
        lcd_page_enter(page->auto_return_target);
    }
}

static uint16_t lcd_vehicle_status_pack_bits(const app_vehicle_io_state_t *state)
{
    uint16_t bits = 0U;

    bits |= (state->wk_acc  ? (1U << 0) : 0U);
    bits |= (state->wk_on   ? (1U << 1) : 0U);
    bits |= (state->sw_kl1  ? (1U << 2) : 0U);
    bits |= (state->sw_kl2  ? (1U << 3) : 0U);
    bits |= (state->sw_kl3  ? (1U << 4) : 0U);
    bits |= (state->sw_kl4  ? (1U << 5) : 0U);
    bits |= (state->sw_kl5  ? (1U << 6) : 0U);
    bits |= (state->sw_kl6  ? (1U << 7) : 0U);
    bits |= (state->sw_kl7  ? (1U << 8) : 0U);
    bits |= (state->sw_kl8  ? (1U << 9) : 0U);
    bits |= (state->sw_kl9  ? (1U << 10) : 0U);
    bits |= (state->sw_kl10 ? (1U << 11) : 0U);

    return bits;
}

static void lcd_page_handle_dynamic_refresh(void)
{
    rt_tick_t now;
    const app_vehicle_io_state_t *state;
    uint16_t bits;

    if (g_lcd_current_page_id != LCD_PAGE_DEVICE_STATUS_VEHICLE) {
        g_lcd_vehicle_status_last_bits = 0xFFFFU;
        return;
    }

    now = rt_tick_get();
    if ((now - g_lcd_vehicle_status_check_tick) < rt_tick_from_millisecond(100U)) {
        return;
    }
    g_lcd_vehicle_status_check_tick = now;

    state = svc_vehicle_io_get_state();
    if (state == RT_NULL) {
        return;
    }

    bits = lcd_vehicle_status_pack_bits(state);

    if (g_lcd_vehicle_status_last_bits == 0xFFFFU) {
        g_lcd_vehicle_status_last_bits = bits;
        return;
    }

    if (bits != g_lcd_vehicle_status_last_bits) {
        g_lcd_vehicle_status_last_bits = bits;
        g_lcd_need_redraw = RT_TRUE;
    }
}


static void lcd_render_current_page(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);

    if ((page != RT_NULL) && (page->render != RT_NULL)) {
        page->render();
    } else {
        lcd_render_submenu_ui();
    }
}

static rt_bool_t lcd_get_list_page_resources(lcd_page_id_t page_id,
                                             const uint16_t *const **item_texts,
                                             const uint8_t **item_counts,
                                             uint8_t *item_count,
                                             const uint16_t **title_text,
                                             uint8_t *title_count)
{
    if ((item_texts == RT_NULL) || (item_counts == RT_NULL) || (item_count == RT_NULL) ||
        (title_text == RT_NULL) || (title_count == RT_NULL)) {
        return RT_FALSE;
    }

    switch (page_id) {
    case LCD_PAGE_DRIVE_RECORD_MENU:
        *item_texts = g_drive_sub_item_texts_u8g2;
        *item_counts = g_drive_sub_item_counts_u8g2;
        *item_count = 9U;
        *title_text = g_menu_item_text_1;
        *title_count = 4U;
        return RT_TRUE;

    case LCD_PAGE_DEVICE_STATUS_MENU:
        *item_texts = g_device_sub_item_texts_u8g2;
        *item_counts = g_device_sub_item_counts_u8g2;
        *item_count = 9U;
        *title_text = g_menu_item_text_2;
        *title_count = 4U;
        return RT_TRUE;

    case LCD_PAGE_SYSTEM_SETTING_MENU:
        *item_texts = g_system_sub_item_texts_u8g2;
        *item_counts = g_system_sub_item_counts_u8g2;
        *item_count = 9U;
        *title_text = g_menu_item_text_3;
        *title_count = 4U;
        return RT_TRUE;

    case LCD_PAGE_DRIVE_RECORD_INFO_CENTER:
        *item_texts = g_info_center_item_texts_u8g2;
        *item_counts = g_info_center_item_counts_u8g2;
        *item_count = 4U;
        *title_text = g_drive_sub_item_8;   /* 信息中心 */
        *title_count = 4U;
        return RT_TRUE;

    case LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT:
        *item_texts = g_info_text_item_texts_u8g2;
        *item_counts = g_info_text_item_counts_u8g2;
        *item_count = 4U;
        *title_text = g_info_center_item_1; /* 文本信息 */
        *title_count = 4U;
        return RT_TRUE;

    default:
        break;
    }

    return RT_FALSE;
}



static rt_bool_t lcd_home_ui_set_data(uint16_t speed_kmh_x10,
                                      uint8_t hour,
                                      uint8_t minute,
                                      uint8_t second,
                                      uint8_t drive_hour,
                                      uint8_t drive_minute,
                                      uint8_t drive_second,
                                      const char *card_id)
{
    rt_bool_t changed = RT_FALSE;

    if (g_lcd_home_ui.speed_kmh_x10 != speed_kmh_x10) {
        g_lcd_home_ui.speed_kmh_x10 = speed_kmh_x10;
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
    (void)lcd_home_ui_set_data(g_lcd_home_ui.speed_kmh_x10,
                               hour,
                               minute,
                               second,
                               g_lcd_home_ui.drive_hour,
                               g_lcd_home_ui.drive_minute,
                               g_lcd_home_ui.drive_second,
                               g_lcd_home_ui.card_id);
}

void svc_lcd_update_top_status(uint8_t value)
{
    if (g_lcd_home_ui.top_status_value != value) {
        g_lcd_home_ui.top_status_value = value;

        if (g_lcd_menu_mode == RT_FALSE) {
            g_lcd_need_redraw = RT_TRUE;
        }
    }
}

void svc_lcd_update_home_speed(uint16_t speed_kmh_x10)
{
    (void)lcd_home_ui_set_data(speed_kmh_x10,
                               g_lcd_home_ui.hour,
                               g_lcd_home_ui.minute,
                               g_lcd_home_ui.second,
                               g_lcd_home_ui.drive_hour,
                               g_lcd_home_ui.drive_minute,
                               g_lcd_home_ui.drive_second,
                               g_lcd_home_ui.card_id);
}

void svc_lcd_update_drive_time(uint8_t hour, uint8_t minute, uint8_t second)
{
    (void)lcd_home_ui_set_data(g_lcd_home_ui.speed_kmh_x10,
                               g_lcd_home_ui.hour,
                               g_lcd_home_ui.minute,
                               g_lcd_home_ui.second,
                               hour,
                               minute,
                               second,
                               g_lcd_home_ui.card_id);
}

void svc_lcd_update_card_id(const char *card_id)
{
    if (card_id == RT_NULL) {
        return;
    }

    (void)lcd_home_ui_set_data(g_lcd_home_ui.speed_kmh_x10,
                               g_lcd_home_ui.hour,
                               g_lcd_home_ui.minute,
                               g_lcd_home_ui.second,
                               g_lcd_home_ui.drive_hour,
                               g_lcd_home_ui.drive_minute,
                               g_lcd_home_ui.drive_second,
                               card_id);
}

void svc_lcd_update_overtime_drive_count(uint16_t count)
{
    if (g_lcd_overtime_drive_count != count) {
        g_lcd_overtime_drive_count = count;

        if (g_lcd_current_page_id == LCD_PAGE_DRIVE_RECORD_FATIGUE) {
            g_lcd_need_redraw = RT_TRUE;
        }
    }
}

void svc_lcd_update_total_mileage(uint32_t odo_km, uint16_t odo_rem_m)
{
    if ((g_lcd_total_mileage_km != odo_km) ||
        (g_lcd_total_mileage_rem_m != odo_rem_m)) {
        g_lcd_total_mileage_km = odo_km;
        g_lcd_total_mileage_rem_m = odo_rem_m;

        if (g_lcd_current_page_id == LCD_PAGE_DRIVE_RECORD_MILEAGE) {
            g_lcd_need_redraw = RT_TRUE;
        }
    }
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
    char value_str[4];

    /* 第1个图标始终显示 */
    lcd_u8g2_draw_bitmap12x12(u8g2, x, y, g_icon_signal_12x12);

    /* 第2、第3个图标和后面的两位数由第4字节控制 */
    if(g_lcd_home_ui.top_status_value !=0U)
    {
        lcd_u8g2_draw_bitmap12x12(u8g2, (uint8_t)(x+18U), y, g_icon_g_12x12);
        lcd_u8g2_draw_bitmap12x12(u8g2, (uint8_t)(x+36U), y, g_icon_status_12x12);


        rt_snprintf(value_str, sizeof(value_str), "%02u",
                            (unsigned int)g_lcd_home_ui.top_status_value);

        u8g2_SetFont(u8g2,u8g2_font_5x7_tf);
        u8g2_DrawStr(u8g2,(uint8_t)(x+50U),(uint8_t)(y+9U),value_str);


    }

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
    static uint16_t speed_kmh_x10 = 0U;
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

    speed_kmh_x10++;

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

    lcd_home_ui_set_data(speed_kmh_x10,
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



    rt_snprintf(speed_str, sizeof(speed_str), "%u.%u km/h",
                (unsigned int)(g_lcd_home_ui.speed_kmh_x10 / 10U),
                (unsigned int)(g_lcd_home_ui.speed_kmh_x10 % 10U));


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

    /* 第1行：顶部图标 */
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
    rt_memset(g_lcd_page_selected, 0, sizeof(g_lcd_page_selected));
    g_lcd_current_page_id = LCD_PAGE_HOME;
    g_lcd_page_enter_tick = 0U;
    g_lcd_need_redraw = RT_TRUE;

    APP_NON_CAN_LOG("LCD: ui thread start\r\n");

    rt_thread_mdelay(APP_ADC_STARTUP_DELAY_MS + 100);

    /* Boot check page: stay 5 seconds, then enter home UI. */
    lcd_render_boot_check_ui();
    rt_thread_mdelay(5000);

    g_lcd_need_redraw = RT_TRUE;

    while (1)
    {
        if (svc_adc_consume_s1_event() == RT_TRUE) {
            lcd_page_handle_back();
        }




        if (svc_adc_consume_s2_event() == RT_TRUE) {
            lcd_page_handle_nav(-1);
        }

        if (svc_adc_consume_s3_event() == RT_TRUE) {
            lcd_page_handle_nav(1);
        }





        if (svc_adc_consume_s4_event() == RT_TRUE) {
            lcd_page_handle_confirm();
        }

        #if 0
        if (RT_FALSE) {
            if (g_lcd_menu_depth == 1U) {
                g_lcd_submenu_type = lcd_get_submenu_type_from_menu_index(g_lcd_menu_index);
                if (g_lcd_submenu_type != LCD_SUBMENU_NONE) {
                    g_lcd_submenu_index = 0U;
                    g_lcd_menu_depth = 2U;
                    g_lcd_menu_mode = RT_TRUE;
                    g_lcd_need_redraw = RT_TRUE;
                }
            } else if (g_lcd_menu_depth == 2U) {
                g_lcd_menu_depth = 3U;
                g_lcd_menu_mode = RT_TRUE;
                g_lcd_need_redraw = RT_TRUE;
            } else if (g_lcd_menu_depth == 3U) {
                if ((g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
                    (g_lcd_submenu_index == 0U)) {
                    /* 疲劳驾驶到此为止 */
                } else if ((g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
                           (g_lcd_submenu_index == 5U)) {
                    /* 车辆载重状态 -> 设置成功页 */
                    g_lcd_menu_depth = 4U;
                    g_lcd_load_status_ok_tick = rt_tick_get();
                    g_lcd_need_redraw = RT_TRUE;
                } else if (g_lcd_menu_depth < 4U) {
                    g_lcd_menu_depth++;
                    g_lcd_menu_mode = RT_TRUE;
                    g_lcd_need_redraw = RT_TRUE;
                }
            } else if (g_lcd_menu_depth == 4U) {
                if ((g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
                    (g_lcd_submenu_index == 5U)) {
                    /* 在设置成功页按 S4 提前返回 */
                    g_lcd_menu_depth = 3U;
                    g_lcd_need_redraw = RT_TRUE;
                }
            }
        }

        #endif

        lcd_page_handle_auto_return();
        lcd_page_handle_dynamic_refresh();

        #if 0
        if (RT_FALSE &&
            (g_lcd_menu_depth == 4U) &&
            (g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
            (g_lcd_submenu_index == 5U)) {
            if ((rt_tick_get() - g_lcd_load_status_ok_tick) >= rt_tick_from_millisecond(3000)) {
                g_lcd_menu_depth = 3U;
                g_lcd_need_redraw = RT_TRUE;
            }
        }


        #endif

        if (g_lcd_need_redraw == RT_TRUE) {
            lcd_render_current_page();
            g_lcd_need_redraw = RT_FALSE;
        }

        #if 0
        if (RT_FALSE) {
            if (g_lcd_menu_depth == 0U) {
                lcd_render_home_ui();
            } else if (g_lcd_menu_depth == 1U) {
                lcd_render_menu_ui();
            } else if (g_lcd_menu_depth == 2U) {
                if (g_lcd_submenu_type != LCD_SUBMENU_NONE) {
                    lcd_render_drive_record_submenu_ui();
                } else {
                    lcd_render_submenu_ui();
                }



            } else if (g_lcd_menu_depth == 3U) {
                if ((g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
                    (g_lcd_submenu_index == 0U)) {
                    lcd_render_fatigue_drive_record_ui();
                } else if ((g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
                           (g_lcd_submenu_index == 1U)) {
                    lcd_render_location_status_ui();
                } else if ((g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
                           (g_lcd_submenu_index == 2U)) {
                    lcd_render_drive_mileage_ui();
                } else if ((g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
                           (g_lcd_submenu_index == 3U)) {
                    lcd_render_driver_info_ui();
                } else if ((g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
                           (g_lcd_submenu_index == 4U)) {
                    lcd_render_vehicle_info_ui();
                } else if ((g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
                           (g_lcd_submenu_index == 5U)) {
                    lcd_render_vehicle_load_status_ui();
                } else {
                    lcd_render_submenu_ui();
                }
            } else if (g_lcd_menu_depth == 4U) {
                if ((g_lcd_submenu_type == LCD_SUBMENU_DRIVE_RECORD) &&
                    (g_lcd_submenu_index == 5U)) {
                    lcd_render_vehicle_load_status_ok_ui();
                } else {
                    lcd_render_submenu_ui();
                }
            } else {
                lcd_render_submenu_ui();
            }









            g_lcd_need_redraw = RT_FALSE;
        }
        #endif

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
















