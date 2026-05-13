#ifndef LCD_UI_H
#define LCD_UI_H

#include <rtthread.h>
#include <stdint.h>
#include "u8g2.h"

#define LCD_TEXT_MSG_MAX_LEN        4096U

typedef enum
{
    LCD_PAGE_HOME = 0,
    LCD_PAGE_MAIN_MENU,
    LCD_PAGE_DRIVE_RECORD_MENU,
    LCD_PAGE_DEVICE_STATUS_MENU,
    LCD_PAGE_SYSTEM_SETTING_MENU,
    LCD_PAGE_DRIVE_RECORD_FATIGUE,
    LCD_PAGE_DRIVE_RECORD_LOCATION,
    LCD_PAGE_DRIVE_RECORD_MILEAGE,
    LCD_PAGE_DRIVE_RECORD_DRIVER_INFO,
    LCD_PAGE_DRIVE_RECORD_VEHICLE_INFO,
    LCD_PAGE_DRIVE_RECORD_LOAD_STATUS,
    LCD_PAGE_DRIVE_RECORD_LOAD_STATUS_OK,
    LCD_PAGE_DRIVE_RECORD_EXPORT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_BROADCAST,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EVENT_REPORT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_EWAYBILL,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_DISPATCH,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_NORMAL,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_FAULT,
    LCD_PAGE_DRIVE_RECORD_INFO_CENTER_TEXT_EMERGENCY,
    LCD_PAGE_DRIVE_RECORD_VIN,
    LCD_PAGE_DEVICE_STATUS_VERSION,
    LCD_PAGE_DEVICE_STATUS_GFRS,
    LCD_PAGE_DEVICE_STATUS_SELF_TEST,
    LCD_PAGE_DEVICE_STATUS_LOCATION_MODULE,
    LCD_PAGE_DEVICE_STATUS_VEHICLE,
    LCD_PAGE_DEVICE_STATUS_STORAGE,
    LCD_PAGE_DEVICE_STATUS_AV,
    LCD_PAGE_DEVICE_STATUS_SPEED,
    LCD_PAGE_DEVICE_STATUS_DRIVER_MONITOR,
    LCD_PAGE_SYSTEM_SETTING_KEY_VOLUME,
    LCD_PAGE_COMMON_CONFIG_OK,
    LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS,
    LCD_PAGE_SYSTEM_SETTING_BACKLIGHT_TIME,
    LCD_PAGE_SYSTEM_SETTING_BRIGHTNESS_LEVEL,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_VIN,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_SET,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_COLOR,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PLATE_CLASS,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_PROVINCE_ID,
    LCD_PAGE_SYSTEM_SETTING_VEHICLE_INFO_CITY_ID,
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM,
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_LOCAL_PHONE,
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SOS_PHONE,
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SERVER1,
    LCD_PAGE_SYSTEM_SETTING_HOST_PARAM_SERVER2,
    LCD_PAGE_SYSTEM_SETTING_INIT_MILEAGE,
    LCD_PAGE_SYSTEM_SETTING_REGISTER,
    LCD_PAGE_SYSTEM_SETTING_UNREGISTER,
    LCD_PAGE_SYSTEM_SETTING_REC_VER,
    LCD_PAGE_SYSTEM_SETTING_COMPONENT_VER,
    LCD_PAGE_MAX
} lcd_page_id_t;

typedef enum
{
    LCD_PAGE_KIND_VIEW = 0,
    LCD_PAGE_KIND_LIST,
    LCD_PAGE_KIND_ACTION_RESULT
} lcd_page_kind_t;

typedef struct
{
    lcd_page_id_t page_id;
    lcd_page_id_t parent_id;
    lcd_page_kind_t kind;
    const lcd_page_id_t *children;
    uint8_t child_count;
    uint8_t select_count;
    rt_bool_t show_title;
    void (*render)(void);
    void (*on_confirm)(void);
    uint32_t auto_return_ms;
    lcd_page_id_t auto_return_target;
} lcd_page_node_t;

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

typedef struct
{
    lcd_page_id_t page_id;
    const uint16_t *const *item_texts;
    const uint8_t *item_counts;
    uint8_t item_count;
    const uint16_t *title_text;
    uint8_t title_count;
} lcd_ui_list_resource_t;

typedef void (*lcd_ui_nav_enter_hook_t)(lcd_page_id_t page_id);
typedef void (*lcd_ui_render_fallback_t)(void);

void lcd_ui_core_reset(void);
void lcd_ui_data_reset(void);
void lcd_ui_list_register(const lcd_ui_list_resource_t *resources, uint8_t resource_count);
void lcd_ui_pages_register(const lcd_page_node_t *pages, uint16_t page_count);
void lcd_ui_nav_set_enter_hook(lcd_ui_nav_enter_hook_t hook);
void lcd_ui_render_set_fallback(lcd_ui_render_fallback_t fallback);

lcd_home_ui_data_t *lcd_ui_data_get_home_mutable(void);
lcd_text_msg_t *lcd_ui_data_get_text_mutable(void);
uint16_t *lcd_ui_data_get_overtime_drive_count_mutable(void);
uint32_t *lcd_ui_data_get_total_mileage_km_mutable(void);
uint16_t *lcd_ui_data_get_total_mileage_rem_m_mutable(void);

rt_bool_t *lcd_ui_core_menu_mode_mutable(void);
lcd_page_id_t *lcd_ui_core_current_page_mutable(void);
uint8_t *lcd_ui_core_page_selected_mutable(void);
rt_tick_t *lcd_ui_core_page_enter_tick_mutable(void);
rt_bool_t *lcd_ui_core_need_redraw_mutable(void);
lcd_page_id_t *lcd_ui_core_common_ok_return_page_mutable(void);

const lcd_page_node_t *lcd_get_page_node(lcd_page_id_t page_id);
uint8_t lcd_page_get_depth(lcd_page_id_t page_id);
uint8_t lcd_page_get_select_count(lcd_page_id_t page_id);
void lcd_page_enter(lcd_page_id_t page_id);
void lcd_page_enter_common_ok(lcd_page_id_t return_page);
void lcd_page_handle_back(void);
void lcd_page_handle_nav(int8_t delta);
void lcd_page_handle_confirm(void);
void lcd_page_handle_auto_return(void);
void lcd_page_handle_dynamic_refresh(void);
void lcd_render_current_page(void);

rt_bool_t lcd_get_list_page_resources(lcd_page_id_t page_id,
                                      const uint16_t *const **item_texts,
                                      const uint8_t **item_counts,
                                      uint8_t *item_count,
                                      const uint16_t **title_text,
                                      uint8_t *title_count);

uint16_t lcd_u8g2_draw_unicode_seq(u8g2_t *u8g2,
                                   uint16_t x,
                                   uint16_t y,
                                   const uint16_t *codes,
                                   uint8_t count);
uint16_t lcd_u8g2_get_unicode_seq_width(u8g2_t *u8g2,
                                        const uint16_t *codes,
                                        uint8_t count);

void lcd_timestamp_to_local_ymdhm(uint32_t timestamp,
                                  uint16_t *year,
                                  uint8_t *month,
                                  uint8_t *day,
                                  uint8_t *hour,
                                  uint8_t *minute);

#endif /* LCD_UI_H */
