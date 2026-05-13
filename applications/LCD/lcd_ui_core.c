#include "lcd_ui_core.h"

static rt_bool_t g_lcd_menu_mode = RT_FALSE;
static lcd_page_id_t g_lcd_current_page_id = LCD_PAGE_HOME;
static uint8_t g_lcd_page_selected[LCD_PAGE_MAX];
static rt_tick_t g_lcd_page_enter_tick = 0U;
static rt_bool_t g_lcd_need_redraw = RT_TRUE;
static lcd_page_id_t g_lcd_common_ok_return_page = LCD_PAGE_HOME;

void lcd_ui_core_reset(void)
{
    g_lcd_menu_mode = RT_FALSE;
    rt_memset(g_lcd_page_selected, 0, sizeof(g_lcd_page_selected));
    g_lcd_current_page_id = LCD_PAGE_HOME;
    g_lcd_page_enter_tick = 0U;
    g_lcd_need_redraw = RT_TRUE;
    g_lcd_common_ok_return_page = LCD_PAGE_HOME;
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
