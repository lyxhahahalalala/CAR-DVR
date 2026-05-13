#ifndef LCD_UI_NAV_H
#define LCD_UI_NAV_H

#include <rtthread.h>
#include "lcd_ui_core.h"

typedef void (*lcd_ui_nav_enter_hook_t)(lcd_page_id_t page_id);

void lcd_ui_nav_set_enter_hook(lcd_ui_nav_enter_hook_t hook);
void lcd_page_enter(lcd_page_id_t page_id);
void lcd_page_enter_common_ok(lcd_page_id_t return_page);
void lcd_page_handle_back(void);
void lcd_page_handle_nav(int8_t delta);
void lcd_page_handle_confirm(void);
void lcd_page_handle_auto_return(void);

#endif /* LCD_UI_NAV_H */
