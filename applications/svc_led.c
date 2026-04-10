#include <rtthread.h>
#include "rtt_board.h"

#include "app_config.h"
#include "svc_led.h"

static void svc_led_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        app_led_write(APP_LED0, APP_LED_ON);
        rt_thread_mdelay(APP_LED_TOGGLE_PERIOD_MS);
        app_led_write(APP_LED0, APP_LED_OFF);
        rt_thread_mdelay(APP_LED_TOGGLE_PERIOD_MS);
    }
}

int svc_led_init(void)
{
    /* LED 依赖板级引脚初始化，这里保留统一服务初始化入口。 */
    return RT_EOK;
}

int svc_led_task_start(void)
{
    rt_thread_t led_thread;

    led_thread = rt_thread_create(APP_LED_TASK_NAME,
                                  svc_led_thread_entry,
                                  RT_NULL,
                                  APP_LED_TASK_STACK_SIZE,
                                  APP_LED_TASK_PRIORITY,
                                  APP_LED_TASK_TICK);
    if (led_thread == RT_NULL)
    {
        rt_kprintf("led thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(led_thread);
    return RT_EOK;
}
