#ifndef LCD_UI_H
#define LCD_UI_H

#include <rtthread.h>
#include <stdint.h>
#include "u8g2.h"

/*
 * ============================================================
 *  LCD UI 层 (User Interface Layer)
 * ============================================================
 *
 * 职责:
 *   管理 LCD 的菜单系统和页面导航。
 *   这是 LCD 子系统中最"上层"的模块, 负责:
 *   - 页面 (Page) 的注册和导航
 *   - 菜单列表的渲染
 *   - 按键事件的处理 (确认、返回、上下翻页)
 *   - 主页数据的管理 (速度、时间、定位等)
 *
 * 架构:
 *   页面系统使用"树形结构"(父子关系):
 *     HOME (根)
 *       ├── MAIN_MENU (主菜单)
 *       │   ├── DRIVE_RECORD (行驶记录子菜单)
 *       │   ├── DEVICE_STATUS (设备状态子菜单)
 *       │   └── SYSTEM_SETTING (系统设置子菜单)
 *       │       ├── VEHICLE_INFO (车辆信息)
 *       │       └── HOST_PARAM (主机参数)
 *       └── 其他页面
 *
 *   每个页面是一个 lcd_page_node_t 节点:
 *   - 有唯一的 page_id
 *   - 有父节点 (parent_id)
 *   - 有子节点列表 (children)
 *   - 有渲染函数 (render)
 *   - 有确认处理函数 (on_confirm)
 *   - 支持自动返回 (auto_return_ms)
 */

/* 文本消息最大长度: 4KB */
#define LCD_TEXT_MSG_MAX_LEN        4096U

/*
 * ============================================================
 *  页面 ID 枚举
 * ============================================================
 *
 * 所有 LCD 页面都有唯一的 ID, 用于导航和渲染选择。
 * 这是一个扁平化的枚举, 虽然页面在逻辑上是树形结构,
 * 但通过 parent_id 和 children 字段建立关系。
 *
 * 为什么不用嵌套结构体表示树?
 *   枚举 + 查找表 (lcd_page_node_t 数组) 更灵活:
 *   - 方便增删页面 (只需要修改枚举和数组)
 *   - 方便序列化和调试 (每个页面有数字 ID)
 *   - 支持循环引用(如返回主页)
 */
