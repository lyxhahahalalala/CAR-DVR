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


void lcd_rst_set(rt_bool_t active);
void lcd_spi_send_byte(uint8_t byte);

void lcd_fb_public_clear(void);
void lcd_fb_public_set_pixel(uint8_t x, uint8_t y, rt_bool_t on);
void lcd_fb_public_flush(void);
void lcd_fb_public_copy_pages(const uint8_t *src, uint16_t src_stride);
void svc_lcd_update_home_time(uint8_t hour, uint8_t minute, uint8_t second);

void svc_lcd_update_top_status(uint8_t value);
void svc_lcd_update_home_speed(uint16_t speed_kmh_x10);
void svc_lcd_update_drive_time(uint8_t hour, uint8_t minute, uint8_t second);
void svc_lcd_update_card_id(const char *card_id);
void svc_lcd_update_overtime_drive_count(uint16_t count);



#endif /* APPLICATIONS_SVC_LCD_H_ */
