/*
 * ============================================================
 *  svc_lcd.c — LCD 服务层 (最大的模块, ~4000 行)
 * ============================================================
 *
 * 功能概述:
 *   这是整个工程中最大的源文件, 实现了 LCD 显示的所有业务逻辑。
 *   它协调了三个子层 (lcd_drv → lcd_graphics → lcd_ui) 的配合,
 *   同时作为对外接口提供给其他模块更新显示数据。
 *
 * 本文件包含:
 *   1) 菜单文本数据 (所有中英文菜单的 Unicode 码点数组)
 *   2) 页面定义 (lcd_page_node_t 树形结构)
 *   3) 渲染函数 (主页、菜单列表、设置项等)
 *   4) 按键处理 (方向键、确认键、数字键盘编辑)
 *   5) 服务层接口 (svc_lcd_init, svc_lcd_task_start 等)
 *   6) 数据更新接口 (svc_lcd_update_xxx 系列)
 *
 * 为什么这么大?
 *   嵌入式项目中, LCD 显示通常是最"重"的模块, 因为:
 *   - 所有菜单文本都编译在代码中 (ROM 存储, 不占 RAM)
 *   - 每个页面有自己的渲染逻辑
 *   - 中文菜单文本以 Unicode 码点形式存储 (每个汉字 2 字节)
 *   如果未来需要缩减文件大小, 可以将菜单数据分离到 .c 数据文件
 *
 * 分层架构:
 *   svc_lcd          (服务层) ← 本文件, 协调各层
 *     ├── lcd_ui    (UI 层)  ← 页面导航、菜单管理
 *     ├── lcd_graphics (图形层) ← 帧缓冲管理
 *     └── lcd_drv   (驱动层) ← ST7567 硬件操作
 */
#include <rtthread.h>

#include "app_config.h"
#include "app_usart_cmd.h"
#include "svc_lcd.h"
#include "svc_adc.h"
#include "u8g2_port.h"
#include "svc_vehicle_io.h"
#include "svc_storage.h"
#include "LCD/lcd_drv.h"
#include "LCD/lcd_graphics.h"
#include "LCD/lcd_ui.h"
#include <string.h>

/* ---- 硬件参数 ---- */
#define LCD_COLS                    132     /* ST7567 列数 */
#define LCD_PAGES                   8       /* 页数 (64 行 / 8) */
#define LCD_ROWS                    64      /* 总行数 */

/* ---- UI 边距 ---- */
#define LCD_UI_MARGIN_LEFT          6
#define LCD_UI_MARGIN_RIGHT         6
#define LCD_UI_MARGIN_TOP           4
#define LCD_UI_MARGIN_BOTTOM        4

/* ---- 字体选择 ---- */
/*
 * LCD_FONT_ASCII_SMALL: 6x10 等宽 ASCII 字体
 * LCD_FONT_CN_12: 12x12 中文字体 (文泉驿, GB2312 编码)
 * 这两个字体来自 u8g2 图形库, 在 u8g2_port.c 中注册
 */
#define LCD_FONT_ASCII_SMALL    u8g2_font_6x10_tf
#define LCD_FONT_CN_12          u8g2_font_wqy12_t_gb2312


/*
 * ---- 车牌编辑焦点状态 ----
 *
 * 车牌编辑界面中, 焦点可以在以下位置切换:
 *   PROVINCE: 省份简称 (京/沪/湘/粤...)
 *   LETTER:   字母 (A~Z)
 *   DIGIT0~4: 5 位数字/字母
 *   CONFIRM:  确认按钮
 *   EXIT:     退出按钮
 *
 * 用户通过 S2/S3 切换焦点, S1/S4 修改值
 */
#define LCD_PLATE_FOCUS_PROVINCE  0U
#define LCD_PLATE_FOCUS_LETTER    1U
#define LCD_PLATE_FOCUS_DIGIT0    2U
#define LCD_PLATE_FOCUS_DIGIT1    3U
#define LCD_PLATE_FOCUS_DIGIT2    4U
#define LCD_PLATE_FOCUS_DIGIT3    5U
#define LCD_PLATE_FOCUS_DIGIT4    6U
#define LCD_PLATE_FOCUS_CONFIRM   7U
#define LCD_PLATE_FOCUS_EXIT      8U
#define LCD_PLATE_FOCUS_MAX       LCD_PLATE_FOCUS_EXIT

/* 车牌编辑状态全局变量 */
static uint8_t g_lcd_plate_dirty = 0U;              /* 车牌数据是否被修改过 */
static uint8_t g_lcd_plate_focus = LCD_PLATE_FOCUS_PROVINCE; /* 当前焦点位置 */
static uint8_t g_lcd_plate_valid = 0U;              /* 车牌是否有效 */
static uint8_t g_lcd_plate_province_index = 0U;     /* 省份索引 (0=京) */
static uint8_t g_lcd_plate_letter_index = 0U;       /* 字母索引 (0=A) */
static uint8_t g_lcd_plate_digits[5] = {0U};        /* 5位数字(0=0, 1=1...35=Z) */


#define LCD_VIN_LEN                  17U
#define LCD_VIN_CHAR_COUNT           36U

#define LCD_VIN_AREA_POSITION        0U
#define LCD_VIN_AREA_CHAR_SELECT     1U
#define LCD_VIN_AREA_ACTION          2U

#define LCD_VIN_ACTION_CONFIRM       0U
#define LCD_VIN_ACTION_EXIT          1U

#define LCD_VIN_COMPLETE_MASK        ((1UL << LCD_VIN_LEN) - 1UL)

static uint8_t g_lcd_vin_dirty = 0U;
static uint8_t g_lcd_vin_valid = 0U;
static uint8_t g_lcd_vin_area = LCD_VIN_AREA_POSITION;
static uint8_t g_lcd_vin_cursor = 0U;
static uint8_t g_lcd_vin_option_index = 0U;
static uint8_t g_lcd_vin_action = LCD_VIN_ACTION_EXIT;
static uint8_t g_lcd_vin_chars[LCD_VIN_LEN] = {0U};
static uint32_t g_lcd_vin_edit_mask = 0UL;

#define LCD_PROVINCE_ID_LEN                  2U
#define LCD_PROVINCE_ID_CHAR_COUNT           10U

#define LCD_PROVINCE_ID_AREA_POSITION        0U
#define LCD_PROVINCE_ID_AREA_CHAR_SELECT     1U
#define LCD_PROVINCE_ID_AREA_ACTION          2U

#define LCD_PROVINCE_ID_ACTION_CONFIRM       0U
#define LCD_PROVINCE_ID_ACTION_EXIT          1U

#define LCD_PROVINCE_ID_COMPLETE_MASK        ((1U << LCD_PROVINCE_ID_LEN) - 1U)



static uint8_t g_lcd_province_id_dirty = 0U;
static uint8_t g_lcd_province_id_valid = 0U;
static uint8_t g_lcd_province_id_area = LCD_PROVINCE_ID_AREA_POSITION;
static uint8_t g_lcd_province_id_cursor = 0U;
static uint8_t g_lcd_province_id_option_index = 0U;
static uint8_t g_lcd_province_id_action = LCD_PROVINCE_ID_ACTION_EXIT;
static uint8_t g_lcd_province_id_chars[LCD_PROVINCE_ID_LEN] = {0U};
static uint8_t g_lcd_province_id_edit_mask = 0U;


#define LCD_CITY_ID_LEN                  4U
#define LCD_CITY_ID_CHAR_COUNT           10U

#define LCD_CITY_ID_AREA_POSITION        0U
#define LCD_CITY_ID_AREA_CHAR_SELECT     1U
#define LCD_CITY_ID_AREA_ACTION          2U

#define LCD_CITY_ID_ACTION_CONFIRM       0U
#define LCD_CITY_ID_ACTION_EXIT          1U

#define LCD_CITY_ID_COMPLETE_MASK        ((1U << LCD_CITY_ID_LEN) - 1U)

static uint8_t g_lcd_city_id_dirty = 0U;
static uint8_t g_lcd_city_id_valid = 0U;
static uint8_t g_lcd_city_id_area = LCD_CITY_ID_AREA_POSITION;
static uint8_t g_lcd_city_id_cursor = 0U;
static uint8_t g_lcd_city_id_option_index = 0U;
static uint8_t g_lcd_city_id_action = LCD_CITY_ID_ACTION_EXIT;
static uint8_t g_lcd_city_id_chars[LCD_CITY_ID_LEN] = {0U};
static uint8_t g_lcd_city_id_edit_mask = 0U;


/* 主页子页面: 0=主主页, 1=副主页 (显示更多信息) */
static uint8_t g_lcd_home_subpage = 0U;
#define LCD_TEXT_MSG_LINES_PER_PAGE 3U
#define LCD_TEXT_MSG_LINE_MAX_CHARS 10U


char speed_str[16];
char time_str[16];
char drive_time_str[16];

#define g_lcd_home_ui                 (*lcd_ui_data_get_home_mutable())
#define g_lcd_text_msg                (*lcd_ui_data_get_text_mutable())

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

static const uint16_t g_vehicle_info_setting_item_1[] = {
    0x8F66, 0x8F86, 0x0056, 0x0049, 0x004E /* 车辆VIN */
};

static const uint16_t g_vehicle_info_setting_item_2[] = {
    0x8F66, 0x724C, 0x8BBE, 0x7F6E /* 车牌设置 */
};

static const uint16_t g_vehicle_info_setting_item_3[] = {
    0x8F66, 0x724C, 0x989C, 0x8272 /* 车牌颜色 */
};

static const uint16_t g_vehicle_info_setting_item_4[] = {
    0x8F66, 0x724C, 0x5206, 0x7C7B /* 车牌分类 */
};

static const uint16_t g_vehicle_info_setting_item_5[] = {
    0x7701, 0x57DF, 0x0049, 0x0044 /* 省域ID */
};

static const uint16_t g_vehicle_info_setting_item_6[] = {
    0x5E02, 0x57DF, 0x0049, 0x0044 /* 市域ID */
};

static const uint16_t g_host_param_item_1[] = {
    0x672C, 0x673A, 0x53F7, 0x7801, 0x8BBE, 0x7F6E /* 本机号码设置 */
};

static const uint16_t g_host_param_item_2[] = {
    0x0053, 0x004F, 0x0053, 0x53F7, 0x7801, 0x8BBE, 0x7F6E /* SOS号码设置 */
};

static const uint16_t g_host_param_item_3[] = {
    0x7B2C, 0x4E00, 0x670D, 0x52A1, 0x5668 /* 第一服务器 */
};

static const uint16_t g_host_param_item_4[] = {
    0x7B2C, 0x4E8C, 0x670D, 0x52A1, 0x5668 /* 第二服务器 */
};

static const uint16_t *const g_host_param_item_texts_u8g2[] = {
    g_host_param_item_1,
    g_host_param_item_2,
    g_host_param_item_3,
    g_host_param_item_4
};

static const uint8_t g_host_param_item_counts_u8g2[] = {
    6, 7, 5, 5
};


static const uint16_t *const g_vehicle_info_setting_item_texts_u8g2[] = {
    g_vehicle_info_setting_item_1,
    g_vehicle_info_setting_item_2,
    g_vehicle_info_setting_item_3,
    g_vehicle_info_setting_item_4,
    g_vehicle_info_setting_item_5,
    g_vehicle_info_setting_item_6
};

static const uint8_t g_vehicle_info_setting_item_counts_u8g2[] = {
    5, 4, 4, 4, 4, 4
};

static const uint16_t g_vehicle_plate_class_item_1[] = {
    0x4E58, 0x7528, 0x8F66 /* 乘用车 */
};

static const uint16_t g_vehicle_plate_class_item_2[] = {
    0x8D27, 0x8F66 /* 货车 */
};

static const uint16_t g_vehicle_plate_class_item_3[] = {
    0x4E13, 0x7528, 0x6C7D, 0x8F66 /* 专用汽车 */
};

static const uint16_t g_vehicle_plate_class_item_4[] = {
    0x6302, 0x8F66 /* 挂车 */
};

static const uint16_t g_vehicle_plate_class_item_5[] = {
    0x6C7D, 0x8F66, 0x5217, 0x8F66 /* 汽车列车 */
};

static const uint16_t *const g_vehicle_plate_class_item_texts_u8g2[] = {
    g_vehicle_plate_class_item_1,
    g_vehicle_plate_class_item_2,
    g_vehicle_plate_class_item_3,
    g_vehicle_plate_class_item_4,
    g_vehicle_plate_class_item_5
};

static const uint8_t g_vehicle_plate_class_item_counts_u8g2[] = {
    3, 2, 4, 2, 4
};

static const uint16_t g_vehicle_plate_color_item_1[] = {
    0x84DD, 0x8272 /* 蓝色 */
};

static const uint16_t g_vehicle_plate_color_item_2[] = {
    0x9EC4, 0x8272 /* 黄色 */
};

static const uint16_t g_vehicle_plate_color_item_3[] = {
    0x767D, 0x8272 /* 白色 */
};

static const uint16_t g_vehicle_plate_color_item_4[] = {
    0x9ED1, 0x8272 /* 黑色 */
};

static const uint16_t g_vehicle_plate_color_item_5[] = {
    0x7EFF, 0x8272 /* 绿色 */
};

static const uint16_t *const g_vehicle_plate_color_item_texts_u8g2[] = {
    g_vehicle_plate_color_item_1,
    g_vehicle_plate_color_item_2,
    g_vehicle_plate_color_item_3,
    g_vehicle_plate_color_item_4,
    g_vehicle_plate_color_item_5
};

static const uint8_t g_vehicle_plate_color_item_counts_u8g2[] = {
    2, 2, 2, 2, 2
};

static const uint16_t g_lcd_plate_province_codes[] = {
    0x4EAC, /* 京 */
    0x6D25, /* 津 */
    0x6CAA, /* 沪 */
    0x6E1D, /* 渝 */
    0x5180, /* 冀 */
    0x8C6B, /* 豫 */
    0x4E91, /* 云 */
    0x8FBD, /* 辽 */
    0x9ED1, /* 黑 */
    0x6E58, /* 湘 */
    0x7696, /* 皖 */
    0x9C81, /* 鲁 */
    0x65B0, /* 新 */
    0x82CF, /* 苏 */
    0x6D59, /* 浙 */
    0x8D63, /* 赣 */
    0x9102, /* 鄂 */
    0x6842, /* 桂 */
    0x7518, /* 甘 */
    0x664B, /* 晋 */
    0x8499, /* 蒙 */
    0x9655, /* 陕 */
    0x5409, /* 吉 */
    0x95FD, /* 闽 */
    0x8D35, /* 贵 */
    0x7CA4, /* 粤 */
    0x9752, /* 青 */
    0x85CF, /* 藏 */
    0x5DDD, /* 川 */
    0x5B81, /* 宁 */
    0x743C  /* 琼 */
};


