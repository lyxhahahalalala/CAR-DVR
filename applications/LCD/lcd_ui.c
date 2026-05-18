/*
 * ============================================================
 *  lcd_ui.c — LCD UI 核心层
 * ============================================================
 *
 * 功能:
 *   这是 LCD 子系统的 UI 管理层, 负责:
 *   1) 页面导航 (进入/返回/确认)
 *   2) 菜单数据管理 (主页数据、消息数据)
 *   3) 页面渲染调度
 *   4) 按键事件处理 (方向、确认、返回)
 *   5) 车辆状态的动态刷新 (仅限设备状态页面)
 *   6) u8g2 辅助绘图函数 (Unicode 文本绘制)
 *   7) 时间戳转本地时间
 *
 * 本模块不直接操作硬件 SPI 或帧缓冲, 而是通过函数指针
 * 调用 svc_lcd 中注册的渲染函数。这种"回调注册"模式
 * 使 UI 层与渲染实现解耦。
 */
#include "lcd_ui.h"
#include "svc_vehicle_io.h"

/*
 * ============================================================
 *  静态全局变量
 * ============================================================
 *
 * 为什么都是 static?
 *   这些数据只在本文件中使用, 对外通过 get_mutable() 函数
 *   暴露可修改指针 (而不是暴露全局变量本身)。
 *   这是一种"封装"——外部只知道有数据, 不知道具体变量名。
 */

/* ---- 页面数据 ---- */
static lcd_home_ui_data_t g_lcd_home_ui;                /* 主页显示数据 */
static lcd_text_msg_t g_lcd_text_msg;                    /* 文本消息数据 */
static uint16_t g_lcd_overtime_drive_count;              /* 超时驾驶次数 */
static uint32_t g_lcd_total_mileage_km;                  /* 总里程 (km) */
static uint16_t g_lcd_total_mileage_rem_m;               /* 总里程 (m 小数部分) */

/* ---- UI 核心状态 ---- */
static rt_bool_t g_lcd_menu_mode = RT_FALSE;             /* 是否在菜单模式 (非主页) */
static lcd_page_id_t g_lcd_current_page_id = LCD_PAGE_HOME; /* 当前页面 ID */
static uint8_t g_lcd_page_selected[LCD_PAGE_MAX];        /* 每页的选中项索引 */
static rt_tick_t g_lcd_page_enter_tick = 0U;             /* 进入当前页时的 tick */
static rt_bool_t g_lcd_need_redraw = RT_TRUE;            /* 需要重绘标志 */
static lcd_page_id_t g_lcd_common_ok_return_page = LCD_PAGE_HOME; /* "设置成功"页的返回目标 */

/* ---- 页面系统注册句柄 ---- */
static const lcd_page_node_t *g_lcd_pages;                  /* 页面节点数组 (来自 svc_lcd) */
static uint16_t g_lcd_page_count;                           /* 页面总数 */
static const lcd_ui_list_resource_t *g_lcd_ui_list_resources;   /* 列表资源数组 */
static uint8_t g_lcd_ui_list_resource_count;                    /* 列表资源数量 */
static lcd_ui_nav_enter_hook_t g_lcd_ui_nav_enter_hook;         /* 页面进入钩子 */
static lcd_ui_render_fallback_t g_lcd_ui_render_fallback;       /* 备用渲染函数 */

/* ---- 车辆状态动态刷新控制 ---- */
static rt_tick_t g_lcd_vehicle_status_check_tick = 0U;   /* 上次检查 tick */
static uint16_t g_lcd_vehicle_status_last_bits = 0xFFFFU;/* 上次状态位域 (用于检测变化) */

