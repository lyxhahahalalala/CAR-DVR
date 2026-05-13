#include "lcd_ui_list.h"

static const lcd_ui_list_resource_t *g_lcd_ui_list_resources;
static uint8_t g_lcd_ui_list_resource_count;

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
