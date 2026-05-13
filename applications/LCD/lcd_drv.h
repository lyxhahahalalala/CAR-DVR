#ifndef APPLICATIONS_LCD_LCD_DRV_H_
#define APPLICATIONS_LCD_LCD_DRV_H_

#include <rtthread.h>
#include <stdint.h>

int lcd_drv_init(void);

void lcd_reset(void);
void lcd_rst_set(rt_bool_t active);
void lcd_backlight_on(void);
void lcd_backlight_off(void);
void lcd_a0_set(rt_bool_t is_data);
void lcd_csn_set(rt_bool_t active);
void lcd_spi_send_byte(uint8_t byte);
void lcd_clear(void);
void lcd_fill_all(void);
void lcd_drv_write_page(uint8_t page, uint8_t col, const uint8_t *buf, uint16_t len);

#endif /* APPLICATIONS_LCD_LCD_DRV_H_ */