static const char g_lcd_plate_letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char g_lcd_plate_tail_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";


static const lcd_ui_list_resource_t g_lcd_list_resources[] = {
    {
        LCD_PAGE_DRIVE_RECORD_MENU,
        g_drive_sub_item_texts_u8g2,
        g_drive_sub_item_counts_u8g2,
        9U,
        g_menu_item_text_1,
        4U
    },
    {
        LCD_PAGE_DEVICE_STATUS_MENU,
        g_device_sub_item_texts_u8g2,
        g_device_sub_item_counts_u8g2,
        9U,
        g_menu_item_text_2,
        4U
    },
    {
        LCD_PAGE_SYSTEM_SETTING_MENU,
        g_system_sub_item_texts_u8g2,
        g_system_sub_item_counts_u8g2,
        9U,
        g_menu_item_text_3,
        4U
    },
    {
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
        g_vehicle_info_setting_item_texts_u8g2,
        g_vehicle_info_setting_item_counts_u8g2,
        6U,
        g_system_sub_item_3,
        6U
    },
    {
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_COLOR,
        g_vehicle_plate_color_item_texts_u8g2,
        g_vehicle_plate_color_item_counts_u8g2,
        5U,
        g_vehicle_info_setting_item_3,
        4U
    },
    {
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_CLASS,
        g_vehicle_plate_class_item_texts_u8g2,
        g_vehicle_plate_class_item_counts_u8g2,
        5U,
        g_vehicle_info_setting_item_4,
        4U
    },
    {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER,
        g_info_center_item_texts_u8g2,
        g_info_center_item_counts_u8g2,
        4U,
        g_drive_sub_item_8,
        4U
    },
    {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT,
        g_info_text_item_texts_u8g2,
        g_info_text_item_counts_u8g2,
        4U,
        g_info_center_item_1,
        4U
    },
    {
        LCD_PAGE_SYSTEM_SETTING_HOST_PARAM,
        g_host_param_item_texts_u8g2,
        g_host_param_item_counts_u8g2,
        4U,
        g_system_sub_item_4,
        6U
    }
};


static void lcd_render_info_center_text_normal_ui(void);
static rt_bool_t lcd_handle_text_normal_keys(void);
static uint16_t lcd_utf8_next_codepoint(const char *s, uint16_t *consumed);
static void lcd_prepare_text_normal_page(void);

static uint8_t g_lcd_load_status_index = 0U;   /* 0=空载 1=半载 2=满载 */
static rt_tick_t g_lcd_load_status_ok_tick = 0U;

#define g_lcd_menu_mode              (*lcd_ui_core_menu_mode_mutable())
#define g_lcd_current_page_id        (*lcd_ui_core_current_page_mutable())
#define g_lcd_page_selected          (lcd_ui_core_page_selected_mutable())
#define g_lcd_page_enter_tick        (*lcd_ui_core_page_enter_tick_mutable())
#define g_lcd_need_redraw            (*lcd_ui_core_need_redraw_mutable())
#define g_lcd_common_ok_return_page  (*lcd_ui_core_common_ok_return_page_mutable())
#define g_lcd_overtime_drive_count    (*lcd_ui_data_get_overtime_drive_count_mutable())
#define g_lcd_total_mileage_km        (*lcd_ui_data_get_total_mileage_km_mutable())
#define g_lcd_total_mileage_rem_m     (*lcd_ui_data_get_total_mileage_rem_m_mutable())

#define LCD_LOCAL_PHONE_FOCUS_NUMBER   0U
#define LCD_LOCAL_PHONE_FOCUS_DIGIT    1U
#define LCD_LOCAL_PHONE_FOCUS_ACTION   2U

static char g_lcd_local_phone_digits[12] = "00000000000";
static uint8_t g_lcd_local_phone_cursor = 0U;
static uint8_t g_lcd_local_phone_selected_digit = 0U;
static uint8_t g_lcd_local_phone_focus = LCD_LOCAL_PHONE_FOCUS_NUMBER;
static uint8_t g_lcd_local_phone_func_index = 1U; /* 0=退出 1=保存 */



static void lcd_render_home_ui(void);
static void lcd_render_home_sub_ui(void);
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
static void lcd_render_key_volume_ui(void);
static void lcd_render_common_config_ok_ui(void);
static void lcd_page_confirm_key_volume(void);
static void lcd_page_confirm_backlight_time(void);
static void lcd_page_confirm_vehicle_plate_class(void);
static void lcd_prepare_vehicle_plate_class_page(void);
static void lcd_page_confirm_vehicle_plate_color(void);
static void lcd_prepare_vehicle_plate_color_page(void);


static void lcd_render_brightness_menu_ui(void);
static void lcd_render_backlight_time_ui(void);
static void lcd_render_brightness_level_ui(void);
static void lcd_page_confirm_brightness_level(void);
static void lcd_render_local_phone_ui(void);
static void lcd_prepare_local_phone_page(void);
static rt_bool_t lcd_handle_local_phone_keys(void);
static void lcd_render_plate_set_ui(void);
static void lcd_prepare_plate_set_page(void);
static rt_bool_t lcd_handle_plate_set_keys(void);
static void lcd_plate_draw_ascii_cell(u8g2_t *u8g2,
                                      uint8_t x,
                                      uint8_t y,
                                      char ch,
                                      rt_bool_t selected);
static void lcd_plate_draw_top_line(u8g2_t *u8g2);
static void lcd_plate_draw_ascii_option(u8g2_t *u8g2,
                                        uint8_t x,
                                        uint8_t y,
                                        char ch,
                                        rt_bool_t selected);
static void lcd_plate_save_and_ok(void);
static void lcd_render_vin_set_ui(void);
static void lcd_prepare_vin_set_page(void);
static rt_bool_t lcd_handle_vin_set_keys(void);
static void lcd_vin_save_and_ok(void);
static void lcd_vin_draw_top_line(u8g2_t *u8g2);

static void lcd_render_province_id_set_ui(void);
static void lcd_prepare_province_id_set_page(void);
static rt_bool_t lcd_handle_province_id_set_keys(void);
static void lcd_province_id_save_and_ok(void);
static void lcd_province_id_draw_top_line(u8g2_t *u8g2);

static void lcd_render_city_id_set_ui(void);
static void lcd_prepare_city_id_set_page(void);
static rt_bool_t lcd_handle_city_id_set_keys(void);
static void lcd_city_id_save_and_ok(void);
static void lcd_city_id_draw_top_line(u8g2_t *u8g2);


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