/*
 * ============================================================
 *  lcd_ui_data_reset — 重置 UI 数据到默认值
 * ============================================================
 *
 * 调用时机: 系统初始化时
 *
 * 为什么主页时间默认是 09:16:45?
 *   这只是一个占位初始值, 表示"还未从 GPS/RTC 获取到时间"。
 *   实际时间会被 svc_lcd_update_home_time() 更新。
 *   初始值设置成合理的白天时间, 避免刚开机时显示 00:00:00
 *   让用户以为时间显示不正常。
 *
 * 卡号默认 20 个 '0' (未插卡状态):
 *   行驶记录仪标准: 未插入驾驶员卡时, 卡号显示全零。
 */
void lcd_ui_data_reset(void)
{
    rt_memset(&g_lcd_home_ui, 0, sizeof(g_lcd_home_ui));
    rt_memset(&g_lcd_text_msg, 0, sizeof(g_lcd_text_msg));

    g_lcd_home_ui.hour = 9U;         /* 默认时 */
    g_lcd_home_ui.minute = 16U;      /* 默认分 */
    g_lcd_home_ui.second = 45U;      /* 默认秒 */
    rt_strncpy(g_lcd_home_ui.card_id,
               "000000000000000000", /* 20 个 0 (未插卡) */
               sizeof(g_lcd_home_ui.card_id) - 1U);
    g_lcd_home_ui.card_id[sizeof(g_lcd_home_ui.card_id) - 1U] = '\0';

    g_lcd_overtime_drive_count = 0U;     /* 超时驾驶次数 = 0 */
    g_lcd_total_mileage_km = 0U;         /* 总里程 = 0 km */
    g_lcd_total_mileage_rem_m = 0U;      /* 总里程小数 = 0 m */
}

/*
 * ============================================================
 *  lcd_ui_core_reset — 重置 UI 核心状态
 * ============================================================
 *
 * 与 lcd_ui_data_reset 的区别:
 *   data_reset → 重置"显示内容"(速度、时间等)
 *   core_reset → 重置"UI 状态"(当前页、选中项等)
 *
 * 调用时机: 系统初始化、配置恢复出厂等
 */
void lcd_ui_core_reset(void)
{
    g_lcd_menu_mode = RT_FALSE;                              /* 回到主页模式 */
    rt_memset(g_lcd_page_selected, 0, sizeof(g_lcd_page_selected));  /* 所有页面选中第 0 项 */
    g_lcd_current_page_id = LCD_PAGE_HOME;                   /* 回到主页 */
    g_lcd_page_enter_tick = 0U;
    g_lcd_need_redraw = RT_TRUE;                             /* 强制重绘 */
    g_lcd_common_ok_return_page = LCD_PAGE_HOME;
}

/*
 * ============================================================
 *  数据获取接口 (暴露可修改指针)
 * ============================================================
 *
 * 为什么用"返回可修改指针"而不是"设置函数"?
 *   性能考虑: 外部模块(svc_lcd)需要频繁更新主页数据
 *   (每秒至少更新一次时间), 如果每次更新都调用 setter 函数,
 *   函数调用开销累积可观。直接操作内存更快。
 *
 *   安全性: 虽然返回了可修改指针, 但调用者通常是同一个
 *   模块(svc_lcd.c), 不会出现多线程同时修改的问题。
 */

lcd_home_ui_data_t *lcd_ui_data_get_home_mutable(void)
{
    return &g_lcd_home_ui;
}

lcd_text_msg_t *lcd_ui_data_get_text_mutable(void)
{
    return &g_lcd_text_msg;
}

uint16_t *lcd_ui_data_get_overtime_drive_count_mutable(void)
{
    return &g_lcd_overtime_drive_count;
}

uint32_t *lcd_ui_data_get_total_mileage_km_mutable(void)
{
    return &g_lcd_total_mileage_km;
}

uint16_t *lcd_ui_data_get_total_mileage_rem_m_mutable(void)
{
    return &g_lcd_total_mileage_rem_m;
}

rt_bool_t *lcd_ui_core_menu_mode_mutable(void)
{
    return &g_lcd_menu_mode;
}

