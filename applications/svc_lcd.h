#ifndef APPLICATIONS_SVC_LCD_H_
#define APPLICATIONS_SVC_LCD_H_

#include <rtthread.h>

/* 初始化与任务 */
int svc_lcd_init(void);
int svc_lcd_task_start(void);

/* 基础控制 */
void lcd_reset(void);
void lcd_backlight_on(void);
void lcd_backlight_off(void);
void lcd_a0_set(rt_bool_t is_data);
void lcd_csn_set(rt_bool_t active);

/* 显示操作 */
void lcd_clear(void);      /* 清屏（DDRAM 全 0）*/
void lcd_fill_all(void);   /* 全亮（DDRAM 全 0xFF）*/

#endif /* APPLICATIONS_SVC_LCD_H_ */
