#include "lcd_ui_data.h"

#include <string.h>

static lcd_home_ui_data_t g_lcd_home_ui;
static lcd_text_msg_t g_lcd_text_msg;
static uint16_t g_lcd_overtime_drive_count = 0U;
static uint32_t g_lcd_total_mileage_km = 0U;
static uint16_t g_lcd_total_mileage_rem_m = 0U;

void lcd_ui_data_reset(void)
{
    rt_memset(&g_lcd_home_ui, 0, sizeof(g_lcd_home_ui));
    rt_memset(&g_lcd_text_msg, 0, sizeof(g_lcd_text_msg));

    g_lcd_home_ui.hour = 9U;
    g_lcd_home_ui.minute = 16U;
    g_lcd_home_ui.second = 45U;
    rt_strncpy(g_lcd_home_ui.card_id,
               "000000000000000000",
               sizeof(g_lcd_home_ui.card_id) - 1U);
    g_lcd_home_ui.card_id[sizeof(g_lcd_home_ui.card_id) - 1U] = '\0';

    g_lcd_overtime_drive_count = 0U;
    g_lcd_total_mileage_km = 0U;
    g_lcd_total_mileage_rem_m = 0U;
}

lcd_home_ui_data_t *lcd_ui_data_get_home_mutable(void)
{
    return &g_lcd_home_ui;
}

lcd_text_msg_t *lcd_ui_data_get_text_mutable(void)
{
    return &g_lcd_text_msg;
}

uint16_t *lcd_ui_data_get_overtime_drive_count_mutable(void)
{
    return &g_lcd_overtime_drive_count;
}

uint32_t *lcd_ui_data_get_total_mileage_km_mutable(void)
{
    return &g_lcd_total_mileage_km;
}

uint16_t *lcd_ui_data_get_total_mileage_rem_m_mutable(void)
{
    return &g_lcd_total_mileage_rem_m;
}
