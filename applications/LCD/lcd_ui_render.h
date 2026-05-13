#ifndef LCD_UI_RENDER_H
#define LCD_UI_RENDER_H

typedef void (*lcd_ui_render_fallback_t)(void);

void lcd_ui_render_set_fallback(lcd_ui_render_fallback_t fallback);
void lcd_render_current_page(void);

#endif /* LCD_UI_RENDER_H */
