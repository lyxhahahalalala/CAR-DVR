#ifndef APPLICATIONS_SVC_LCD_H_
#define APPLICATIONS_SVC_LCD_H_

#include <rtthread.h>

int svc_lcd_init(void);
int svc_lcd_task_start(void);

void lcd_reset(void);
void lcd_backlight_on(void);
void lcd_backlight_off(void);
void lcd_a0_set(rt_bool_t is_data);
void lcd_csn_set(rt_bool_t active);

#endif /* APPLICATIONS_SVC_LCD_H_ */
