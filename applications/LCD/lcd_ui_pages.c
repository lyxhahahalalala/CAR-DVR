#include "lcd_ui_pages.h"

static const lcd_page_node_t *g_lcd_pages;
static uint16_t g_lcd_page_count;

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
