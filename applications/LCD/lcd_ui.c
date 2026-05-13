#include "lcd_ui.h"
#include "svc_vehicle_io.h"

static lcd_home_ui_data_t g_lcd_home_ui;
static lcd_text_msg_t g_lcd_text_msg;
static uint16_t g_lcd_overtime_drive_count;
static uint32_t g_lcd_total_mileage_km;
static uint16_t g_lcd_total_mileage_rem_m;

static rt_bool_t g_lcd_menu_mode = RT_FALSE;
static lcd_page_id_t g_lcd_current_page_id = LCD_PAGE_HOME;
static uint8_t g_lcd_page_selected[LCD_PAGE_MAX];
static rt_tick_t g_lcd_page_enter_tick = 0U;
static rt_bool_t g_lcd_need_redraw = RT_TRUE;
static lcd_page_id_t g_lcd_common_ok_return_page = LCD_PAGE_HOME;

static const lcd_page_node_t *g_lcd_pages;
static uint16_t g_lcd_page_count;
static const lcd_ui_list_resource_t *g_lcd_ui_list_resources;
static uint8_t g_lcd_ui_list_resource_count;
static lcd_ui_nav_enter_hook_t g_lcd_ui_nav_enter_hook;
static lcd_ui_render_fallback_t g_lcd_ui_render_fallback;
static rt_tick_t g_lcd_vehicle_status_check_tick = 0U;
static uint16_t g_lcd_vehicle_status_last_bits = 0xFFFFU;

void lcd_ui_data_reset(void)
{
    rt_memset(&g_lcd_home_ui, 0, sizeof(g_lcd_home_ui));
    rt_memset(&g_lcd_text_msg, 0, sizeof(g_lcd_text_msg));

    g_lcd_home_ui.hour = 9U;
    g_lcd_home_ui.minute = 16U;
    g_lcd_home_ui.second = 45U;
    rt_strncpy(g_lcd_home_ui.card_id,
               "000000000000000000",
               sizeof(g_lcd_home_ui.card_id) - 1U);
    g_lcd_home_ui.card_id[sizeof(g_lcd_home_ui.card_id) - 1U] = '\0';

    g_lcd_overtime_drive_count = 0U;
    g_lcd_total_mileage_km = 0U;
    g_lcd_total_mileage_rem_m = 0U;
}

void lcd_ui_core_reset(void)
{
    g_lcd_menu_mode = RT_FALSE;
    rt_memset(g_lcd_page_selected, 0, sizeof(g_lcd_page_selected));
    g_lcd_current_page_id = LCD_PAGE_HOME;
    g_lcd_page_enter_tick = 0U;
    g_lcd_need_redraw = RT_TRUE;
    g_lcd_common_ok_return_page = LCD_PAGE_HOME;
}

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

void lcd_ui_pages_register(const lcd_page_node_t *pages, uint16_t page_count)
{
    g_lcd_pages = pages;
    g_lcd_page_count = page_count;
}

const lcd_page_node_t *lcd_get_page_node(lcd_page_id_t page_id)
{
    if ((g_lcd_pages == RT_NULL) || ((uint16_t)page_id >= g_lcd_page_count)) {
        return RT_NULL;
    }

    return &g_lcd_pages[page_id];
}

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

void lcd_ui_list_register(const lcd_ui_list_resource_t *resources, uint8_t resource_count)
{
    g_lcd_ui_list_resources = resources;
    g_lcd_ui_list_resource_count = resource_count;
}

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

void lcd_ui_nav_set_enter_hook(lcd_ui_nav_enter_hook_t hook)
{
    g_lcd_ui_nav_enter_hook = hook;
}

void lcd_page_enter(lcd_page_id_t page_id)
{
    if (page_id >= LCD_PAGE_MAX) {
        return;
    }

    g_lcd_current_page_id = page_id;

    if (g_lcd_ui_nav_enter_hook != RT_NULL) {
        g_lcd_ui_nav_enter_hook(page_id);
    }

    g_lcd_menu_mode = (page_id != LCD_PAGE_HOME) ? RT_TRUE : RT_FALSE;
    g_lcd_page_enter_tick = rt_tick_get();
    g_lcd_need_redraw = RT_TRUE;
}

void lcd_page_enter_common_ok(lcd_page_id_t return_page)
{
    if (return_page >= LCD_PAGE_MAX) {
        return_page = LCD_PAGE_HOME;
    }

    g_lcd_common_ok_return_page = return_page;
    lcd_page_enter(LCD_PAGE_COMMON_CONFIG_OK);
}

void lcd_page_handle_back(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);

    if (g_lcd_current_page_id == LCD_PAGE_COMMON_CONFIG_OK) {
        lcd_page_enter(g_lcd_common_ok_return_page);
        return;
    }

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

void lcd_page_handle_nav(int8_t delta)
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

void lcd_page_handle_confirm(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);
    uint8_t selected_index;

    if (page == RT_NULL) {
        return;
    }

    if (page->kind == LCD_PAGE_KIND_ACTION_RESULT) {
        if (g_lcd_current_page_id == LCD_PAGE_COMMON_CONFIG_OK) {
            lcd_page_enter(g_lcd_common_ok_return_page);
        } else if (page->auto_return_target < LCD_PAGE_MAX) {
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

void lcd_page_handle_dynamic_refresh(void)
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

void lcd_ui_render_set_fallback(lcd_ui_render_fallback_t fallback)
{
    g_lcd_ui_render_fallback = fallback;
}

void lcd_render_current_page(void)
{
    const lcd_page_node_t *page = lcd_get_page_node(g_lcd_current_page_id);

    if ((page != RT_NULL) && (page->render != RT_NULL)) {
        page->render();
        return;
    }

    if (g_lcd_ui_render_fallback != RT_NULL) {
        g_lcd_ui_render_fallback();
    }
}

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

    local_seconds = timestamp + 8U * 3600U;
    days = local_seconds / 86400U;
    secs_of_day = local_seconds % 86400U;

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
