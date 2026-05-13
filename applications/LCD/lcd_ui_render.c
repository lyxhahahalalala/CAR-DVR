#include <rtthread.h>

#include "lcd_ui_core.h"
#include "lcd_ui_pages.h"
#include "lcd_ui_render.h"

static lcd_ui_render_fallback_t g_lcd_ui_render_fallback;

#define g_lcd_current_page_id        (*lcd_ui_core_current_page_mutable())

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
