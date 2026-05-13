#ifndef APPLICATIONS_LCD_LCD_UI_DATA_H_
#define APPLICATIONS_LCD_LCD_UI_DATA_H_

#include <rtthread.h>
#include <stdint.h>

#define LCD_TEXT_MSG_MAX_LEN        4096U

typedef struct
{
    uint16_t speed_kmh_x10;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t drive_hour;
    uint8_t drive_minute;
    uint8_t drive_second;
    uint8_t top_status_value;
    uint32_t latitude;
    uint32_t longitude;
    uint32_t timestamp;
    uint8_t latitude_direction;
    uint8_t longitude_direction;
    char card_id[20];
} lcd_home_ui_data_t;

typedef struct
{
    uint8_t valid;
    uint8_t flag;
    uint8_t text_type;
    uint16_t text_len;
    char text[LCD_TEXT_MSG_MAX_LEN + 1U];
    uint16_t page_index;
    uint16_t page_count;
} lcd_text_msg_t;

void lcd_ui_data_reset(void);

lcd_home_ui_data_t *lcd_ui_data_get_home_mutable(void);
lcd_text_msg_t *lcd_ui_data_get_text_mutable(void);
uint16_t *lcd_ui_data_get_overtime_drive_count_mutable(void);
uint32_t *lcd_ui_data_get_total_mileage_km_mutable(void);
uint16_t *lcd_ui_data_get_total_mileage_rem_m_mutable(void);

#endif /* APPLICATIONS_LCD_LCD_UI_DATA_H_ */
