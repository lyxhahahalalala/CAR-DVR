#ifndef LCD_UI_PAGES_H
#define LCD_UI_PAGES_H

#include <rtthread.h>
#include "lcd_ui_core.h"

void lcd_ui_pages_register(const lcd_page_node_t *pages, uint16_t page_count);
const lcd_page_node_t *lcd_get_page_node(lcd_page_id_t page_id);
uint8_t lcd_page_get_depth(lcd_page_id_t page_id);
uint8_t lcd_page_get_select_count(lcd_page_id_t page_id);

#endif /* LCD_UI_PAGES_H */
