#ifndef LCD_UI_LIST_H
#define LCD_UI_LIST_H

#include <rtthread.h>
#include "lcd_ui_core.h"

typedef struct
{
    lcd_page_id_t page_id;
    const uint16_t *const *item_texts;
    const uint8_t *item_counts;
    uint8_t item_count;
    const uint16_t *title_text;
    uint8_t title_count;
} lcd_ui_list_resource_t;

void lcd_ui_list_register(const lcd_ui_list_resource_t *resources, uint8_t resource_count);
rt_bool_t lcd_get_list_page_resources(lcd_page_id_t page_id,
                                      const uint16_t *const **item_texts,
                                      const uint8_t **item_counts,
                                      uint8_t *item_count,
                                      const uint16_t **title_text,
                                      uint8_t *title_count);

#endif /* LCD_UI_LIST_H */
