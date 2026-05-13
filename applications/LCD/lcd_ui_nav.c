#include "lcd_ui_nav.h"
#include "lcd_ui_pages.h"

static lcd_ui_nav_enter_hook_t g_lcd_ui_nav_enter_hook;

#define g_lcd_menu_mode              (*lcd_ui_core_menu_mode_mutable())
#define g_lcd_current_page_id        (*lcd_ui_core_current_page_mutable())
#define g_lcd_page_selected          (lcd_ui_core_page_selected_mutable())
#define g_lcd_page_enter_tick        (*lcd_ui_core_page_enter_tick_mutable())
#define g_lcd_need_redraw            (*lcd_ui_core_need_redraw_mutable())
#define g_lcd_common_ok_return_page  (*lcd_ui_core_common_ok_return_page_mutable())

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