lcd_page_id_t *lcd_ui_core_current_page_mutable(void)
{
    return &g_lcd_current_page_id;
}

uint8_t *lcd_ui_core_page_selected_mutable(void)
{
    return g_lcd_page_selected;
}

rt_tick_t *lcd_ui_core_page_enter_tick_mutable(void)
{
    return &g_lcd_page_enter_tick;
}

rt_bool_t *lcd_ui_core_need_redraw_mutable(void)
{
    return &g_lcd_need_redraw;
}

lcd_page_id_t *lcd_ui_core_common_ok_return_page_mutable(void)
{
    return &g_lcd_common_ok_return_page;
}

/* ============================================================
 *  页面系统注册
 * ============================================================ */
void lcd_ui_pages_register(const lcd_page_node_t *pages, uint16_t page_count)
{
    g_lcd_pages = pages;           /* 保存页面节点数组指针 */
    g_lcd_page_count = page_count; /* 页面总数 */
}

/* ============================================================
 *  lcd_get_page_node — 根据 ID 获取页面节点
 * ============================================================
 *
 * 页面节点存储在一个数组中, 以 page_id 为索引:
 *   g_lcd_pages[page_id]
 * 这是最快查找方式 (O(1))
 *
 * 边界检查:
 *   page_id >= g_lcd_page_count → 无效 ID, 返回 NULL
 */
const lcd_page_node_t *lcd_get_page_node(lcd_page_id_t page_id)
{
    if ((g_lcd_pages == RT_NULL) || ((uint16_t)page_id >= g_lcd_page_count)) {
        return RT_NULL;
    }

    return &g_lcd_pages[page_id];
}

/* ============================================================
 *  lcd_page_get_depth — 计算页面在树中的深度
 * ============================================================
 *
 * 深度 = 从当前页面向上追溯到根(HOME)经过的层级数
 * 例如: HOME 深度=0, MAIN_MENU 深度=1, DRIVE_RECORD 深度=2
 *
 * 实现: 通过 parent_id 链向上追溯, 直到找不到父节点
 */
uint8_t lcd_page_get_depth(lcd_page_id_t page_id)
{
    uint8_t depth = 0U;
    const lcd_page_node_t *page;

    while ((uint16_t)page_id < g_lcd_page_count) {
        page = lcd_get_page_node(page_id);
        if ((page == RT_NULL) || ((uint16_t)page->parent_id >= g_lcd_page_count)) {
            break;
        }

        depth++;
        page_id = page->parent_id;
    }

    return depth;
}

/* ============================================================
 *  lcd_page_get_select_count — 获取页面的选择项数
 * ============================================================
 *
 * 优先使用 page->select_count (如果 > 0)
 * 否则回退到 page->child_count
 *
 * 为什么有两个表示选择数的字段?
 *   大多数情况下, 选择项数 = 子页面数。
 *   但有些页面(如"设置成功")没有子页面,
 *   其 select_count = 0, 表示不可选择。
 */
uint8_t lcd_page_get_select_count(lcd_page_id_t page_id)
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

/* ============================================================
 *  列表资源注册与查询
 * ============================================================ */
void lcd_ui_list_register(const lcd_ui_list_resource_t *resources, uint8_t resource_count)
{
    g_lcd_ui_list_resources = resources;
    g_lcd_ui_list_resource_count = resource_count;
}

/**
 * 查询指定页面 ID 对应的列表资源
 *
 * 遍历资源数组, 找到 page_id 匹配的项,
 * 通过输出参数返回文本和计数指针
 *
 * @return RT_TRUE = 找到, RT_FALSE = 未找到
 */
