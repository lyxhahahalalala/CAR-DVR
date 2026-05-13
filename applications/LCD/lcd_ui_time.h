#ifndef LCD_UI_TIME_H
#define LCD_UI_TIME_H

#include <stdint.h>

void lcd_timestamp_to_local_ymdhm(uint32_t timestamp,
                                  uint16_t *year,
                                  uint8_t *month,
                                  uint8_t *day,
                                  uint8_t *hour,
                                  uint8_t *minute);

#endif /* LCD_UI_TIME_H */