typedef enum
{
    /* === 主页 === */
    LCD_PAGE_HOME = 0,                              /* 主页: 速度/时间/定位 */
    LCD_PAGE_MAIN_MENU,                              /* 主菜单入口 */

    /* === 行驶记录 === */
    LCD_PAGE_DRIVE_RECORD_MENU,                      /* 行驶记录菜单 */
    LCD_PAGE_DEVICE_STATUS_MENU,                     /* 设备状态菜单 */
    LCD_PAGE_SYSTEM_SETTING_MENU,                    /* 系统设置菜单 */

    /* 行驶记录子页 */
    LCD_PAGE_DRIVE_RECORD_FATIGUE,                   /* 疲劳驾驶记录 */
    LCD_PAGE_DRIVE_RECORD_LOCATION,                  /* 位置信息 */
    LCD_PAGE_DRIVE_RECORD_MILEAGE,                   /* 里程信息 */
    LCD_PAGE_DRIVE_RECORD_DRIVER_INFO,               /* 驾驶员信息 */
    LCD_PAGE_DRIVE_RECORD_VEHICLE_INFO,              /* 车辆信息(行驶记录中) */
    LCD_PAGE_DRIVE_RECORD_LOAD_STATUS,               /* 载货状态 */
    LCD_PAGE_DRIVE_RECORD_LOAD_STATUS_OK,            /* 载货设置成功 */
    LCD_PAGE_DRIVE_RECORD_EXPORT,                    /* 数据导出 */
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER,               /* 信息中心 */
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT,          /* 文本消息列表 */
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_BROADCAST,     /* 广播消息 */
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EVENT_REPORT,  /* 事件报告 */
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EWAYBILL,      /* 电子运单 */

    /* 文本消息子类型 (按消息类型分类) */
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_DISPATCH,   /* 调度信息 */
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_NORMAL,     /* 普通信息 */
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_FAULT,      /* 故障信息 */
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_EMERGENCY,  /* 紧急信息 */

    LCD_PAGE_DRIVE_RECORD_VIN,                         /* VIN 码查询 */

    /* === 设备状态 === */
    LCD_PAGE_DEVICE_STATUS_VERSION,                    /* 版本信息 */
    LCD_PAGE_DEVICE_STATUS_GFRS,                       /* GFRS 状态 */
    LCD_PAGE_DEVICE_STATUS_SELF_TEST,                  /* 自检 */
    LCD_PAGE_DEVICE_STATUS_LOCATION_MODULE,            /* 定位模块状态 */
    LCD_PAGE_DEVICE_STATUS_VEHICLE,                    /* 车辆状态 */
    LCD_PAGE_DEVICE_STATUS_STORAGE,                    /* 存储状态 */
    LCD_PAGE_DEVICE_STATUS_AV,                         /* 音视频状态 */
    LCD_PAGE_DEVICE_STATUS_SPEED,                      /* 速度传感器 */
    LCD_PAGE_DEVICE_STATUS_DRIVER_MONITOR,             /* 驾驶员监控 */

    /* === 系统设置 === */
    LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME,                /* 按键音量 */
    LCD_PAGE_COMMON_CONFIG_OK,                         /* 通用"设置成功"页 */
    LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS,                /* 亮度设置菜单 */
    LCD_PAGE_SYSTEM_SETTING_BACKLIGHT_TIME,            /* 背光时间 */
    LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS_LEVEL,          /* 亮度级别 */

    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,              /* 车辆信息设置 */
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_VIN,          /* VIN 码设置 */
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_SET,    /* 车牌号设置 */
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_COLOR,  /* 车牌颜色 */
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_CLASS,  /* 车辆类型 */
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PROVINCE_ID,  /* 省份编号 */
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_CITY_ID,      /* 城市编号 */

    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM,                /* 主机参数设置 */
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_LOCAL_PHONE,     /* 本地电话号码 */
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SOS_PHONE,      /* 紧急电话 */
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SERVER1,         /* 服务器 1 */
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SERVER2,         /* 服务器 2 */

    LCD_PAGE_SYSTEM_SETTING_INIT_MILEAGE,              /* 初始里程设置 */
    LCD_PAGE_SYSTEM_SETTING_REGISTER,                   /* 设备注册 */
    LCD_PAGE_SYSTEM_SETTING_UNREGISTER,                 /* 设备注销 */
    LCD_PAGE_SYSTEM_SETTING_REC_VER,                    /* 记录仪版本 */
    LCD_PAGE_SYSTEM_SETTING_COMPONENT_VER,              /* 部件版本 */

    LCD_PAGE_MAX  /* 页面总数 (必须保持为最后一个) */
} lcd_page_id_t;

/*
 * 页面类型:
 *   VIEW  — 只显示信息, 没有可选的菜单项
 *   LIST  — 有可选择的菜单项列表
 *   ACTION_RESULT — 操作结果页(如"设置成功")
 */
typedef enum
{
    LCD_PAGE_KIND_VIEW = 0,
    LCD_PAGE_KIND_LIST,
    LCD_PAGE_KIND_ACTION_RESULT
} lcd_page_kind_t;

/*
 * 页面节点: 描述一个页面的所有属性
 *
 * 这种"树形数据 + 函数指针"的结构在嵌入式 UI 中很常见:
 *   - 所有页面数据在编译时定义好 (const 数组, 放在 ROM)
 *   - 运行时通过 page_id 索引查找
 *   - render 和 on_confirm 函数指针提供了灵活性
 *   - auto_return 实现了屏保/超时自动回主页的功能
 */