rt_bool_t lcd_get_list_page_resources(lcd_page_id_t page_id,
                                      const uint16_t *const **item_texts,
                                      const uint8_t **item_counts,
                                      uint8_t *item_count,
                                      const uint16_t **title_text,
                                      uint8_t *title_count)
{
    uint8_t i;
    const lcd_ui_list_resource_t *resource;

    if ((item_texts == RT_NULL) || (item_counts == RT_NULL) || (item_count == RT_NULL) ||
        (title_text == RT_NULL) || (title_count == RT_NULL)) {
        return RT_FALSE;
    }

    for (i = 0U; i < g_lcd_ui_list_resource_count; i++) {
        resource = &g_lcd_ui_list_resources[i];
        if (resource->page_id != page_id) {
            continue;
        }

        *item_texts = resource->item_texts;
        *item_counts = resource->item_counts;
        *item_count = resource->item_count;
        *title_text = resource->title_text;
        *title_count = resource->title_count;
        return RT_TRUE;
    }

    return RT_FALSE;
}

/* ============================================================
 *  导航钩子注册
 * ============================================================ */
void lcd_ui_nav_set_enter_hook(lcd_ui_nav_enter_hook_t hook)
{
    g_lcd_ui_nav_enter_hook = hook;
}

/*
 * ============================================================
 *  lcd_page_enter — 进入指定页面 (核心导航函数)
 * ============================================================
 *
 * 这个函数是所有页面跳转的入口, 负责:
 *   1) 更新当前页面 ID
 *   2) 调用进入钩子 (如果已注册)
 *   3) 设置菜单模式标志
 *   4) 记录进入时刻 (用于自动返回)
 *   5) 标记需要重绘
 */
void lcd_page_enter(lcd_page_id_t page_id)
{
    if (page_id >= LCD_PAGE_MAX) {
        return;
    }

    g_lcd_current_page_id = page_id;

    /* 调用进入钩子 (svc_lcd 可能用它来做页面进入前的准备工作) */
    if (g_lcd_ui_nav_enter_hook != RT_NULL) {
        g_lcd_ui_nav_enter_hook(page_id);
    }

    g_lcd_menu_mode = (page_id != LCD_PAGE_HOME) ? RT_TRUE : RT_FALSE;
    g_lcd_page_enter_tick = rt_tick_get();           /* 记录进入时刻 */
    g_lcd_need_redraw = RT_TRUE;                     /* 新页面需要重绘 */
}

/*
 * 进入"设置成功"通用页面
 * return_page: 用户确认后返回的页面
 *
 * 这个通用页面用于所有"设置成功"的反馈,
 * 避免每个设置项都要创建一个独立的"成功"页面。
 */
void lcd_page_enter_common_ok(lcd_page_id_t return_page)
{
    if (return_page >= LCD_PAGE_MAX) {
        return_page = LCD_PAGE_HOME;
    }

    g_lcd_common_ok_return_page = return_page;
    lcd_page_enter(LCD_PAGE_COMMON_CONFIG_OK);
}

/*
 * ============================================================
 *  lcd_page_handle_back — 返回上一级
 * ============================================================
 *
 * 返回逻辑:
 *   - 如果在"设置成功"页 → 回到该页的返回目标
 *   - 如果在主页 → 进入主菜单 (主页的"返回"等于进入菜单)
 *   - 其他页面 → 进入父页面
 *   - 父页面无效 → 回到主页 (安全回退)
 */
void lcd_page_handle_back(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);

    if (g_lcd_current_page_id == LCD_PAGE_COMMON_CONFIG_OK) {
        lcd_page_enter(g_lcd_common_ok_return_page);
        return;
    }

    if (g_lcd_current_page_id == LCD_PAGE_HOME) {
        lcd_page_enter(LCD_PAGE_MAIN_MENU);   /* 主页按返回 = 进菜单 */
        return;
    }

    if ((page != RT_NULL) && (page->parent_id < LCD_PAGE_MAX)) {
        lcd_page_enter(page->parent_id);      /* 进入父页面 */
    } else {
        lcd_page_enter(LCD_PAGE_HOME);        /* 安全回退 */
    }
}

