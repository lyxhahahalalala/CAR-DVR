#include <rtthread.h>

#include "lcd_ui_core.h"
#include "svc_vehicle_io.h"

static rt_tick_t g_lcd_vehicle_status_check_tick = 0U;
static uint16_t g_lcd_vehicle_status_last_bits = 0xFFFFU;

#define g_lcd_current_page_id        (*lcd_ui_core_current_page_mutable())
#define g_lcd_need_redraw            (*lcd_ui_core_need_redraw_mutable())

static uint16_t lcd_vehicle_status_pack_bits(const app_vehicle_io_state_t *state)
{
    uint16_t bits = 0U;

    bits |= (state->wk_acc  ? (1U << 0) : 0U);
    bits |= (state->wk_on   ? (1U << 1) : 0U);
    bits |= (state->sw_kl1  ? (1U << 2) : 0U);
    bits |= (state->sw_kl2  ? (1U << 3) : 0U);
    bits |= (state->sw_kl3  ? (1U << 4) : 0U);
    bits |= (state->sw_kl4  ? (1U << 5) : 0U);
    bits |= (state->sw_kl5  ? (1U << 6) : 0U);
    bits |= (state->sw_kl6  ? (1U << 7) : 0U);
    bits |= (state->sw_kl7  ? (1U << 8) : 0U);
    bits |= (state->sw_kl8  ? (1U << 9) : 0U);
    bits |= (state->sw_kl9  ? (1U << 10) : 0U);
    bits |= (state->sw_kl10 ? (1U << 11) : 0U);

    return bits;
}

void lcd_page_handle_dynamic_refresh(void)
{
    rt_tick_t now;
    const app_vehicle_io_state_t *state;
    uint16_t bits;

    if (g_lcd_current_page_id != LCD_PAGE_DEVICE_STATUS_VEHICLE) {
        g_lcd_vehicle_status_last_bits = 0xFFFFU;
        return;
    }

    now = rt_tick_get();
    if ((now - g_lcd_vehicle_status_check_tick) < rt_tick_from_millisecond(100U)) {
        return;
    }
    g_lcd_vehicle_status_check_tick = now;

    state = svc_vehicle_io_get_state();
    if (state == RT_NULL) {
        return;
    }

    bits = lcd_vehicle_status_pack_bits(state);

    if (g_lcd_vehicle_status_last_bits == 0xFFFFU) {
        g_lcd_vehicle_status_last_bits = bits;
        return;
    }

    if (bits != g_lcd_vehicle_status_last_bits) {
        g_lcd_vehicle_status_last_bits = bits;
        g_lcd_need_redraw = RT_TRUE;
    }
}
