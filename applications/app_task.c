#include <rtthread.h>

#include "app_task.h"
#include "svc_adc.h"
#include "svc_can.h"
#include "svc_led.h"
#include "svc_lcd.h"
#include "svc_power.h"
#include "svc_storage.h"
#include "svc_vehicle_io.h"

int app_task_start(void)
{
    int result;

    /* 任务调度层只关心服务启动顺序，不再包含具体业务实现。 */
    result = svc_led_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = svc_power_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = svc_can_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = svc_lcd_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = svc_vehicle_io_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    /* ADC 任务交给独立服务模块启动，app_task 只负责统一调度。 */
    result = svc_adc_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = svc_storage_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    return RT_EOK;
}