/*
 * ============================================================
 *  lcd_page_handle_nav — 上下导航
 * ============================================================
 *
 * delta: +1 (向下), -1 (向上)
 *
 * 边界保护:
 *   不能小于 0
 *   不能大于 (select_count - 1)
 *
 * 导航成功时才设置 need_redraw = TRUE,
 * 否则(已经在最上/最下)不需要重绘
 */
void lcd_page_handle_nav(int8_t delta)
{
    uint8_t select_count = lcd_page_get_select_count(g_lcd_current_page_id);
    uint8_t *selected = &g_lcd_page_selected[g_lcd_current_page_id];

    if (select_count == 0U) {
        return;  /* 没有可选项, 忽略导航 */
    }

    if ((delta < 0) && (*selected > 0U)) {
        (*selected)--;
        g_lcd_need_redraw = RT_TRUE;
    } else if ((delta > 0) && (*selected + 1U < select_count)) {
        (*selected)++;
        g_lcd_need_redraw = RT_TRUE;
    }
}

/*
 * ============================================================
 *  lcd_page_handle_confirm — 确认选择
 * ============================================================
 *
 * 处理顺序:
 *   1) 如果是 ACTION_RESULT 类型 → 自动返回
 *   2) 如果有 on_confirm 回调 → 调用它
 *   3) 如果是 LIST 类型且有子页面 → 进入选中的子页面
 *
 * 这种"先回调、后导航"的顺序:
 *   回调可能执行一些操作(如保存配置), 然后调用
 *   lcd_page_enter() 导航到下一页。回调内部的
 *   lcd_page_enter() 调用会覆盖这里的导航逻辑。
 */
void lcd_page_handle_confirm(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);
    uint8_t selected_index;

    if (page == RT_NULL) {
        return;
    }

    /* ACTION_RESULT 类型: 无需选择, 直接自动返回 */
    if (page->kind == LCD_PAGE_KIND_ACTION_RESULT) {
        if (g_lcd_current_page_id == LCD_PAGE_COMMON_CONFIG_OK) {
            lcd_page_enter(g_lcd_common_ok_return_page);
        } else if (page->auto_return_target < LCD_PAGE_MAX) {
            lcd_page_enter(page->auto_return_target);
        }
        return;
    }

    /* 有确认回调 → 调用它 */
    if (page->on_confirm != RT_NULL) {
        page->on_confirm();
        return;
    }

    /* LIST 类型 → 进入选中的子页面 */
    if ((page->kind == LCD_PAGE_KIND_LIST) &&
        (page->children != RT_NULL) &&
        (page->child_count > 0U)) {
        selected_index = g_lcd_page_selected[g_lcd_current_page_id];
        if (selected_index < page->child_count) {
            lcd_page_enter(page->children[selected_index]);
        }
    }
}

/*
 * ============================================================
 *  lcd_page_handle_auto_return — 自动返回检测
 * ============================================================
 *
 * 在 svc_lcd 的刷新线程中周期性调用:
 *   如果当前页面配置了 auto_return_ms (超时时间),
 *   且进入当前页后经过的时间 ≥ 超时时间,
 *   则自动跳转到 auto_return_target。
 *
 * 用途: 屏保、超时自动回主页、操作结果自动消失等
 */
void lcd_page_handle_auto_return(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);

    if ((page == RT_NULL) || (page->auto_return_ms == 0U) || (page->auto_return_target >= LCD_PAGE_MAX)) {
        return;
    }

    if ((rt_tick_get() - g_lcd_page_enter_tick) >= rt_tick_from_millisecond(page->auto_return_ms)) {
        lcd_page_enter(page->auto_return_target);
    }
}