static uint16_t lcd_utf8_next_codepoint(const char *s, uint16_t *consumed)
{
    /*
     * LCD 绘制用的是 Unicode 码点，SoC 文本缓存里保存的是 UTF-8 字节流。
     * 这里每次从当前位置解出一个字符，同时返回本次消耗的字节数。
     */
    const uint8_t *p = (const uint8_t *)s;

    if ((s == RT_NULL) || (p[0] == '\0')) {
        if (consumed != RT_NULL) {
            *consumed = 0U;
        }
        return 0U;
    }

    if (p[0] < 0x80U) {
        if (consumed != RT_NULL) {
            *consumed = 1U;
        }
        return p[0];
    }

    if (((p[0] & 0xE0U) == 0xC0U) && (p[1] != '\0')) {
        if (consumed != RT_NULL) {
            *consumed = 2U;
        }
        return (uint16_t)(((p[0] & 0x1FU) << 6) |
                          (p[1] & 0x3FU));
    }

    if (((p[0] & 0xF0U) == 0xE0U) &&
        (p[1] != '\0') &&
        (p[2] != '\0')) {
        if (consumed != RT_NULL) {
            *consumed = 3U;
        }
        return (uint16_t)(((p[0] & 0x0FU) << 12) |
                          ((p[1] & 0x3FU) << 6) |
                          (p[2] & 0x3FU));
    }

    /* 不认识的字节序列退化成 '?'，避免坏数据把整页显示拖死。 */
    if (consumed != RT_NULL) {
        *consumed = 1U;
    }
    return '?';
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
    rt_bool_t show_title = RT_TRUE;
    const lcd_page_node_t *page_node;

    page_node = lcd_get_page_node(g_lcd_current_page_id);
    if (page_node != RT_NULL) {
        show_title = page_node->show_title;
    }

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

    if (show_title == RT_TRUE) {
        if (selected_index < 3U) {
            page_start = 0U;
            row_count = (submenu_item_count < 3U) ? submenu_item_count : 3U;

            row_y[0] = 28U;
            row_y[1] = 42U;
            row_y[2] = 56U;

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
    } else {
        if (selected_index < 4U) {
            page_start = 0U;
            row_count = (submenu_item_count < 4U) ? submenu_item_count : 4U;

            row_y[0] = 16U;
            row_y[1] = 30U;
            row_y[2] = 44U;
            row_y[3] = 58U;

            for (row = 0; row < row_count; row++) {
                uint8_t item_index = (uint8_t)(page_start + row);
                lcd_u8g2_draw_submenu_item(u8g2,
                                           item_index,
                                           row_y[row],
                                           (item_index == selected_index) ? RT_TRUE : RT_FALSE);
            }
        } else {
            page_start = 4U;
            row_count = (uint8_t)(submenu_item_count - 4U);
            if (row_count > 4U) {
                row_count = 4U;
            }

            row_y[0] = 16U;
            row_y[1] = 30U;
            row_y[2] = 44U;
            row_y[3] = 58U;

            for (row = 0; row < row_count; row++) {
                uint8_t item_index = (uint8_t)(page_start + row);
                lcd_u8g2_draw_submenu_item(u8g2,
                                           item_index,
                                           row_y[row],
                                           (item_index == selected_index) ? RT_TRUE : RT_FALSE);
            }
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

static void lcd_render_key_volume_ui(void)
{
    u8g2_t *u8g2;
    uint8_t selected_index;
    static const uint16_t g_item_mute[] = {
        0x6309, 0x952E, 0x9759, 0x97F3 /* 按键静音 */
    };
    static const uint16_t g_item_voice[] = {
        0x6309, 0x952E, 0x53D1, 0x97F3 /* 按键发音 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    selected_index = g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME];

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    if (selected_index == 0U) {
        u8g2_DrawBox(u8g2, 0, 11, LCD_COLS, 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 23, g_item_mute, 4);
    u8g2_SetDrawColor(u8g2, 1);

    if (selected_index == 1U) {
        u8g2_DrawBox(u8g2, 0, 31, LCD_COLS, 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 43, g_item_voice, 4);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_port_flush_buffer();
}


static void lcd_render_common_config_ok_ui(void)
{
    u8g2_t *u8g2;
    static const uint16_t g_ok_text[] = {
        0x914D, 0x7F6E, 0x6570, 0x636E, 0x6210, 0x529F, 0xFF01 /* 配置数据成功！ */
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
    lcd_u8g2_draw_unicode_seq(u8g2, 24, 32, g_ok_text, 7);

    u8g2_DrawFrame(u8g2, 46, 46, 40, 14);
    lcd_u8g2_draw_unicode_seq(u8g2, 54, 58, g_confirm_text, 2);

    u8g2_port_flush_buffer();
}

static void lcd_render_brightness_menu_ui(void)
{
    u8g2_t *u8g2;
    uint8_t selected_index;
    static const uint16_t g_item_backlight_time[] = {
        0x80CC, 0x5149, 0x6301, 0x7EED, 0x65F6, 0x95F4, 0x8BBE, 0x7F6E /* 背光持续时间设置 */
    };
    static const uint16_t g_item_brightness[] = {
        0x5C4F, 0x5E55, 0x4EAE, 0x5EA6, 0x8BBE, 0x7F6E /* 屏幕亮度设置 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    selected_index = g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS];

    if (selected_index == 0U) {
        u8g2_DrawBox(u8g2, 0, 11, LCD_COLS, 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 23, g_item_backlight_time, 8);
    u8g2_SetDrawColor(u8g2, 1);

    if (selected_index == 1U) {
        u8g2_DrawBox(u8g2, 0, 31, LCD_COLS, 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 43, g_item_brightness, 6);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_port_flush_buffer();
}

static void lcd_render_backlight_time_ui(void)
{
    u8g2_t *u8g2;
    uint8_t selected_index;
    uint16_t width;
    uint16_t right_x;

    static const uint16_t g_item_off[] = {
        0x5173, 0x95ED /* 关闭 */
    };
    static const uint16_t g_item_15s[] = {
        0x0031, 0x0035, 0x79D2 /* 15秒 */
    };
    static const uint16_t g_item_30s[] = {
        0x0033, 0x0030, 0x79D2 /* 30秒 */
    };
    static const uint16_t g_item_1min[] = {
        0x0031, 0x006D, 0x0069, 0x006E /* 1min */
    };
    static const uint16_t g_item_2min[] = {
        0x0032, 0x006D, 0x0069, 0x006E /* 2min */
    };
    static const uint16_t g_item_always[] = {
        0x5E38, 0x4EAE /* 常亮 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    selected_index = g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_BACKLIGHT_TIME];

    /* 第1行左：关闭 */
    if (selected_index == 0U) {
        u8g2_DrawBox(u8g2, 0, 11, 58, 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 23, g_item_off, 2);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第1行右：15秒 */
    width = lcd_u8g2_get_unicode_seq_width(u8g2, g_item_15s, 3);
    right_x = (uint16_t)(LCD_COLS - 2U - width);
    if (selected_index == 1U) {
        u8g2_DrawBox(u8g2, 66, 11, (uint8_t)(LCD_COLS - 66), 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, right_x, 23, g_item_15s, 3);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第2行左：30秒 */
    if (selected_index == 2U) {
        u8g2_DrawBox(u8g2, 0, 27, 58, 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 39, g_item_30s, 3);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第2行右：1min */
    width = lcd_u8g2_get_unicode_seq_width(u8g2, g_item_1min, 4);
    right_x = (uint16_t)(LCD_COLS - 2U - width);
    if (selected_index == 3U) {
        u8g2_DrawBox(u8g2, 66, 27, (uint8_t)(LCD_COLS - 66), 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, right_x, 39, g_item_1min, 4);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第3行左：2min */
    if (selected_index == 4U) {
        u8g2_DrawBox(u8g2, 0, 43, 58, 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 55, g_item_2min, 4);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第3行右：常亮 */
    width = lcd_u8g2_get_unicode_seq_width(u8g2, g_item_always, 2);
    right_x = (uint16_t)(LCD_COLS - 2U - width);
    if (selected_index == 5U) {
        u8g2_DrawBox(u8g2, 66, 43, (uint8_t)(LCD_COLS - 66), 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, right_x, 55, g_item_always, 2);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_port_flush_buffer();
}

static void lcd_render_brightness_level_ui(void)
{
    u8g2_t *u8g2;
    uint8_t selected_index;
    uint16_t width;
    uint16_t right_x;

    static const uint16_t g_item_10[] = {
        0x0031, 0x0030, 0x0025 /* 10% */
    };
    static const uint16_t g_item_30[] = {
        0x0033, 0x0030, 0x0025 /* 30% */
    };
    static const uint16_t g_item_50[] = {
        0x0035, 0x0030, 0x0025 /* 50% */
    };
    static const uint16_t g_item_70[] = {
        0x0037, 0x0030, 0x0025 /* 70% */
    };
    static const uint16_t g_item_100[] = {
        0x0031, 0x0030, 0x0030, 0x0025 /* 100% */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();

    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    selected_index = g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS_LEVEL];

    /* 第1行左：10% */
    if (selected_index == 0U) {
        u8g2_DrawBox(u8g2, 0, 11, 58, 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 23, g_item_10, 3);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第1行右：30% */
    width = lcd_u8g2_get_unicode_seq_width(u8g2, g_item_30, 3);
    right_x = (uint16_t)(LCD_COLS - 2U - width);
    if (selected_index == 1U) {
        u8g2_DrawBox(u8g2, 66, 11, (uint8_t)(LCD_COLS - 66), 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, right_x, 23, g_item_30, 3);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第2行左：50% */
    if (selected_index == 2U) {
        u8g2_DrawBox(u8g2, 0, 27, 58, 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 39, g_item_50, 3);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第2行右：70% */
    width = lcd_u8g2_get_unicode_seq_width(u8g2, g_item_70, 3);
    right_x = (uint16_t)(LCD_COLS - 2U - width);
    if (selected_index == 3U) {
        u8g2_DrawBox(u8g2, 66, 27, (uint8_t)(LCD_COLS - 66), 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, right_x, 39, g_item_70, 3);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第3行：100% */
    if (selected_index == 4U) {
        u8g2_DrawBox(u8g2, 0, 43, LCD_COLS, 15);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 55, g_item_100, 4);
    u8g2_SetDrawColor(u8g2, 1);

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
                                    (state->wk_acc != 0U) ? RT_TRUE : RT_FALSE);
        lcd_vehicle_draw_ascii_right(u8g2, 14, "ON",
                                     (state->wk_on != 0U) ? RT_TRUE : RT_FALSE);

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

static const lcd_page_id_t g_brightness_setting_children[] = {
    LCD_PAGE_SYSTEM_SETTING_BACKLIGHT_TIME,
    LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS_LEVEL
};

static const lcd_page_id_t g_vehicle_info_children[] = {
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_VIN,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_SET,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_COLOR,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_CLASS,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PROVINCE_ID,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_CITY_ID
};

static const lcd_page_id_t g_host_param_children[] = {
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_LOCAL_PHONE,
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SOS_PHONE,
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SERVER1,
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SERVER2
};



static const lcd_page_node_t g_lcd_pages[LCD_PAGE_MAX] = {
    [LCD_PAGE_HOME] = {
        LCD_PAGE_HOME, LCD_PAGE_MAX, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, RT_FALSE, lcd_render_home_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_MAIN_MENU] = {
        LCD_PAGE_MAIN_MENU, LCD_PAGE_HOME, LCD_PAGE_KIND_LIST,
        g_main_menu_children, 3U, 3U, RT_TRUE, lcd_render_menu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_MENU] = {
        LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_MAIN_MENU, LCD_PAGE_KIND_LIST,
        g_drive_record_children, 9U, 9U, RT_TRUE, lcd_render_drive_record_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DEVICE_STATUS_MENU] = {
        LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_MAIN_MENU, LCD_PAGE_KIND_LIST,
        g_device_status_children, 9U, 9U, RT_TRUE, lcd_render_drive_record_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_MENU] = {
        LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_MAIN_MENU, LCD_PAGE_KIND_LIST,
        g_system_setting_children, 9U, 9U, RT_TRUE, lcd_render_drive_record_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_FATIGUE] = {
        LCD_PAGE_DRIVE_RECORD_FATIGUE, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, RT_FALSE, lcd_render_fatigue_drive_record_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_LOCATION] = {
        LCD_PAGE_DRIVE_RECORD_LOCATION, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, RT_FALSE, lcd_render_location_status_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_MILEAGE] = {
        LCD_PAGE_DRIVE_RECORD_MILEAGE, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, RT_FALSE, lcd_render_drive_mileage_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_DRIVER_INFO] = {
        LCD_PAGE_DRIVE_RECORD_DRIVER_INFO, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, RT_FALSE, lcd_render_driver_info_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_VEHICLE_INFO] = {
        LCD_PAGE_DRIVE_RECORD_VEHICLE_INFO, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, RT_FALSE, lcd_render_vehicle_info_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_LOAD_STATUS] = {
        LCD_PAGE_DRIVE_RECORD_LOAD_STATUS, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_LIST,
        RT_NULL, 0U, 3U, RT_FALSE, lcd_render_vehicle_load_status_ui, lcd_page_confirm_load_status, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_LOAD_STATUS_OK] = {
        LCD_PAGE_DRIVE_RECORD_LOAD_STATUS_OK, LCD_PAGE_DRIVE_RECORD_LOAD_STATUS, LCD_PAGE_KIND_ACTION_RESULT,
        RT_NULL, 0U, 0U, RT_FALSE, lcd_render_vehicle_load_status_ok_ui, RT_NULL, 3000U, LCD_PAGE_DRIVE_RECORD_LOAD_STATUS
    },
    [LCD_PAGE_DRIVE_RECORD_EXPORT] = {
        LCD_PAGE_DRIVE_RECORD_EXPORT, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DRIVE_RECORD_INFO_CENTER] = {
        LCD_PAGE_DRIVE_RECORD_INFO_CENTER,
        LCD_PAGE_DRIVE_RECORD_MENU,
        LCD_PAGE_KIND_LIST,
        g_info_center_children,
        4U,
        4U,
        RT_TRUE,
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
        RT_FALSE,
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
        RT_FALSE,
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
        RT_FALSE,
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
        RT_TRUE,
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
        RT_FALSE,
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
        RT_FALSE,
        lcd_render_info_center_text_normal_ui,
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
        RT_FALSE,
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
        RT_FALSE,
        lcd_render_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },

    [LCD_PAGE_DRIVE_RECORD_VIN] = {
        LCD_PAGE_DRIVE_RECORD_VIN, LCD_PAGE_DRIVE_RECORD_MENU, LCD_PAGE_KIND_VIEW,
        RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX
    },
    [LCD_PAGE_DEVICE_STATUS_VERSION] = { LCD_PAGE_DEVICE_STATUS_VERSION, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_device_version_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_GFRS] = { LCD_PAGE_DEVICE_STATUS_GFRS, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_SELF_TEST] = { LCD_PAGE_DEVICE_STATUS_SELF_TEST, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_LOCATION_MODULE] = { LCD_PAGE_DEVICE_STATUS_LOCATION_MODULE, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_VEHICLE] = { LCD_PAGE_DEVICE_STATUS_VEHICLE, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_LIST, RT_NULL, 0U, 3U, RT_FALSE, lcd_render_vehicle_status_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_STORAGE] = { LCD_PAGE_DEVICE_STATUS_STORAGE, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_AV] = { LCD_PAGE_DEVICE_STATUS_AV, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_SPEED] = { LCD_PAGE_DEVICE_STATUS_SPEED, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_DEVICE_STATUS_DRIVER_MONITOR] = { LCD_PAGE_DEVICE_STATUS_DRIVER_MONITOR, LCD_PAGE_DEVICE_STATUS_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME] = {
        LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME,
        LCD_PAGE_SYSTEM_SETTING_MENU,
        LCD_PAGE_KIND_LIST,
        RT_NULL,
        0U,
        2U,
        RT_FALSE,
        lcd_render_key_volume_ui,
        lcd_page_confirm_key_volume,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_COMMON_CONFIG_OK] = {
        LCD_PAGE_COMMON_CONFIG_OK,
        LCD_PAGE_MAX,
        LCD_PAGE_KIND_ACTION_RESULT,
        RT_NULL,
        0U,
        0U,
        RT_FALSE,
        lcd_render_common_config_ok_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },


    [LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS] = {
        LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS,
        LCD_PAGE_SYSTEM_SETTING_MENU,
        LCD_PAGE_KIND_LIST,
        g_brightness_setting_children,
        2U,
        2U,
        RT_FALSE,
        lcd_render_brightness_menu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_BACKLIGHT_TIME] = {
        LCD_PAGE_SYSTEM_SETTING_BACKLIGHT_TIME,
        LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS,
        LCD_PAGE_KIND_LIST,
        RT_NULL,
        0U,
        6U,
        RT_FALSE,
        lcd_render_backlight_time_ui,
        lcd_page_confirm_backlight_time,
        0U,
        LCD_PAGE_MAX
    },


    [LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS_LEVEL] = {
        LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS_LEVEL,
        LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS,
        LCD_PAGE_KIND_LIST,
        RT_NULL,
        0U,
        5U,
        RT_FALSE,
        lcd_render_brightness_level_ui,
        lcd_page_confirm_brightness_level,
        0U,
        LCD_PAGE_MAX
    },


    [LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO] = {
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
        LCD_PAGE_SYSTEM_SETTING_MENU,
        LCD_PAGE_KIND_LIST,
        g_vehicle_info_children,
        6U,
        6U,
        RT_FALSE,
        lcd_render_drive_record_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },

    [LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_VIN] = {
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_VIN,
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        RT_FALSE,
        lcd_render_vin_set_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_SET] = {
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_SET,
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        RT_FALSE,
        lcd_render_plate_set_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_COLOR] = {
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_COLOR,
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
        LCD_PAGE_KIND_LIST,
        RT_NULL,
        0U,
        5U,
        RT_FALSE,
        lcd_render_drive_record_submenu_ui,
        lcd_page_confirm_vehicle_plate_color,
        0U,
        LCD_PAGE_MAX
    },


    [LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_CLASS] = {
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_CLASS,
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
        LCD_PAGE_KIND_LIST,
        RT_NULL,
        0U,
        5U,
        RT_FALSE,
        lcd_render_drive_record_submenu_ui,
        lcd_page_confirm_vehicle_plate_class,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PROVINCE_ID] = {
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PROVINCE_ID,
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        RT_FALSE,
        lcd_render_province_id_set_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_CITY_ID] = {
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_CITY_ID,
        LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        RT_FALSE,
        lcd_render_city_id_set_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },


    [LCD_PAGE_SYSTEM_SETTING_HOST_PARAM] = {
        LCD_PAGE_SYSTEM_SETTING_HOST_PARAM,
        LCD_PAGE_SYSTEM_SETTING_MENU,
        LCD_PAGE_KIND_LIST,
        g_host_param_children,
        4U,
        4U,
        RT_FALSE,
        lcd_render_drive_record_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_LOCAL_PHONE] = {
        LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_LOCAL_PHONE,
        LCD_PAGE_SYSTEM_SETTING_HOST_PARAM,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        RT_FALSE,
        lcd_render_local_phone_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SOS_PHONE] = {
        LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SOS_PHONE,
        LCD_PAGE_SYSTEM_SETTING_HOST_PARAM,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        RT_FALSE,
        lcd_render_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SERVER1] = {
        LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SERVER1,
        LCD_PAGE_SYSTEM_SETTING_HOST_PARAM,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        RT_FALSE,
        lcd_render_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },
    [LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SERVER2] = {
        LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SERVER2,
        LCD_PAGE_SYSTEM_SETTING_HOST_PARAM,
        LCD_PAGE_KIND_VIEW,
        RT_NULL,
        0U,
        0U,
        RT_FALSE,
        lcd_render_submenu_ui,
        RT_NULL,
        0U,
        LCD_PAGE_MAX
    },





    [LCD_PAGE_SYSTEM_SETTING_INIT_MILEAGE] = { LCD_PAGE_SYSTEM_SETTING_INIT_MILEAGE, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_REGISTER] = { LCD_PAGE_SYSTEM_SETTING_REGISTER, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_UNREGISTER] = { LCD_PAGE_SYSTEM_SETTING_UNREGISTER, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_REC_VER] = { LCD_PAGE_SYSTEM_SETTING_REC_VER, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX },
    [LCD_PAGE_SYSTEM_SETTING_COMPONENT_VER] = { LCD_PAGE_SYSTEM_SETTING_COMPONENT_VER, LCD_PAGE_SYSTEM_SETTING_MENU, LCD_PAGE_KIND_VIEW, RT_NULL, 0U, 0U, RT_FALSE, lcd_render_submenu_ui, RT_NULL, 0U, LCD_PAGE_MAX }

};

static void lcd_page_on_enter_prepare(lcd_page_id_t page_id)
{
    if (page_id == LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_LOCAL_PHONE) {
        lcd_prepare_local_phone_page();
    }

    if (page_id == LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_CLASS) {
        lcd_prepare_vehicle_plate_class_page();
    }
    if (page_id == LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_COLOR) {
        lcd_prepare_vehicle_plate_color_page();
    }

    if (page_id == LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_NORMAL) {
        lcd_prepare_text_normal_page();
    }
    if (page_id == LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_SET) {
        lcd_prepare_plate_set_page();
    }
    if (page_id == LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_VIN) {
        lcd_prepare_vin_set_page();
    }
    if (page_id == LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PROVINCE_ID) {
        lcd_prepare_province_id_set_page();
    }
    if (page_id == LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_CITY_ID) {
        lcd_prepare_city_id_set_page();
    }



}

static void lcd_prepare_local_phone_page(void)
{
    svc_storage_phone_t phone;

    if (svc_storage_load_local_phone(&phone) == RT_TRUE) {
        rt_memcpy(g_lcd_local_phone_digits, phone.digits, sizeof(g_lcd_local_phone_digits));
    } else {
        rt_memcpy(g_lcd_local_phone_digits, "00000000000", sizeof(g_lcd_local_phone_digits));
    }

    g_lcd_local_phone_cursor = 0U;
    g_lcd_local_phone_focus = LCD_LOCAL_PHONE_FOCUS_NUMBER;
    g_lcd_local_phone_func_index = 1U;
    g_lcd_local_phone_selected_digit = (uint8_t)(g_lcd_local_phone_digits[0] - '0');
}

static void lcd_prepare_vehicle_plate_class_page(void)
{
    svc_storage_plate_class_t plate_class;
    uint8_t selected = 0U;

    if (svc_storage_load_plate_class(&plate_class) == RT_TRUE) {
        if ((plate_class.plate_class >= 1U) && (plate_class.plate_class <= 5U)) {
            selected = (uint8_t)(plate_class.plate_class - 1U);
        }
    }

    g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_CLASS] = selected;
}

static void lcd_prepare_vehicle_plate_color_page(void)
{
    svc_storage_plate_color_t plate_color;
    uint8_t selected = 0U;

    if (svc_storage_load_plate_color(&plate_color) == RT_TRUE) {
        if ((plate_color.plate_color >= 1U) && (plate_color.plate_color <= 5U)) {
            selected = (uint8_t)(plate_color.plate_color - 1U);
        }
    }

    g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_COLOR] = selected;
}


static void lcd_page_confirm_load_status(void)
{
    lcd_page_enter(LCD_PAGE_DRIVE_RECORD_LOAD_STATUS_OK);
}

static void lcd_page_confirm_key_volume(void)
{
    /*
     * 这里后续如果你要真正保存“静音/发音”配置，
     * 就根据 g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME]
     * 去写配置变量或 EEPROM。
     */
    lcd_page_enter_common_ok(LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME);
}

static void lcd_page_confirm_backlight_time(void)
{
    /*
     * 这里后续根据
     * g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_BACKLIGHT_TIME]
     * 去写背光持续时间配置和 EEPROM。
     */
    lcd_page_enter_common_ok(LCD_PAGE_SYSTEM_SETTING_BACKLIGHT_TIME);
}

static void lcd_page_confirm_brightness_level(void)
{
    /*
     * 后续根据
     * g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS_LEVEL]
     * 去写亮度配置和 EEPROM。
     */
    lcd_page_enter_common_ok(LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS_LEVEL);
}

static void lcd_page_confirm_vehicle_plate_class(void)
{
    svc_storage_plate_class_t plate_class;

    plate_class.plate_class =
        (uint8_t)(g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_CLASS] + 1U);

    if (svc_storage_save_plate_class(&plate_class) == RT_TRUE) {
        app_usart_cmd_refresh_status_config();
        lcd_page_enter_common_ok(LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_CLASS);
    }

}

static void lcd_page_confirm_vehicle_plate_color(void)
{
    svc_storage_plate_color_t plate_color;

    plate_color.plate_color =
        (uint8_t)(g_lcd_page_selected[LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_COLOR] + 1U);

    if (svc_storage_save_plate_color(&plate_color) == RT_TRUE) {
        app_usart_cmd_refresh_status_config();
        lcd_page_enter_common_ok(LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_COLOR);
    }

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

static void lcd_render_info_center_text_normal_ui(void)
{
    /*
     * 文本信息页不做复杂自动排版，只按固定“3 行 x 10 字符”分页。
     * 这样实现简单，内存占用也稳定，适合当前这块小屏。
     */
    u8g2_t *u8g2;
    uint16_t i;
    uint16_t offset;
    uint16_t consumed;
    uint16_t code;
    uint16_t page_start_char;
    uint16_t current_char_index;
    uint8_t line;
    uint16_t x;
    uint16_t y;
    uint8_t line_chars;
    char page_str[16];

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

    if ((g_lcd_text_msg.valid == 0U) || (g_lcd_text_msg.text_len == 0U)) {
        lcd_u8g2_draw_unicode_seq(u8g2, 2, 28, g_no_text_text, 3);
        u8g2_port_flush_buffer();
        return;
    }

    /* 一页 3 行，每行最多 10 个字符，先按字符索引跳到页起点 */
    /* 先跳到当前页起点，再从这里开始逐行绘制。 */
    page_start_char = (uint16_t)(g_lcd_text_msg.page_index *
                                 LCD_TEXT_MSG_LINES_PER_PAGE *
                                 LCD_TEXT_MSG_LINE_MAX_CHARS);

    offset = 0U;
    current_char_index = 0U;

    while ((g_lcd_text_msg.text[offset] != '\0') &&
           (current_char_index < page_start_char)) {
        code = lcd_utf8_next_codepoint(&g_lcd_text_msg.text[offset], &consumed);
        if ((code == 0U) || (consumed == 0U)) {
            break;
        }
        offset = (uint16_t)(offset + consumed);
        current_char_index++;
    }

    for (line = 0U; line < LCD_TEXT_MSG_LINES_PER_PAGE; line++) {
        x = 2U;
        y = (uint16_t)(16U + line * 16U);
        line_chars = 0U;

        while ((g_lcd_text_msg.text[offset] != '\0') &&
               (line_chars < LCD_TEXT_MSG_LINE_MAX_CHARS)) {
            code = lcd_utf8_next_codepoint(&g_lcd_text_msg.text[offset], &consumed);
            if ((code == 0U) || (consumed == 0U)) {
                break;
            }

            /* 换行符只负责换行，不直接画到屏幕上。 */
            if ((code == '\r') || (code == '\n')) {
                offset = (uint16_t)(offset + consumed);
                break;
            }

            x += u8g2_DrawGlyph(u8g2, x, y, code);
            offset = (uint16_t)(offset + consumed);
            line_chars++;

        }
    }

    /* 重新估算总页数 */
    /* 重新扫描总字符数，用来给右下角页码计数。 */
    current_char_index = 0U;
    offset = 0U;
    while (g_lcd_text_msg.text[offset] != '\0') {
        code = lcd_utf8_next_codepoint(&g_lcd_text_msg.text[offset], &consumed);
        if ((code == 0U) || (consumed == 0U)) {
            break;
        }
        offset = (uint16_t)(offset + consumed);
        current_char_index++;
    }

    g_lcd_text_msg.page_count = (uint16_t)((current_char_index +
        (LCD_TEXT_MSG_LINES_PER_PAGE * LCD_TEXT_MSG_LINE_MAX_CHARS) - 1U) /
        (LCD_TEXT_MSG_LINES_PER_PAGE * LCD_TEXT_MSG_LINE_MAX_CHARS));

    if (g_lcd_text_msg.page_count == 0U) {
        g_lcd_text_msg.page_count = 1U;
    }

    rt_snprintf(page_str, sizeof(page_str), "%u/%u",
                (unsigned int)(g_lcd_text_msg.page_index + 1U),
                (unsigned int)g_lcd_text_msg.page_count);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 96, 62, page_str);

    u8g2_port_flush_buffer();
}

static rt_bool_t lcd_handle_text_normal_keys(void)
{
    if (g_lcd_current_page_id != LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_NORMAL) {
        return RT_FALSE;
    }

    if (svc_adc_consume_s1_event() == RT_TRUE) {
        lcd_page_enter(LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT);
        return RT_TRUE;
    }

    if (svc_adc_consume_s2_event() == RT_TRUE) {
        if (g_lcd_text_msg.page_index > 0U) {
            g_lcd_text_msg.page_index--;
            g_lcd_need_redraw = RT_TRUE;
        }
        return RT_TRUE;
    }

    if (svc_adc_consume_s3_event() == RT_TRUE) {
        if ((g_lcd_text_msg.page_count > 0U) &&
            ((g_lcd_text_msg.page_index + 1U) < g_lcd_text_msg.page_count)) {
            g_lcd_text_msg.page_index++;
            g_lcd_need_redraw = RT_TRUE;
        }
        return RT_TRUE;
    }

    if (svc_adc_consume_s4_event() == RT_TRUE) {
        return RT_TRUE;
    }

    return RT_TRUE;
}


/*
 * ============================================================
 *  主页数据更新接口
 * ============================================================
 *
 * 这些函数由其他服务模块(如 GPS 定位、速度采集等)调用,
 * 用于更新 LCD 主页的显示数据。
 *
 * 更新策略:
 *   每个函数更新对应字段, 同时保留其他字段不变。
 *   通过 lcd_home_ui_set_data() 内部的比较逻辑,
 *   只有在数据真正变化时才触发重绘。
 *
 * 为什么不在每个 update 函数中都设置 g_lcd_need_redraw?
 *   lcd_home_ui_set_data() 内部已经处理了变化检测和重绘标志。
 *   只有 svc_lcd_update_top_status 和 svc_lcd_update_home_location
 *   是直接修改字段并检查是否需要重绘的。
 *
 * 线程安全性:
 *   这些函数可能在多个线程(如 GPS 线程、CAN 线程)中被调用,
 *   LCD 线程也在读取这些数据。由于都是对 uint8/uint16/uint32
 *   的单次写入, 在 Cortex-M 上是原子操作, 不需要加锁。
 */

/** 更新时间 (时/分/秒) */
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

/** 更新顶部状态栏 (GPS/卡状态/记录状态等的位图) */
void svc_lcd_update_top_status(uint8_t value)
{
    if (g_lcd_home_ui.top_status_value != value) {
        g_lcd_home_ui.top_status_value = value;

        /* 只在主页显示时触发重绘 */
        if (g_lcd_menu_mode == RT_FALSE) {
            g_lcd_need_redraw = RT_TRUE;
        }
    }
}

/** 更新速度 (×10, 单位 0.1 km/h) */
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

/** 更新行驶时间 (从行程开始累计) */
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

/** 更新驾驶员卡号 (20 位数字, 不插卡时全 0) */
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

void svc_lcd_update_home_location(uint32_t latitude,
                                  uint32_t longitude,
                                  uint8_t latitude_direction,
                                  uint8_t longitude_direction,
                                  uint32_t timestamp)
{
    rt_bool_t changed = RT_FALSE;

    if (g_lcd_home_ui.latitude != latitude) {
        g_lcd_home_ui.latitude = latitude;
        changed = RT_TRUE;
    }

    if (g_lcd_home_ui.longitude != longitude) {
        g_lcd_home_ui.longitude = longitude;
        changed = RT_TRUE;
    }

    if (g_lcd_home_ui.latitude_direction != latitude_direction) {
        g_lcd_home_ui.latitude_direction = latitude_direction;
        changed = RT_TRUE;
    }

    if (g_lcd_home_ui.longitude_direction != longitude_direction) {
        g_lcd_home_ui.longitude_direction = longitude_direction;
        changed = RT_TRUE;
    }

    if (g_lcd_home_ui.timestamp != timestamp) {
        g_lcd_home_ui.timestamp = timestamp;
        changed = RT_TRUE;
    }

    if ((changed == RT_TRUE) && (g_lcd_current_page_id == LCD_PAGE_HOME)) {
        g_lcd_need_redraw = RT_TRUE;
    }
}

void svc_lcd_update_text_message(uint8_t flag,
                                 uint8_t text_type,
                                 const char *utf8_text,
                                 uint16_t text_len)
{
    uint16_t copy_len;

    if (utf8_text == RT_NULL) {
        return;
    }

    /*
     * LCD 侧只缓存“最近一条”文本消息。
     * 长度超过显示缓存上限时直接截断，避免页面层再处理越界。
     */
    if (text_len > LCD_TEXT_MSG_MAX_LEN) {
        copy_len = LCD_TEXT_MSG_MAX_LEN;
    } else {
        copy_len = text_len;
    }

    g_lcd_text_msg.valid = 1U;
    g_lcd_text_msg.flag = flag;
    g_lcd_text_msg.text_type = text_type;
    g_lcd_text_msg.text_len = copy_len;
    rt_memcpy(g_lcd_text_msg.text, utf8_text, copy_len);
    g_lcd_text_msg.text[copy_len] = '\0';
    g_lcd_text_msg.page_index = 0U;
    g_lcd_text_msg.page_count = 0U;

    if (g_lcd_current_page_id == LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_NORMAL) {
        g_lcd_need_redraw = RT_TRUE;
    }
}

static void lcd_prepare_text_normal_page(void)
{
    g_lcd_text_msg.page_index = 0U;
    g_lcd_text_msg.page_count = 1U;
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

    if (g_lcd_home_subpage != 0U) {
        lcd_render_home_sub_ui();
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

static void lcd_render_home_sub_ui(void)
{
    u8g2_t *u8g2;
    char lat_str[24];
    char lon_str[24];
    char date_time_str[24];
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    char lat_prefix[2] = "";
    char lon_prefix[2] = "";

    static const uint16_t g_lat_text[] = {
        0x7EAC, 0x5EA6, 0xFF1A /* 纬度： */
    };
    static const uint16_t g_lon_text[] = {
        0x7ECF, 0x5EA6, 0xFF1A /* 经度： */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第一行状态栏保持不变 */
    lcd_u8g2_draw_top_icons(u8g2, 2, 2);

    if (g_lcd_home_ui.latitude_direction != 0U) {
        lat_prefix[0] = '-';
        lat_prefix[1] = '\0';
    }

    if (g_lcd_home_ui.longitude_direction != 0U) {
        lon_prefix[0] = '-';
        lon_prefix[1] = '\0';
    }

    rt_snprintf(lat_str, sizeof(lat_str), "%s%lu.%06lu",
                lat_prefix,
                (unsigned long)(g_lcd_home_ui.latitude / 1000000U),
                (unsigned long)(g_lcd_home_ui.latitude % 1000000U));

    rt_snprintf(lon_str, sizeof(lon_str), "%s%lu.%06lu",
                lon_prefix,
                (unsigned long)(g_lcd_home_ui.longitude / 1000000U),
                (unsigned long)(g_lcd_home_ui.longitude % 1000000U));

    lcd_timestamp_to_local_ymdhm(g_lcd_home_ui.timestamp,
                                 &year, &month, &day, &hour, &minute);

    rt_snprintf(date_time_str, sizeof(date_time_str), "%u.%u.%u    %02u:%02u",
                (unsigned int)year,
                (unsigned int)month,
                (unsigned int)day,
                (unsigned int)hour,
                (unsigned int)minute);

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 24, g_lat_text, 3);
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 40, g_lon_text, 3);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 40, 24, lat_str);
    u8g2_DrawStr(u8g2, 40, 40, lon_str);
    u8g2_DrawStr(u8g2, 8, 56, date_time_str);

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




/*
 * ============================================================
 *  svc_lcd_init — LCD 子系统初始化
 * ============================================================
 *
 * 初始化顺序 (有严格的依赖关系):
 *   1) lcd_drv_init()       ← 必须先初始化硬件 (GPIO + SPI + ST7567)
 *   2) lcd_ui_data_reset()  ← 重置显示数据 (默认时间 09:16:45)
 *   3) 注册列表资源、页面节点、钩子、fallback 渲染器
 *   4) u8g2_port_init()     ← 初始化 u8g2 图形库 (依赖硬件就绪)
 *
 * 注意: lcd_drv_init() 内部已经做了清屏和打开显示,
 *       所以不需要额外调用 lcd_fb_public_clear/flush
 */
int svc_lcd_init(void)
{
    lcd_drv_init();                          /* 初始化硬件驱动 */
    lcd_ui_data_reset();                     /* 重置显示数据 */
    lcd_ui_list_register(g_lcd_list_resources,              /* 注册菜单列表资源 */
                         (uint8_t)(sizeof(g_lcd_list_resources) / sizeof(g_lcd_list_resources[0])));
    lcd_ui_pages_register(g_lcd_pages, LCD_PAGE_MAX);       /* 注册页面节点 */
    lcd_ui_nav_set_enter_hook(lcd_page_on_enter_prepare);   /* 注册页面进入钩子 */
    lcd_ui_render_set_fallback(lcd_render_submenu_ui);      /* 注册通用渲染器 */
    u8g2_port_init();                        /* 初始化 u8g2 图形库 */
    APP_NON_CAN_LOG("LCD ST7567 init done\r\n");
    return RT_EOK;
}


/*
 * ============================================================
 *  svc_lcd_thread_entry — LCD UI 线程入口 (核心事件循环)
 * ============================================================
 *
 * 这个线程是 LCD 模块的"大脑", 负责:
 *   1) 显示开机自检页面 5 秒
 *   2) 进入主循环, 持续处理按键事件
 *   3) 驱动页面刷新
 *
 * 事件处理顺序 (优先级从高到低):
 *   1) 主页按键 (S1~S4)
 *   2) 文本消息按键 (lcd_handle_text_normal_keys)
 *   3) 车牌编辑按键 (lcd_handle_plate_set_keys)
 *   4) 电话号码编辑按键 (lcd_handle_local_phone_keys)
 *   5) 通用方向/确认/返回键
 *
 * 为什么用 if-else 链而不是统一接收所有按键?
 *   不同页面模式下按键的含义不同。例如:
 *   - 主页: S1/S4 切换子页面, S4 进入菜单
 *   - 编辑页面: S1/S4 修改值, S2/S3 在字段间移动
 *   所以按键处理必须与当前页面 ID 绑定。
 *
 * 刷新策略:
 *   每次按键处理后立即渲染, 确保响应迅速。
 *   g_lcd_need_redraw 标志控制是否需要重绘。
 */
static void svc_lcd_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    lcd_backlight_on();                     /* 打开背光 */

    lcd_ui_core_reset();                    /* 重置 UI 到主页 */

    APP_NON_CAN_LOG("LCD: ui thread start\r\n");

    /* 等待 ADC 初始化完成 (ADC 驱动初始化和按键消抖) */
    rt_thread_mdelay(APP_ADC_STARTUP_DELAY_MS + 100);

    /* === 显示开机自检画面, 5 秒后进入主页 === */
    lcd_render_boot_check_ui();
    rt_thread_mdelay(5000);

    g_lcd_need_redraw = RT_TRUE;

    while (1)
    {

        if (g_lcd_current_page_id == LCD_PAGE_HOME) {
            if (svc_adc_consume_s1_event() == RT_TRUE) {
                rt_thread_mdelay(10);
                continue;
            }

            if ((svc_adc_consume_s2_event() == RT_TRUE) ||
                (svc_adc_consume_s3_event() == RT_TRUE)) {
                g_lcd_home_subpage = (g_lcd_home_subpage == 0U) ? 1U : 0U;
                g_lcd_need_redraw = RT_TRUE;

                lcd_page_handle_auto_return();
                lcd_page_handle_dynamic_refresh();

                if (g_lcd_need_redraw == RT_TRUE) {
                    lcd_render_current_page();
                    g_lcd_need_redraw = RT_FALSE;
                }

                rt_thread_mdelay(10);
                continue;
            }

            if (svc_adc_consume_s4_event() == RT_TRUE) {
                lcd_page_enter(LCD_PAGE_MAIN_MENU);

                lcd_page_handle_auto_return();
                lcd_page_handle_dynamic_refresh();

                if (g_lcd_need_redraw == RT_TRUE) {
                    lcd_render_current_page();
                    g_lcd_need_redraw = RT_FALSE;
                }

                rt_thread_mdelay(10);
                continue;
            }
        }
        if (lcd_handle_text_normal_keys() == RT_TRUE) {
            lcd_page_handle_auto_return();
            lcd_page_handle_dynamic_refresh();

            if (g_lcd_need_redraw == RT_TRUE) {
                lcd_render_current_page();
                g_lcd_need_redraw = RT_FALSE;
            }

            rt_thread_mdelay(10);
            continue;
        }

        if (lcd_handle_vin_set_keys() == RT_TRUE) {
            lcd_page_handle_auto_return();
            lcd_page_handle_dynamic_refresh();

            if (g_lcd_need_redraw == RT_TRUE) {
                lcd_render_current_page();
                g_lcd_need_redraw = RT_FALSE;
            }

            rt_thread_mdelay(10);
            continue;
        }

        if (lcd_handle_province_id_set_keys() == RT_TRUE) {
            lcd_page_handle_auto_return();
            lcd_page_handle_dynamic_refresh();

            if (g_lcd_need_redraw == RT_TRUE) {
                lcd_render_current_page();
                g_lcd_need_redraw = RT_FALSE;
            }

            rt_thread_mdelay(10);
            continue;
        }

        if (lcd_handle_city_id_set_keys() == RT_TRUE) {
            lcd_page_handle_auto_return();
            lcd_page_handle_dynamic_refresh();

            if (g_lcd_need_redraw == RT_TRUE) {
                lcd_render_current_page();
                g_lcd_need_redraw = RT_FALSE;
            }

            rt_thread_mdelay(10);
            continue;
        }

        if (lcd_handle_plate_set_keys() == RT_TRUE) {
            lcd_page_handle_auto_return();
            lcd_page_handle_dynamic_refresh();

            if (g_lcd_need_redraw == RT_TRUE) {
                lcd_render_current_page();
                g_lcd_need_redraw = RT_FALSE;
            }

            rt_thread_mdelay(10);
            continue;
        }


        if (lcd_handle_local_phone_keys() == RT_TRUE) {
            lcd_page_handle_auto_return();
            lcd_page_handle_dynamic_refresh();

            if (g_lcd_need_redraw == RT_TRUE) {
                lcd_render_current_page();
                g_lcd_need_redraw = RT_FALSE;
            }

            rt_thread_mdelay(10);
            continue;
        }


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



/*
 * ============================================================
 *  svc_lcd_task_start — 创建 LCD UI 线程
 * ============================================================
 *
 * 线程参数:
 *   栈大小: APP_LCD_TASK_STACK_SIZE (在 app_config.h 配置)
 *   优先级: APP_LCD_TASK_PRIORITY (通常 18, 中等偏低)
 *   时间片: APP_LCD_TASK_TICK
 *
 * 为什么 LCD 线程优先级较低 (18)?
 *   LCD 刷新涉及大量的 u8g2 渲染和 SPI 通信, 耗时较长。
 *   如果优先级太高, 会抢占 CAN 通信等关键任务。
 *   因为人眼对 LCD 刷新频率不敏感 (50ms 刷新就足够流畅),
 *   所以 LCD 任务可以放在低优先级。
 */
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

static void lcd_render_local_phone_ui(void)
{
    u8g2_t *u8g2;
    uint8_t i;
    uint8_t cursor_x;
    char phone_str[12];

    static const uint16_t g_title_text[] = {
        0x672C, 0x673A, 0x53F7, 0x7801, 0xFF1A /* 本机号码： */
    };
    static const uint16_t g_exit_text[] = {
        0x9000, 0x51FA /* 退出 */
    };
    static const uint16_t g_save_text[] = {
        0x4FDD, 0x5B58 /* 保存 */
    };

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    rt_memcpy(phone_str, g_lcd_local_phone_digits, sizeof(phone_str));

    u8g2_port_clear_buffer();
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    /* 第一行：本机号码：+ 11位号码 */
    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 12, g_title_text, 5);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    u8g2_DrawStr(u8g2, 62, 12, phone_str);

    /* 第一行始终显示当前编辑位 */
    cursor_x = (uint8_t)(62U + g_lcd_local_phone_cursor * 6U);

    if (g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_NUMBER) {
        u8g2_DrawBox(u8g2, cursor_x - 1U, 2U, 8U, 12U);
        u8g2_SetDrawColor(u8g2, 0);

        {
            char s[2];
            s[0] = g_lcd_local_phone_digits[g_lcd_local_phone_cursor];
            s[1] = '\0';
            u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
            u8g2_DrawStr(u8g2, cursor_x, 12, s);
        }

        u8g2_SetDrawColor(u8g2, 1);
    } else {
        u8g2_DrawFrame(u8g2, cursor_x - 1U, 2U, 8U, 12U);
    }


    /* 第二行：0123456789 */
    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    for (i = 0U; i < 10U; i++) {
        uint8_t x = (uint8_t)(4U + i * 12U);

        if ((g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_DIGIT) &&
            (i == g_lcd_local_phone_selected_digit)) {
            u8g2_DrawBox(u8g2, x - 1U, 20U, 10U, 12U);
            u8g2_SetDrawColor(u8g2, 0);
        }

        {
            char s[2];
            s[0] = (char)('0' + i);
            s[1] = '\0';
            u8g2_DrawStr(u8g2, x, 30, s);
        }

        u8g2_SetDrawColor(u8g2, 1);
    }

    /* 第三行：退出 / 保存 */
    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    if ((g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_ACTION) &&
        (g_lcd_local_phone_func_index == 0U)) {
        u8g2_DrawBox(u8g2, 10, 40, 34, 14);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 16, 52, g_exit_text, 2);
    u8g2_SetDrawColor(u8g2, 1);

    if ((g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_ACTION) &&
        (g_lcd_local_phone_func_index == 1U)) {
        u8g2_DrawBox(u8g2, 78, 40, 34, 14);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 84, 52, g_save_text, 2);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_port_flush_buffer();
}

static void lcd_plate_draw_ascii_cell(u8g2_t *u8g2,
                                      uint8_t x,
                                      uint8_t y,
                                      char ch,
                                      rt_bool_t selected)
{
    char s[2];

    if (selected == RT_TRUE) {
        u8g2_DrawBox(u8g2, (uint8_t)(x - 1U), (uint8_t)(y - 10U), 8U, 12U);
        u8g2_SetDrawColor(u8g2, 0);
    }

    s[0] = ch;
    s[1] = '\0';
    u8g2_DrawStr(u8g2, x, y, s);

    u8g2_SetDrawColor(u8g2, 1);
}

static void lcd_plate_draw_top_line(u8g2_t *u8g2)
{
    uint8_t i;
    static const uint8_t digit_x[5] = {34U, 42U, 50U, 58U, 66U};

    if ((g_lcd_plate_valid == 0U) && (g_lcd_plate_dirty == 0U)) {
        u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
        u8g2_DrawStr(u8g2, 2, 12, "_ _._ _ _ _ _");
        return;
    }

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);
    if (g_lcd_plate_focus == LCD_PLATE_FOCUS_PROVINCE) {
        u8g2_DrawBox(u8g2, 1U, 1U, 14U, 14U);
        u8g2_SetDrawColor(u8g2, 0);
    }
    u8g2_DrawGlyph(u8g2, 2U, 12U, g_lcd_plate_province_codes[g_lcd_plate_province_index]);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
    lcd_plate_draw_ascii_cell(u8g2,
                              18U,
                              12U,
                              g_lcd_plate_letters[g_lcd_plate_letter_index],
                              (g_lcd_plate_focus == LCD_PLATE_FOCUS_LETTER) ? RT_TRUE : RT_FALSE);

    u8g2_DrawStr(u8g2, 26U, 12U, ".");

    for (i = 0U; i < 5U; i++) {
        lcd_plate_draw_ascii_cell(u8g2,
                                  digit_x[i],
                                  12U,
                                  g_lcd_plate_tail_chars[g_lcd_plate_digits[i]],
                                  (g_lcd_plate_focus == (LCD_PLATE_FOCUS_DIGIT0 + i)) ? RT_TRUE : RT_FALSE);
    }
}


static void lcd_plate_draw_ascii_option(u8g2_t *u8g2,
                                        uint8_t x,
                                        uint8_t y,
                                        char ch,
                                        rt_bool_t selected)
{
    char s[2];

    if (selected == RT_TRUE) {
        u8g2_DrawBox(u8g2, (uint8_t)(x - 1U), (uint8_t)(y - 10U), 9U, 12U);
        u8g2_SetDrawColor(u8g2, 0);
    }

    s[0] = ch;
    s[1] = '\0';
    u8g2_DrawStr(u8g2, x, y, s);

    u8g2_SetDrawColor(u8g2, 1);
}

static void lcd_plate_save_and_ok(void)
{
    svc_storage_plate_number_t plate;
    uint8_t i;

    plate.valid = 1U;
    plate.province_index = g_lcd_plate_province_index;
    plate.letter = g_lcd_plate_letters[g_lcd_plate_letter_index];

    for (i = 0U; i < 5U; i++) {
        plate.digits[i] = g_lcd_plate_tail_chars[g_lcd_plate_digits[i]];
    }
    plate.digits[5] = '\0';

    if (svc_storage_save_plate_number(&plate) == RT_TRUE) {
        g_lcd_plate_valid = 1U;
        g_lcd_plate_dirty = 0U;
        app_usart_cmd_refresh_status_config();
        lcd_page_enter_common_ok(LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_SET);
    }

}

static void lcd_vin_draw_top_line(u8g2_t *u8g2)
{
    uint8_t i;
    uint8_t x;
    char ch;
    rt_bool_t selected;

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);

    for (i = 0U; i < LCD_VIN_LEN; i++) {
        x = (uint8_t)(4U + i * 7U);

        if ((g_lcd_vin_edit_mask & (1UL << i)) != 0UL) {
            ch = g_lcd_plate_tail_chars[g_lcd_vin_chars[i]];
        } else {
            ch = '_';
        }

        selected = ((g_lcd_vin_area != LCD_VIN_AREA_ACTION) &&
                    (g_lcd_vin_cursor == i)) ? RT_TRUE : RT_FALSE;

        lcd_plate_draw_ascii_cell(u8g2,
                                  x,
                                  12U,
                                  ch,
                                  selected);
    }
}


static void lcd_prepare_vin_set_page(void)
{
    svc_storage_vin_t vin;
    uint8_t i;
    const char *p;

    g_lcd_vin_area = LCD_VIN_AREA_POSITION;
    g_lcd_vin_cursor = 0U;
    g_lcd_vin_option_index = 0U;
    g_lcd_vin_action = LCD_VIN_ACTION_EXIT;
    g_lcd_vin_dirty = 0U;
    g_lcd_vin_edit_mask = 0UL;

    if ((svc_storage_load_vin(&vin) == RT_TRUE) && (vin.valid == 1U)) {
        g_lcd_vin_valid = 1U;
        g_lcd_vin_edit_mask = LCD_VIN_COMPLETE_MASK;

        for (i = 0U; i < LCD_VIN_LEN; i++) {
            p = strchr(g_lcd_plate_tail_chars, vin.vin[i]);
            if (p != RT_NULL) {
                g_lcd_vin_chars[i] = (uint8_t)(p - g_lcd_plate_tail_chars);
            } else {
                g_lcd_vin_chars[i] = 0U;
            }
        }
    } else {
        g_lcd_vin_valid = 0U;

        for (i = 0U; i < LCD_VIN_LEN; i++) {
            g_lcd_vin_chars[i] = 0U;
        }
    }

    g_lcd_vin_option_index = g_lcd_vin_chars[g_lcd_vin_cursor];
}


static void lcd_vin_save_and_ok(void)
{
    svc_storage_vin_t vin;
    uint8_t i;

    vin.valid = 1U;

    for (i = 0U; i < LCD_VIN_LEN; i++) {
        vin.vin[i] = g_lcd_plate_tail_chars[g_lcd_vin_chars[i]];
    }
    vin.vin[LCD_VIN_LEN] = '\0';

    if (svc_storage_save_vin(&vin) == RT_TRUE) {
        g_lcd_vin_valid = 1U;
        g_lcd_vin_dirty = 0U;
        app_usart_cmd_refresh_status_config();
        lcd_page_enter_common_ok(LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_VIN);
    }

}

static void lcd_render_vin_set_ui(void)
{
    u8g2_t *u8g2;
    uint8_t i;
    uint8_t x;
    static const uint16_t g_confirm_text[] = {0x786E, 0x8BA4};
    static const uint16_t g_exit_text[] = {0x9000, 0x51FA};

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    lcd_vin_draw_top_line(u8g2);

    u8g2_DrawFrame(u8g2, 2U, 18U, 128U, 28U);

    if (g_lcd_vin_area == LCD_VIN_AREA_CHAR_SELECT) {
        u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);

        for (i = 0U; i < 18U; i++) {
            x = (uint8_t)(4U + i * 7U);
            lcd_plate_draw_ascii_option(
                u8g2,
                x,
                30U,
                g_lcd_plate_tail_chars[i],
                (i == g_lcd_vin_option_index) ? RT_TRUE : RT_FALSE);
        }

        for (i = 18U; i < LCD_VIN_CHAR_COUNT; i++) {
            x = (uint8_t)(4U + (i - 18U) * 7U);
            lcd_plate_draw_ascii_option(
                u8g2,
                x,
                42U,
                g_lcd_plate_tail_chars[i],
                (i == g_lcd_vin_option_index) ? RT_TRUE : RT_FALSE);
        }
    }

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    if ((g_lcd_vin_area == LCD_VIN_AREA_ACTION) &&
        (g_lcd_vin_action == LCD_VIN_ACTION_CONFIRM)) {
        u8g2_DrawBox(u8g2, 0U, 49U, 34U, 14U);
        u8g2_SetDrawColor(u8g2, 0);
    }

    lcd_u8g2_draw_unicode_seq(u8g2, 2U, 60U, g_confirm_text, 2U);
    u8g2_SetDrawColor(u8g2, 1);

    if ((g_lcd_vin_area == LCD_VIN_AREA_ACTION) &&
        (g_lcd_vin_action == LCD_VIN_ACTION_EXIT)) {
        u8g2_DrawBox(u8g2, 96U, 49U, 34U, 14U);
        u8g2_SetDrawColor(u8g2, 0);
    }

    lcd_u8g2_draw_unicode_seq(u8g2, 104U, 60U, g_exit_text, 2U);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_port_flush_buffer();
}


static rt_bool_t lcd_handle_vin_set_keys(void)
{
    uint8_t i;

    if (g_lcd_current_page_id != LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_VIN) {
        return RT_FALSE;
    }

    if (svc_adc_consume_s1_event() == RT_TRUE) {
        if (g_lcd_vin_area == LCD_VIN_AREA_POSITION) {
            g_lcd_vin_option_index = g_lcd_vin_chars[g_lcd_vin_cursor];
            g_lcd_vin_area = LCD_VIN_AREA_CHAR_SELECT;
        } else if (g_lcd_vin_area == LCD_VIN_AREA_CHAR_SELECT) {
            g_lcd_vin_action = LCD_VIN_ACTION_EXIT;
            g_lcd_vin_area = LCD_VIN_AREA_ACTION;
        } else {
            g_lcd_vin_area = LCD_VIN_AREA_POSITION;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s2_event() == RT_TRUE) {
        if (g_lcd_vin_area == LCD_VIN_AREA_POSITION) {
            if (g_lcd_vin_cursor > 0U) {
                g_lcd_vin_cursor--;
            } else {
                g_lcd_vin_cursor = (LCD_VIN_LEN - 1U);
            }
        } else if (g_lcd_vin_area == LCD_VIN_AREA_CHAR_SELECT) {
            if (g_lcd_vin_option_index > 0U) {
                g_lcd_vin_option_index--;
            } else {
                g_lcd_vin_option_index = (LCD_VIN_CHAR_COUNT - 1U);
            }
        } else {
            g_lcd_vin_action = (g_lcd_vin_action == LCD_VIN_ACTION_CONFIRM) ?
                               LCD_VIN_ACTION_EXIT :
                               LCD_VIN_ACTION_CONFIRM;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s3_event() == RT_TRUE) {
        if (g_lcd_vin_area == LCD_VIN_AREA_POSITION) {
            if (g_lcd_vin_cursor < (LCD_VIN_LEN - 1U)) {
                g_lcd_vin_cursor++;
            } else {
                g_lcd_vin_cursor = 0U;
            }
        } else if (g_lcd_vin_area == LCD_VIN_AREA_CHAR_SELECT) {
            if (g_lcd_vin_option_index < (LCD_VIN_CHAR_COUNT - 1U)) {
                g_lcd_vin_option_index++;
            } else {
                g_lcd_vin_option_index = 0U;
            }
        } else {
            g_lcd_vin_action = (g_lcd_vin_action == LCD_VIN_ACTION_CONFIRM) ?
                               LCD_VIN_ACTION_EXIT :
                               LCD_VIN_ACTION_CONFIRM;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s4_event() == RT_TRUE) {
        if (g_lcd_vin_area == LCD_VIN_AREA_CHAR_SELECT) {
            g_lcd_vin_chars[g_lcd_vin_cursor] = g_lcd_vin_option_index;
            g_lcd_vin_edit_mask |= (1UL << g_lcd_vin_cursor);
            g_lcd_vin_dirty = 1U;

            if (g_lcd_vin_cursor < (LCD_VIN_LEN - 1U)) {
                g_lcd_vin_cursor++;
                g_lcd_vin_option_index = g_lcd_vin_chars[g_lcd_vin_cursor];
                g_lcd_vin_area = LCD_VIN_AREA_CHAR_SELECT;
            } else {
                g_lcd_vin_action = LCD_VIN_ACTION_CONFIRM;
                g_lcd_vin_area = LCD_VIN_AREA_ACTION;
            }

            g_lcd_need_redraw = RT_TRUE;
            return RT_TRUE;
        }


        if (g_lcd_vin_area == LCD_VIN_AREA_ACTION) {
            if (g_lcd_vin_action == LCD_VIN_ACTION_EXIT) {
                lcd_page_enter(LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO);
                return RT_TRUE;
            }

            if (g_lcd_vin_edit_mask != LCD_VIN_COMPLETE_MASK) {
                for (i = 0U; i < LCD_VIN_LEN; i++) {
                    if ((g_lcd_vin_edit_mask & (1UL << i)) == 0UL) {
                        g_lcd_vin_cursor = i;
                        break;
                    }
                }

                g_lcd_vin_area = LCD_VIN_AREA_POSITION;
                g_lcd_need_redraw = RT_TRUE;
                return RT_TRUE;
            }

            lcd_vin_save_and_ok();
            return RT_TRUE;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    return RT_FALSE;
}

static void lcd_province_id_draw_top_line(u8g2_t *u8g2)
{
    uint8_t i;
    uint8_t x;
    char ch;
    rt_bool_t selected;

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);

    for (i = 0U; i < LCD_PROVINCE_ID_LEN; i++) {
        x = (uint8_t)(56U + i * 12U);

        if ((g_lcd_province_id_edit_mask & (1U << i)) != 0U) {
            ch = g_lcd_plate_tail_chars[g_lcd_province_id_chars[i]];
        } else {
            ch = '_';
        }

        selected = ((g_lcd_province_id_area != LCD_PROVINCE_ID_AREA_ACTION) &&
                    (g_lcd_province_id_cursor == i)) ? RT_TRUE : RT_FALSE;

        lcd_plate_draw_ascii_cell(u8g2,
                                  x,
                                  12U,
                                  ch,
                                  selected);
    }
}

static void lcd_prepare_province_id_set_page(void)
{
    svc_storage_province_id_t province_id;
    uint8_t i;

    g_lcd_province_id_area = LCD_PROVINCE_ID_AREA_POSITION;
    g_lcd_province_id_cursor = 0U;
    g_lcd_province_id_option_index = 0U;
    g_lcd_province_id_action = LCD_PROVINCE_ID_ACTION_EXIT;
    g_lcd_province_id_dirty = 0U;
    g_lcd_province_id_edit_mask = 0U;

    if ((svc_storage_load_province_id(&province_id) == RT_TRUE) &&
        (province_id.valid == 1U)) {
        g_lcd_province_id_valid = 1U;
        g_lcd_province_id_edit_mask = LCD_PROVINCE_ID_COMPLETE_MASK;

        for (i = 0U; i < LCD_PROVINCE_ID_LEN; i++) {
            g_lcd_province_id_chars[i] = (uint8_t)(province_id.id[i] - '0');
        }
    } else {
        g_lcd_province_id_valid = 0U;

        for (i = 0U; i < LCD_PROVINCE_ID_LEN; i++) {
            g_lcd_province_id_chars[i] = 0U;
        }
    }

    g_lcd_province_id_option_index =
        g_lcd_province_id_chars[g_lcd_province_id_cursor];
}

static void lcd_province_id_save_and_ok(void)
{
    svc_storage_province_id_t province_id;
    uint8_t i;

    province_id.valid = 1U;

    for (i = 0U; i < LCD_PROVINCE_ID_LEN; i++) {
        province_id.id[i] =
            g_lcd_plate_tail_chars[g_lcd_province_id_chars[i]];
    }

    province_id.id[LCD_PROVINCE_ID_LEN] = '\0';

    if (svc_storage_save_province_id(&province_id) == RT_TRUE) {
        g_lcd_province_id_valid = 1U;
        g_lcd_province_id_dirty = 0U;
        app_usart_cmd_refresh_status_config();
        lcd_page_enter_common_ok(LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PROVINCE_ID);
    }

}

static void lcd_render_province_id_set_ui(void)
{
    u8g2_t *u8g2;
    uint8_t i;
    uint8_t x;
    static const uint16_t g_confirm_text[] = {0x786E, 0x8BA4};
    static const uint16_t g_exit_text[] = {0x9000, 0x51FA};

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    lcd_province_id_draw_top_line(u8g2);

    u8g2_DrawFrame(u8g2, 2U, 18U, 128U, 28U);

    if (g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_CHAR_SELECT) {
        u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);

        for (i = 0U; i < LCD_PROVINCE_ID_CHAR_COUNT; i++) {
            x = (uint8_t)(26U + i * 9U);

            lcd_plate_draw_ascii_option(
                u8g2,
                x,
                36U,
                g_lcd_plate_tail_chars[i],
                (i == g_lcd_province_id_option_index) ? RT_TRUE : RT_FALSE);
        }
    }

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    if ((g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_ACTION) &&
        (g_lcd_province_id_action == LCD_PROVINCE_ID_ACTION_CONFIRM)) {
        u8g2_DrawBox(u8g2, 0U, 49U, 34U, 14U);
        u8g2_SetDrawColor(u8g2, 0);
    }

    lcd_u8g2_draw_unicode_seq(u8g2, 2U, 60U, g_confirm_text, 2U);
    u8g2_SetDrawColor(u8g2, 1);

    if ((g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_ACTION) &&
        (g_lcd_province_id_action == LCD_PROVINCE_ID_ACTION_EXIT)) {
        u8g2_DrawBox(u8g2, 96U, 49U, 34U, 14U);
        u8g2_SetDrawColor(u8g2, 0);
    }

    lcd_u8g2_draw_unicode_seq(u8g2, 104U, 60U, g_exit_text, 2U);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_port_flush_buffer();
}

static rt_bool_t lcd_handle_province_id_set_keys(void)
{
    uint8_t i;

    if (g_lcd_current_page_id != LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PROVINCE_ID) {
        return RT_FALSE;
    }

    if (svc_adc_consume_s1_event() == RT_TRUE) {
        if (g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_POSITION) {
            g_lcd_province_id_option_index =
                g_lcd_province_id_chars[g_lcd_province_id_cursor];
            g_lcd_province_id_area = LCD_PROVINCE_ID_AREA_CHAR_SELECT;
        } else if (g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_CHAR_SELECT) {
            g_lcd_province_id_action = LCD_PROVINCE_ID_ACTION_EXIT;
            g_lcd_province_id_area = LCD_PROVINCE_ID_AREA_ACTION;
        } else {
            g_lcd_province_id_area = LCD_PROVINCE_ID_AREA_POSITION;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s2_event() == RT_TRUE) {
        if (g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_POSITION) {
            if (g_lcd_province_id_cursor > 0U) {
                g_lcd_province_id_cursor--;
            } else {
                g_lcd_province_id_cursor = LCD_PROVINCE_ID_LEN - 1U;
            }
        } else if (g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_CHAR_SELECT) {
            if (g_lcd_province_id_option_index > 0U) {
                g_lcd_province_id_option_index--;
            } else {
                g_lcd_province_id_option_index = LCD_PROVINCE_ID_CHAR_COUNT - 1U;
            }
        } else {
            g_lcd_province_id_action =
                (g_lcd_province_id_action == LCD_PROVINCE_ID_ACTION_CONFIRM) ?
                LCD_PROVINCE_ID_ACTION_EXIT :
                LCD_PROVINCE_ID_ACTION_CONFIRM;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s3_event() == RT_TRUE) {
        if (g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_POSITION) {
            if (g_lcd_province_id_cursor < (LCD_PROVINCE_ID_LEN - 1U)) {
                g_lcd_province_id_cursor++;
            } else {
                g_lcd_province_id_cursor = 0U;
            }
        } else if (g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_CHAR_SELECT) {
            if (g_lcd_province_id_option_index < (LCD_PROVINCE_ID_CHAR_COUNT - 1U)) {
                g_lcd_province_id_option_index++;
            } else {
                g_lcd_province_id_option_index = 0U;
            }
        } else {
            g_lcd_province_id_action =
                (g_lcd_province_id_action == LCD_PROVINCE_ID_ACTION_CONFIRM) ?
                LCD_PROVINCE_ID_ACTION_EXIT :
                LCD_PROVINCE_ID_ACTION_CONFIRM;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s4_event() == RT_TRUE) {
        if (g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_CHAR_SELECT) {
            g_lcd_province_id_chars[g_lcd_province_id_cursor] =
                g_lcd_province_id_option_index;
            g_lcd_province_id_edit_mask |=
                (uint8_t)(1U << g_lcd_province_id_cursor);
            g_lcd_province_id_dirty = 1U;

            if (g_lcd_province_id_cursor < (LCD_PROVINCE_ID_LEN - 1U)) {
                g_lcd_province_id_cursor++;
                g_lcd_province_id_option_index =
                    g_lcd_province_id_chars[g_lcd_province_id_cursor];
                g_lcd_province_id_area = LCD_PROVINCE_ID_AREA_CHAR_SELECT;
            } else {
                g_lcd_province_id_action = LCD_PROVINCE_ID_ACTION_CONFIRM;
                g_lcd_province_id_area = LCD_PROVINCE_ID_AREA_ACTION;
            }

            g_lcd_need_redraw = RT_TRUE;
            return RT_TRUE;
        }

        if (g_lcd_province_id_area == LCD_PROVINCE_ID_AREA_ACTION) {
            if (g_lcd_province_id_action == LCD_PROVINCE_ID_ACTION_EXIT) {
                lcd_page_enter(LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO);
                return RT_TRUE;
            }

            if (g_lcd_province_id_edit_mask != LCD_PROVINCE_ID_COMPLETE_MASK) {
                for (i = 0U; i < LCD_PROVINCE_ID_LEN; i++) {
                    if ((g_lcd_province_id_edit_mask & (1U << i)) == 0U) {
                        g_lcd_province_id_cursor = i;
                        break;
                    }
                }

                g_lcd_province_id_area = LCD_PROVINCE_ID_AREA_POSITION;
                g_lcd_need_redraw = RT_TRUE;
                return RT_TRUE;
            }

            lcd_province_id_save_and_ok();
            return RT_TRUE;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    return RT_FALSE;
}

static void lcd_city_id_draw_top_line(u8g2_t *u8g2)
{
    uint8_t i;
    uint8_t x;
    char ch;
    rt_bool_t selected;

    u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);

    for (i = 0U; i < LCD_CITY_ID_LEN; i++) {
        x = (uint8_t)(44U + i * 12U);

        if ((g_lcd_city_id_edit_mask & (1U << i)) != 0U) {
            ch = g_lcd_plate_tail_chars[g_lcd_city_id_chars[i]];
        } else {
            ch = '_';
        }

        selected = ((g_lcd_city_id_area != LCD_CITY_ID_AREA_ACTION) &&
                    (g_lcd_city_id_cursor == i)) ? RT_TRUE : RT_FALSE;

        lcd_plate_draw_ascii_cell(u8g2, x, 12U, ch, selected);
    }
}

static void lcd_prepare_city_id_set_page(void)
{
    svc_storage_city_id_t city_id;
    uint8_t i;

    g_lcd_city_id_area = LCD_CITY_ID_AREA_POSITION;
    g_lcd_city_id_cursor = 0U;
    g_lcd_city_id_option_index = 0U;
    g_lcd_city_id_action = LCD_CITY_ID_ACTION_EXIT;
    g_lcd_city_id_dirty = 0U;
    g_lcd_city_id_edit_mask = 0U;

    if ((svc_storage_load_city_id(&city_id) == RT_TRUE) &&
        (city_id.valid == 1U)) {
        g_lcd_city_id_valid = 1U;
        g_lcd_city_id_edit_mask = LCD_CITY_ID_COMPLETE_MASK;

        for (i = 0U; i < LCD_CITY_ID_LEN; i++) {
            g_lcd_city_id_chars[i] = (uint8_t)(city_id.id[i] - '0');
        }
    } else {
        g_lcd_city_id_valid = 0U;

        for (i = 0U; i < LCD_CITY_ID_LEN; i++) {
            g_lcd_city_id_chars[i] = 0U;
        }
    }

    g_lcd_city_id_option_index = g_lcd_city_id_chars[g_lcd_city_id_cursor];
}

static void lcd_city_id_save_and_ok(void)
{
    svc_storage_city_id_t city_id;
    uint8_t i;

    city_id.valid = 1U;

    for (i = 0U; i < LCD_CITY_ID_LEN; i++) {
        city_id.id[i] = g_lcd_plate_tail_chars[g_lcd_city_id_chars[i]];
    }

    city_id.id[LCD_CITY_ID_LEN] = '\0';

    if (svc_storage_save_city_id(&city_id) == RT_TRUE) {
        g_lcd_city_id_valid = 1U;
        g_lcd_city_id_dirty = 0U;
        app_usart_cmd_refresh_status_config();
        lcd_page_enter_common_ok(LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_CITY_ID);
    }

}

static void lcd_render_city_id_set_ui(void)
{
    u8g2_t *u8g2;
    uint8_t i;
    uint8_t x;
    static const uint16_t g_confirm_text[] = {0x786E, 0x8BA4};
    static const uint16_t g_exit_text[] = {0x9000, 0x51FA};

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    lcd_city_id_draw_top_line(u8g2);

    u8g2_DrawFrame(u8g2, 2U, 18U, 128U, 28U);

    if (g_lcd_city_id_area == LCD_CITY_ID_AREA_CHAR_SELECT) {
        u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);

        for (i = 0U; i < LCD_CITY_ID_CHAR_COUNT; i++) {
            x = (uint8_t)(26U + i * 9U);

            lcd_plate_draw_ascii_option(
                u8g2,
                x,
                36U,
                g_lcd_plate_tail_chars[i],
                (i == g_lcd_city_id_option_index) ? RT_TRUE : RT_FALSE);
        }
    }

    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    if ((g_lcd_city_id_area == LCD_CITY_ID_AREA_ACTION) &&
        (g_lcd_city_id_action == LCD_CITY_ID_ACTION_CONFIRM)) {
        u8g2_DrawBox(u8g2, 0U, 49U, 34U, 14U);
        u8g2_SetDrawColor(u8g2, 0);
    }

    lcd_u8g2_draw_unicode_seq(u8g2, 2U, 60U, g_confirm_text, 2U);
    u8g2_SetDrawColor(u8g2, 1);

    if ((g_lcd_city_id_area == LCD_CITY_ID_AREA_ACTION) &&
        (g_lcd_city_id_action == LCD_CITY_ID_ACTION_EXIT)) {
        u8g2_DrawBox(u8g2, 96U, 49U, 34U, 14U);
        u8g2_SetDrawColor(u8g2, 0);
    }

    lcd_u8g2_draw_unicode_seq(u8g2, 104U, 60U, g_exit_text, 2U);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_port_flush_buffer();
}

static rt_bool_t lcd_handle_city_id_set_keys(void)
{
    uint8_t i;

    if (g_lcd_current_page_id != LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_CITY_ID) {
        return RT_FALSE;
    }

    if (svc_adc_consume_s1_event() == RT_TRUE) {
        if (g_lcd_city_id_area == LCD_CITY_ID_AREA_POSITION) {
            g_lcd_city_id_option_index = g_lcd_city_id_chars[g_lcd_city_id_cursor];
            g_lcd_city_id_area = LCD_CITY_ID_AREA_CHAR_SELECT;
        } else if (g_lcd_city_id_area == LCD_CITY_ID_AREA_CHAR_SELECT) {
            g_lcd_city_id_action = LCD_CITY_ID_ACTION_EXIT;
            g_lcd_city_id_area = LCD_CITY_ID_AREA_ACTION;
        } else {
            g_lcd_city_id_area = LCD_CITY_ID_AREA_POSITION;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s2_event() == RT_TRUE) {
        if (g_lcd_city_id_area == LCD_CITY_ID_AREA_POSITION) {
            if (g_lcd_city_id_cursor > 0U) {
                g_lcd_city_id_cursor--;
            } else {
                g_lcd_city_id_cursor = LCD_CITY_ID_LEN - 1U;
            }
        } else if (g_lcd_city_id_area == LCD_CITY_ID_AREA_CHAR_SELECT) {
            if (g_lcd_city_id_option_index > 0U) {
                g_lcd_city_id_option_index--;
            } else {
                g_lcd_city_id_option_index = LCD_CITY_ID_CHAR_COUNT - 1U;
            }
        } else {
            g_lcd_city_id_action =
                (g_lcd_city_id_action == LCD_CITY_ID_ACTION_CONFIRM) ?
                LCD_CITY_ID_ACTION_EXIT :
                LCD_CITY_ID_ACTION_CONFIRM;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s3_event() == RT_TRUE) {
        if (g_lcd_city_id_area == LCD_CITY_ID_AREA_POSITION) {
            if (g_lcd_city_id_cursor < (LCD_CITY_ID_LEN - 1U)) {
                g_lcd_city_id_cursor++;
            } else {
                g_lcd_city_id_cursor = 0U;
            }
        } else if (g_lcd_city_id_area == LCD_CITY_ID_AREA_CHAR_SELECT) {
            if (g_lcd_city_id_option_index < (LCD_CITY_ID_CHAR_COUNT - 1U)) {
                g_lcd_city_id_option_index++;
            } else {
                g_lcd_city_id_option_index = 0U;
            }
        } else {
            g_lcd_city_id_action =
                (g_lcd_city_id_action == LCD_CITY_ID_ACTION_CONFIRM) ?
                LCD_CITY_ID_ACTION_EXIT :
                LCD_CITY_ID_ACTION_CONFIRM;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s4_event() == RT_TRUE) {
        if (g_lcd_city_id_area == LCD_CITY_ID_AREA_CHAR_SELECT) {
            g_lcd_city_id_chars[g_lcd_city_id_cursor] =
                g_lcd_city_id_option_index;
            g_lcd_city_id_edit_mask |=
                (uint8_t)(1U << g_lcd_city_id_cursor);
            g_lcd_city_id_dirty = 1U;

            if (g_lcd_city_id_cursor < (LCD_CITY_ID_LEN - 1U)) {
                g_lcd_city_id_cursor++;
                g_lcd_city_id_option_index =
                    g_lcd_city_id_chars[g_lcd_city_id_cursor];
                g_lcd_city_id_area = LCD_CITY_ID_AREA_CHAR_SELECT;
            } else {
                g_lcd_city_id_action = LCD_CITY_ID_ACTION_CONFIRM;
                g_lcd_city_id_area = LCD_CITY_ID_AREA_ACTION;
            }

            g_lcd_need_redraw = RT_TRUE;
            return RT_TRUE;
        }

        if (g_lcd_city_id_area == LCD_CITY_ID_AREA_ACTION) {
            if (g_lcd_city_id_action == LCD_CITY_ID_ACTION_EXIT) {
                lcd_page_enter(LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO);
                return RT_TRUE;
            }

            if (g_lcd_city_id_edit_mask != LCD_CITY_ID_COMPLETE_MASK) {
                for (i = 0U; i < LCD_CITY_ID_LEN; i++) {
                    if ((g_lcd_city_id_edit_mask & (1U << i)) == 0U) {
                        g_lcd_city_id_cursor = i;
                        break;
                    }
                }

                g_lcd_city_id_area = LCD_CITY_ID_AREA_POSITION;
                g_lcd_need_redraw = RT_TRUE;
                return RT_TRUE;
            }

            lcd_city_id_save_and_ok();
            return RT_TRUE;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    return RT_FALSE;
}


static void lcd_prepare_plate_set_page(void)
{
    svc_storage_plate_number_t plate;
    uint8_t i;
       const char *p;

    g_lcd_plate_focus = LCD_PLATE_FOCUS_PROVINCE;
    g_lcd_plate_dirty = 0U;

    if ((svc_storage_load_plate_number(&plate) == RT_TRUE) &&
        (plate.valid == 1U) &&
        (plate.province_index < (sizeof(g_lcd_plate_province_codes) / sizeof(g_lcd_plate_province_codes[0]))) &&
        (plate.letter >= 'A') &&
        (plate.letter <= 'Z')) {
        g_lcd_plate_valid = 1U;
        g_lcd_plate_province_index = plate.province_index;
        g_lcd_plate_letter_index = (uint8_t)(plate.letter - 'A');

        for (i = 0U; i < 5U; i++) {

            p = strchr(g_lcd_plate_tail_chars, plate.digits[i]);
            if (p != RT_NULL) {
                g_lcd_plate_digits[i] = (uint8_t)(p - g_lcd_plate_tail_chars);
            } else {
                g_lcd_plate_digits[i] = 0U;
            }

        }
    } else {
        g_lcd_plate_valid = 0U;
        g_lcd_plate_province_index = 0U;
        g_lcd_plate_letter_index = 0U;

        for (i = 0U; i < 5U; i++) {
            g_lcd_plate_digits[i] = 0U;
        }
    }
}

static void lcd_render_plate_set_ui(void)
{
    u8g2_t *u8g2;
    uint8_t i;
    uint8_t x;
    uint8_t selected_digit = 0U;
    uint8_t letter_start;
    static const uint16_t g_confirm_text[] = {0x786E, 0x8BA4}; /* 确认 */
    static const uint16_t g_exit_text[] = {0x9000, 0x51FA};    /* 退出 */

    u8g2 = u8g2_port_get();
    if (u8g2 == RT_NULL) {
        return;
    }

    u8g2_port_clear_buffer();
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetDrawColor(u8g2, 1);

    lcd_plate_draw_top_line(u8g2);

    u8g2_DrawFrame(u8g2, 2, 18, 128, 28);

    if (g_lcd_plate_focus == LCD_PLATE_FOCUS_PROVINCE) {
        uint8_t province_count;
        uint8_t province_start;

        province_count = (uint8_t)(sizeof(g_lcd_plate_province_codes) / sizeof(g_lcd_plate_province_codes[0]));
        u8g2_SetFont(u8g2, LCD_FONT_CN_12);

        if (g_lcd_plate_province_index < 2U) {
            province_start = 0U;
        } else if (g_lcd_plate_province_index >= (uint8_t)(province_count - 2U)) {
            province_start = (uint8_t)(province_count - 4U);
        } else {
            province_start = (uint8_t)(g_lcd_plate_province_index - 1U);
        }

        for (i = 0U; i < 4U; i++) {
            uint8_t province_index = (uint8_t)(province_start + i);

            x = (uint8_t)(14U + i * 28U);
            if (province_index == g_lcd_plate_province_index) {
                u8g2_DrawBox(u8g2, (uint8_t)(x - 3U), 24U, 18U, 16U);
                u8g2_SetDrawColor(u8g2, 0);
            }

            u8g2_DrawGlyph(u8g2, x, 37, g_lcd_plate_province_codes[province_index]);
            u8g2_SetDrawColor(u8g2, 1);
        }
    } else if (g_lcd_plate_focus == LCD_PLATE_FOCUS_LETTER) {
        u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);

        if (g_lcd_plate_letter_index < 3U) {
            letter_start = 0U;
        } else if (g_lcd_plate_letter_index > 22U) {
            letter_start = 20U;
        } else {
            letter_start = (uint8_t)(g_lcd_plate_letter_index - 3U);
        }

        for (i = 0U; i < 6U; i++) {
            uint8_t letter_index = (uint8_t)(letter_start + i);

            x = (uint8_t)(12U + i * 18U);
            lcd_plate_draw_ascii_option(u8g2,
                                        x,
                                        36U,
                                        g_lcd_plate_letters[letter_index],
                                        (letter_index == g_lcd_plate_letter_index) ? RT_TRUE : RT_FALSE);
        }


    } else if ((g_lcd_plate_focus >= LCD_PLATE_FOCUS_DIGIT0) &&
               (g_lcd_plate_focus <= LCD_PLATE_FOCUS_DIGIT4)) {
        uint8_t tail_start;
        uint8_t tail_index;

        u8g2_SetFont(u8g2, LCD_FONT_ASCII_SMALL);
        selected_digit = g_lcd_plate_digits[g_lcd_plate_focus - LCD_PLATE_FOCUS_DIGIT0];

        if (selected_digit < 4U) {
            tail_start = 0U;
        } else if (selected_digit > 31U) {
            tail_start = 27U;
        } else {
            tail_start = (uint8_t)(selected_digit - 4U);
        }

        for (i = 0U; i < 9U; i++) {
            tail_index = (uint8_t)(tail_start + i);
            x = (uint8_t)(6U + i * 14U);

            lcd_plate_draw_ascii_option(u8g2,
                                        x,
                                        30U,
                                        g_lcd_plate_tail_chars[tail_index],
                                        (tail_index == selected_digit) ? RT_TRUE : RT_FALSE);
        }

        for (i = 0U; i < 9U; i++) {
            tail_index = (uint8_t)(tail_start + 9U + i);
            if (tail_index >= 36U) {
                break;
            }

            x = (uint8_t)(6U + i * 14U);
            lcd_plate_draw_ascii_option(u8g2,
                                        x,
                                        42U,
                                        g_lcd_plate_tail_chars[tail_index],
                                        (tail_index == selected_digit) ? RT_TRUE : RT_FALSE);
        }
    }



    u8g2_SetFont(u8g2, LCD_FONT_CN_12);

    if (g_lcd_plate_focus == LCD_PLATE_FOCUS_CONFIRM) {
        u8g2_DrawBox(u8g2, 0, 49, 34, 14);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 2, 60, g_confirm_text, 2);
    u8g2_SetDrawColor(u8g2, 1);

    if (g_lcd_plate_focus == LCD_PLATE_FOCUS_EXIT) {
        u8g2_DrawBox(u8g2, 96, 49, 34, 14);
        u8g2_SetDrawColor(u8g2, 0);
    }
    lcd_u8g2_draw_unicode_seq(u8g2, 104, 60, g_exit_text, 2);
    u8g2_SetDrawColor(u8g2, 1);

    u8g2_port_flush_buffer();
}


static rt_bool_t lcd_handle_plate_set_keys(void)
{
    uint8_t digit_index;

    if (g_lcd_current_page_id != LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_SET) {
        return RT_FALSE;
    }

    if (svc_adc_consume_s1_event() == RT_TRUE) {
        if (g_lcd_plate_focus < LCD_PLATE_FOCUS_MAX) {
            g_lcd_plate_focus++;
        } else {
            g_lcd_plate_focus = LCD_PLATE_FOCUS_PROVINCE;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s2_event() == RT_TRUE) {
        if (g_lcd_plate_focus == LCD_PLATE_FOCUS_PROVINCE) {
            if (g_lcd_plate_province_index > 0U) {
                g_lcd_plate_province_index--;
            } else {
                g_lcd_plate_province_index =
                    (uint8_t)((sizeof(g_lcd_plate_province_codes) / sizeof(g_lcd_plate_province_codes[0])) - 1U);
            }
            g_lcd_plate_dirty = 1U;
        } else if (g_lcd_plate_focus == LCD_PLATE_FOCUS_LETTER) {
            if (g_lcd_plate_letter_index > 0U) {
                g_lcd_plate_letter_index--;
            } else {
                g_lcd_plate_letter_index = 25U;
            }
            g_lcd_plate_dirty = 1U;
        } else if ((g_lcd_plate_focus >= LCD_PLATE_FOCUS_DIGIT0) &&
                   (g_lcd_plate_focus <= LCD_PLATE_FOCUS_DIGIT4)) {
            digit_index = (uint8_t)(g_lcd_plate_focus - LCD_PLATE_FOCUS_DIGIT0);

            if (g_lcd_plate_digits[digit_index] > 0U) {
                g_lcd_plate_digits[digit_index]--;
            } else {
                g_lcd_plate_digits[digit_index] = 35U;
            }

            g_lcd_plate_dirty = 1U;
        } else if (g_lcd_plate_focus == LCD_PLATE_FOCUS_CONFIRM) {
            g_lcd_plate_focus = LCD_PLATE_FOCUS_EXIT;
        } else {
            g_lcd_plate_focus = LCD_PLATE_FOCUS_CONFIRM;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s3_event() == RT_TRUE) {
        if (g_lcd_plate_focus == LCD_PLATE_FOCUS_PROVINCE) {
            g_lcd_plate_province_index++;
            if (g_lcd_plate_province_index >=
                (sizeof(g_lcd_plate_province_codes) / sizeof(g_lcd_plate_province_codes[0]))) {
                g_lcd_plate_province_index = 0U;
            }
            g_lcd_plate_dirty = 1U;
        } else if (g_lcd_plate_focus == LCD_PLATE_FOCUS_LETTER) {
            if (g_lcd_plate_letter_index < 25U) {
                g_lcd_plate_letter_index++;
            } else {
                g_lcd_plate_letter_index = 0U;
            }
            g_lcd_plate_dirty = 1U;
        } else if ((g_lcd_plate_focus >= LCD_PLATE_FOCUS_DIGIT0) &&
                   (g_lcd_plate_focus <= LCD_PLATE_FOCUS_DIGIT4)) {
            digit_index = (uint8_t)(g_lcd_plate_focus - LCD_PLATE_FOCUS_DIGIT0);

            if (g_lcd_plate_digits[digit_index] < 35U) {
                g_lcd_plate_digits[digit_index]++;
            } else {
                g_lcd_plate_digits[digit_index] = 0U;
            }

            g_lcd_plate_dirty = 1U;
        } else if (g_lcd_plate_focus == LCD_PLATE_FOCUS_CONFIRM) {
            g_lcd_plate_focus = LCD_PLATE_FOCUS_EXIT;
        } else {
            g_lcd_plate_focus = LCD_PLATE_FOCUS_CONFIRM;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    if (svc_adc_consume_s4_event() == RT_TRUE) {
        if (g_lcd_plate_focus == LCD_PLATE_FOCUS_CONFIRM) {
            lcd_plate_save_and_ok();
            return RT_TRUE;
        }

        if (g_lcd_plate_focus == LCD_PLATE_FOCUS_EXIT) {
            lcd_page_enter(LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO);
            return RT_TRUE;
        }

        g_lcd_need_redraw = RT_TRUE;
        return RT_TRUE;
    }

    return RT_FALSE;
}



static rt_bool_t lcd_handle_local_phone_keys(void)
{
    svc_storage_phone_t phone;

    if (g_lcd_current_page_id != LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_LOCAL_PHONE) {
        return RT_FALSE;
    }

    if (svc_adc_consume_s2_event() == RT_TRUE) {
        if (g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_NUMBER) {
            if (g_lcd_local_phone_cursor > 0U) {
                g_lcd_local_phone_cursor--;
            }

            g_lcd_local_phone_selected_digit =
                (uint8_t)(g_lcd_local_phone_digits[g_lcd_local_phone_cursor] - '0');
        } else if (g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_DIGIT) {
            if (g_lcd_local_phone_selected_digit > 0U) {
                g_lcd_local_phone_selected_digit--;
            } else {
                g_lcd_local_phone_selected_digit = 9U;
            }
        } else {
            if (g_lcd_local_phone_func_index > 0U) {
                g_lcd_local_phone_func_index--;
            } else {
                g_lcd_local_phone_func_index = 1U;
            }
        }

        g_lcd_need_redraw = RT_TRUE;
    }

    if (svc_adc_consume_s3_event() == RT_TRUE) {
        if (g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_NUMBER) {
            if (g_lcd_local_phone_cursor < 10U) {
                g_lcd_local_phone_cursor++;
            }

            g_lcd_local_phone_selected_digit =
                (uint8_t)(g_lcd_local_phone_digits[g_lcd_local_phone_cursor] - '0');
        } else if (g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_DIGIT) {
            if (g_lcd_local_phone_selected_digit < 9U) {
                g_lcd_local_phone_selected_digit++;
            } else {
                g_lcd_local_phone_selected_digit = 0U;
            }
        } else {
            if (g_lcd_local_phone_func_index < 1U) {
                g_lcd_local_phone_func_index++;
            } else {
                g_lcd_local_phone_func_index = 0U;
            }
        }

        g_lcd_need_redraw = RT_TRUE;
    }

    if (svc_adc_consume_s1_event() == RT_TRUE) {
        if (g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_NUMBER) {
            g_lcd_local_phone_focus = LCD_LOCAL_PHONE_FOCUS_DIGIT;
            g_lcd_local_phone_selected_digit =
                (uint8_t)(g_lcd_local_phone_digits[g_lcd_local_phone_cursor] - '0');
        } else if (g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_DIGIT) {
            g_lcd_local_phone_focus = LCD_LOCAL_PHONE_FOCUS_ACTION;
        } else {
            g_lcd_local_phone_focus = LCD_LOCAL_PHONE_FOCUS_NUMBER;
        }

        g_lcd_need_redraw = RT_TRUE;
    }

    if (svc_adc_consume_s4_event() == RT_TRUE) {
        if (g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_NUMBER) {
            g_lcd_local_phone_selected_digit =
                (uint8_t)(g_lcd_local_phone_digits[g_lcd_local_phone_cursor] - '0');
            g_lcd_need_redraw = RT_TRUE;
        } else if (g_lcd_local_phone_focus == LCD_LOCAL_PHONE_FOCUS_DIGIT) {
            g_lcd_local_phone_digits[g_lcd_local_phone_cursor] =
                (char)('0' + g_lcd_local_phone_selected_digit);

            if (g_lcd_local_phone_cursor < 10U) {
                g_lcd_local_phone_cursor++;
            }
/*
*/

            g_lcd_local_phone_selected_digit =
                (uint8_t)(g_lcd_local_phone_digits[g_lcd_local_phone_cursor] - '0');

            g_lcd_need_redraw = RT_TRUE;
        } else {
            if (g_lcd_local_phone_func_index == 0U) {
                lcd_page_enter(LCD_PAGE_SYSTEM_SETTING_HOST_PARAM);
            } else {
                rt_memcpy(phone.digits, g_lcd_local_phone_digits, sizeof(phone.digits));
                if (svc_storage_save_local_phone(&phone) == RT_TRUE) {
                                    app_usart_cmd_refresh_status_config();
                                    lcd_page_enter_common_ok(LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_LOCAL_PHONE);
                                }
            }

            g_lcd_need_redraw = RT_TRUE;
        }
    }

    return RT_TRUE;
}
