typedef struct
{
    lcd_page_id_t page_id;           /* 页面唯一 ID */
    lcd_page_id_t parent_id;         /* 父页面 ID (返回时跳转) */
    lcd_page_kind_t kind;            /* 页面类型 */
    const lcd_page_id_t *children;   /* 子页面列表 */
    uint8_t child_count;             /* 子页面数量 */
    uint8_t select_count;            /* 列表项数量 */
    rt_bool_t show_title;            /* 是否显示标题栏 */
    void (*render)(void);            /* 渲染函数指针 */
    void (*on_confirm)(void);        /* 确认回调函数指针 */
    uint32_t auto_return_ms;         /* 自动返回超时 (0=不自动返回) */
    lcd_page_id_t auto_return_target; /* 自动返回的目标页面 */
} lcd_page_node_t;

/*
 * 主页数据: 所有在主页上显示的信息
 * 其他服务(svc_lcd)调用函数更新这些数据
 */
typedef struct
{
    uint16_t speed_kmh_x10;          /* 速度: ×10, 如 655 = 65.5 km/h */
    uint8_t hour;                    /* 当前时 */
    uint8_t minute;                  /* 当前分 */
    uint8_t second;                  /* 当前秒 */
    uint8_t drive_hour;              /* 行驶时 */
    uint8_t drive_minute;            /* 行驶分 */
    uint8_t drive_second;            /* 行驶秒 */
    uint8_t top_status_value;        /* 顶部状态栏位图 */
    uint32_t latitude;               /* 纬度: 微度 */
    uint32_t longitude;              /* 经度: 微度 */
    uint32_t timestamp;              /* UTC 时间戳 */
    uint8_t latitude_direction;      /* N/S */
    uint8_t longitude_direction;     /* E/W */
    char card_id[20];                /* 驾驶员卡号 */
} lcd_home_ui_data_t;

/* 文本消息结构体: 信息中心的消息 */
typedef struct
{
    uint8_t valid;                                     /* 有效标志 */
    uint8_t flag;                                      /* 消息标志 */
    uint8_t text_type;                                 /* 消息类型 */
    uint16_t text_len;                                 /* 消息长度 */
    char text[LCD_TEXT_MSG_MAX_LEN + 1U];              /* 消息内容 (最大 4KB) */
    uint16_t page_index;                               /* 当前页码 */
    uint16_t page_count;                               /* 总页数 */
} lcd_text_msg_t;

/*
 * 列表资源: 描述一个菜单页面的文本内容
 *
 * 为什么菜单文本用 uint16_t * (Unicode 码点) 而不是 char *?
 *   因为 u8g2 的字体渲染使用 Unicode 码点数组来支持中文。
 *   GB2312 编码的汉字通过查表被转换成了 Unicode 序列。
 *
 *   item_texts: 每个菜单项对应的 Unicode 字符串数组
 *   item_counts: 每个字符串的字符数
 *   title_text: 页面标题的 Unicode 序列
 *   title_count: 标题字符数
 */
typedef struct
{
    lcd_page_id_t page_id;                   /* 关联的页面 ID */
    const uint16_t *const *item_texts;       /* 菜单项文本 (Unicode 指针数组) */
    const uint8_t *item_counts;              /* 每个菜单项的字符数 */
    uint8_t item_count;                      /* 菜单项数量 */
    const uint16_t *title_text;              /* 标题 Unicode 序列 */
    uint8_t title_count;                     /* 标题字符数 */
} lcd_ui_list_resource_t;

/* 函数指针类型: 页面进入钩子和备用渲染函数 */
typedef void (*lcd_ui_nav_enter_hook_t)(lcd_page_id_t page_id);
typedef void (*lcd_ui_render_fallback_t)(void);

/* ============================================================
 *  UI 核心控制接口
 * ============================================================ */