/*
 * ============================================================
 *  lcd_vehicle_status_pack_bits — 打包车辆状态到位域
 * ============================================================
 *
 * 将车辆 IO 状态转换为 16 位位域:
 *   bit 0: WK_ACC, bit 1: WK_ON, bits 2~11: KL1~KL10
 *
 * 用于检测车辆状态变化 (只在"设备状态→车辆状态"页面使用)
 */
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

/*
 * ============================================================
 *  lcd_page_handle_dynamic_refresh — 动态刷新处理
 * ============================================================
 *
 * 目前只对"车辆状态"页面做动态更新:
 *   每 100ms 检查一次车辆 IO 状态是否变化,
 *   如果变化就标记需要重绘。
 *
 * 为什么只有这个页面需要动态刷新?
 *   主页的刷新是由 svc_lcd 线程(50ms 周期)驱动的,
 *   "设备状态→版本信息"等静态页面只需要绘制一次。
 *   只有"车辆状态"页显示的是实时变化的开关量, 需要动态更新。
 *
 * 为什么每 100ms 而不是实时?
 *   车辆状态(开关量)的变化速度远低于 100ms,
 *   100ms 的轮询间隔已经足够, 而且不会浪费 CPU
 */
void lcd_page_handle_dynamic_refresh(void)
{
    rt_tick_t now;
    const app_vehicle_io_state_t *state;
    uint16_t bits;

    /* 只在"设备状态→车辆状态"页面时执行 */
    if (g_lcd_current_page_id != LCD_PAGE_DEVICE_STATUS_VEHICLE) {
        g_lcd_vehicle_status_last_bits = 0xFFFFU;  /* 离开页面时重置检测状态 */
        return;
    }

    /* 100ms 检测间隔 */
    now = rt_tick_get();
    if ((now - g_lcd_vehicle_status_check_tick) < rt_tick_from_millisecond(100U)) {
        return;
    }
    g_lcd_vehicle_status_check_tick = now;

    /* 获取当前车辆状态 */
    state = svc_vehicle_io_get_state();
    if (state == RT_NULL) {
        return;
    }

    bits = lcd_vehicle_status_pack_bits(state);

    /* 首次进入页面时, 只记录不重绘 */
    if (g_lcd_vehicle_status_last_bits == 0xFFFFU) {
        g_lcd_vehicle_status_last_bits = bits;
        return;
    }

    /* 状态变化 → 需要重绘 */
    if (bits != g_lcd_vehicle_status_last_bits) {
        g_lcd_vehicle_status_last_bits = bits;
        g_lcd_need_redraw = RT_TRUE;
    }
}

/* ============================================================
 *  渲染调度
 * ============================================================ */
void lcd_ui_render_set_fallback(lcd_ui_render_fallback_t fallback)
{
    g_lcd_ui_render_fallback = fallback;
}

/**
 * lcd_render_current_page — 渲染当前页面
 *
 * 优先使用页面节点中的 render 函数指针,
 * 如果当前页面没有 render, 使用 fallback 渲染函数。
 *
 * fallback 是 svc_lcd 提供的"通用列表渲染器",
 * 用于所有没有独立 render 的菜单列表页面。
 */
void lcd_render_current_page(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);

    if ((page != RT_NULL) && (page->render != RT_NULL)) {
        page->render();          /* 使用页面自己的渲染函数 */
        return;
    }

    if (g_lcd_ui_render_fallback != RT_NULL) {
        g_lcd_ui_render_fallback();  /* 使用通用渲染函数 */
    }
}

/*
 * ============================================================
 *  u8g2 Unicode 文本绘制辅助函数
 * ============================================================
 *
 * 为什么需要这些辅助函数?
 *   u8g2 的 DrawGlyph() 函数可以绘制单个 Unicode 码点,
 *   但绘制中文字符串时, 需要将 GB2312 编码的文本转换成
 *   Unicode 码点序列, 然后逐个绘制。
 *
 *   这些 Unicode 码点序列预存在代码中 (在 svc_lcd.c 中的
 *   g_menu_title_u8g2[] 等数组), 以 uint16_t 数组的形式存储。
 *
 * draw_unicode_seq: 绘制后返回下一个 x 坐标 (便于链式调用)
 * get_unicode_seq_width: 计算宽度 (用于居中、对齐等布局)
 */

