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
int lcd_write_cmd(rt_uint8_t cmd);
int lcd_write_data(const rt_uint8_t *data, rt_size_t len);

#endif /* APPLICATIONS_SVC_LCD_H_ */
