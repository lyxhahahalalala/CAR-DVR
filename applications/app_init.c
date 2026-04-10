#include "app_init.h"

#include <rtthread.h>
#include "rtt_board.h"
#include "svc_adc.h"
#include "svc_can.h"
#include "svc_led.h"
#include "svc_lcd.h"
#include "svc_power.h"
#include "svc_storage.h"
#include "svc_vehicle_io.h"

int app_framework_init(void)
{
    /* 板级初始化先放在这里，后续再逐步扩展其它服务初始化。 */
    app_init_led_pins();

    /* 服务初始化先集中收口到这里。 */
    svc_led_init();
    svc_adc_init();
    svc_lcd_init();
    svc_power_init();
    svc_can_init();
    svc_vehicle_io_init();
    svc_storage_init();

    return RT_EOK;
}