/**
 * 绘制 Unicode 码点序列
 * @param u8g2  u8g2 对象指针
 * @param x     起始 x 坐标
 * @param y     基线 y 坐标
 * @param codes Unicode 码点数组
 * @param count 码点数量
 * @return 绘制结束后的 x 坐标
 */
uint16_t lcd_u8g2_draw_unicode_seq(u8g2_t *u8g2,
                                   uint16_t x,
                                   uint16_t y,
                                   const uint16_t *codes,
                                   uint8_t count)
{
    uint8_t i;

    for (i = 0U; i < count; i++) {
        x += u8g2_DrawGlyph(u8g2, x, y, codes[i]);
    }

    return x;
}

/**
 * 计算 Unicode 码点序列的像素宽度
 */
uint16_t lcd_u8g2_get_unicode_seq_width(u8g2_t *u8g2,
                                        const uint16_t *codes,
                                        uint8_t count)
{
    uint16_t width = 0U;
    uint8_t i;

    if ((u8g2 == 0) || (codes == 0)) {
        return 0U;
    }

    for (i = 0U; i < count; i++) {
        width += u8g2_GetGlyphWidth(u8g2, codes[i]);
    }

    return width;
}

/*
 * ============================================================
 *  lcd_timestamp_to_local_ymdhm — Unix 时间戳转本地时间
 * ============================================================
 *
 * 使用经典的"civil_from_days"算法 (Howard Hinnant 的日期算法):
 *   - 不需要浮点运算
 *   - 不需要大表 (如每月的天数表)
 *   - 纯整数运算, 适合 MCU
 *   - 支持公元 0 年至 9999 年
 *
 * 时区处理:
 *   北京时间 (UTC+8), 所以 timestamp 先加 8×3600 秒
 *
 * 注意: 使用 int32_t 做中间计算, 避免 uint32_t 的借位问题
 *   era 计算中涉及负数, 所以用 int32_t
 */
void lcd_timestamp_to_local_ymdhm(uint32_t timestamp,
                                  uint16_t *year,
                                  uint8_t *month,
                                  uint8_t *day,
                                  uint8_t *hour,
                                  uint8_t *minute)
{
    uint32_t local_seconds;
    uint32_t days;
    uint32_t secs_of_day;
    int32_t z;
    int32_t era;
    uint32_t doe;
    uint32_t yoe;
    uint32_t doy;
    uint32_t mp;
    uint32_t d;
    uint32_t m;
    uint32_t y;

    /* 北京时间 = UTC + 8 小时 */
    local_seconds = timestamp + 8U * 3600U;
    days = local_seconds / 86400U;
    secs_of_day = local_seconds % 86400U;

    /* Howard Hinnant 的 civil_from_days 算法 */
    z = (int32_t)days + 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (uint32_t)(z - era * 146097);
    yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
    y = yoe + (uint32_t)era * 400U;
    doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
    mp = (5U * doy + 2U) / 153U;
    d = doy - (153U * mp + 2U) / 5U + 1U;
    if (mp < 10U) {
        m = mp + 3U;
    } else {
        m = mp - 9U;
        y += 1U;
    }

    /* 输出 (调用者可能不需要所有字段, 允许传 NULL) */
    if (year != 0) {
        *year = (uint16_t)y;
    }
    if (month != 0) {
        *month = (uint8_t)m;
    }
    if (day != 0) {
        *day = (uint8_t)d;
    }
    if (hour != 0) {
        *hour = (uint8_t)(secs_of_day / 3600U);
    }
    if (minute != 0) {
        *minute = (uint8_t)((secs_of_day % 3600U) / 60U);
    }
}