void lcd_ui_core_reset(void);                    /* 重置 UI 核心状态 */
void lcd_ui_data_reset(void);                    /* 重置 UI 数据 */
void lcd_ui_list_register(const lcd_ui_list_resource_t *resources, uint8_t resource_count);
void lcd_ui_pages_register(const lcd_page_node_t *pages, uint16_t page_count);
void lcd_ui_nav_set_enter_hook(lcd_ui_nav_enter_hook_t hook);  /* 设置进入页面时的钩子 */
void lcd_ui_render_set_fallback(lcd_ui_render_fallback_t fallback);  /* 设置备用渲染函数 */

/* 获取可修改数据指针 (调用者可直接修改) */
lcd_home_ui_data_t *lcd_ui_data_get_home_mutable(void);
lcd_text_msg_t *lcd_ui_data_get_text_mutable(void);
uint16_t *lcd_ui_data_get_overtime_drive_count_mutable(void);
uint32_t *lcd_ui_data_get_total_mileage_km_mutable(void);
uint16_t *lcd_ui_data_get_total_mileage_rem_m_mutable(void);

/* 获取可修改核心状态指针 */
rt_bool_t *lcd_ui_core_menu_mode_mutable(void);
lcd_page_id_t *lcd_ui_core_current_page_mutable(void);
uint8_t *lcd_ui_core_page_selected_mutable(void);
rt_tick_t *lcd_ui_core_page_enter_tick_mutable(void);
rt_bool_t *lcd_ui_core_need_redraw_mutable(void);
lcd_page_id_t *lcd_ui_core_common_ok_return_page_mutable(void);

/* 页面查询 */
const lcd_page_node_t *lcd_get_page_node(lcd_page_id_t page_id);
uint8_t lcd_page_get_depth(lcd_page_id_t page_id);         /* 页面在树中的深度 */
uint8_t lcd_page_get_select_count(lcd_page_id_t page_id);  /* 页面选择项数 */

/* 页面导航 */
void lcd_page_enter(lcd_page_id_t page_id);                /* 进入指定页面 */
void lcd_page_enter_common_ok(lcd_page_id_t return_page);  /* 进入"设置成功"页 */
void lcd_page_handle_back(void);                            /* 返回 */
void lcd_page_handle_nav(int8_t delta);                     /* 上下导航 */
void lcd_page_handle_confirm(void);                         /* 确认选择 */
void lcd_page_handle_auto_return(void);                     /* 自动返回检测 */
void lcd_page_handle_dynamic_refresh(void);                 /* 动态刷新 */
void lcd_render_current_page(void);                         /* 渲染当前页 */

/* 资源查询 */
rt_bool_t lcd_get_list_page_resources(lcd_page_id_t page_id,
                                      const uint16_t *const **item_texts,
                                      const uint8_t **item_counts,
                                      uint8_t *item_count,
                                      const uint16_t **title_text,
                                      uint8_t *title_count);

/*
 * u8g2 辅助函数:
 *   draw_unicode_seq — 绘制 Unicode 码点序列(支持中文)
 *   get_unicode_seq_width — 计算 Unicode 序列的像素宽度
 */
uint16_t lcd_u8g2_draw_unicode_seq(u8g2_t *u8g2,
                                   uint16_t x,
                                   uint16_t y,
                                   const uint16_t *codes,
                                   uint8_t count);
uint16_t lcd_u8g2_get_unicode_seq_width(u8g2_t *u8g2,
                                        const uint16_t *codes,
                                        uint8_t count);

/* 时间戳转换: Unix 时间戳 → 年月日时分 */
void lcd_timestamp_to_local_ymdhm(uint32_t timestamp,
                                  uint16_t *year,
                                  uint8_t *month,
                                  uint8_t *day,
                                  uint8_t *hour,
                                  uint8_t *minute);

#endif /* LCD_UI_H */
