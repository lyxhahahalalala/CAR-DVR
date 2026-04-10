#include <rtthread.h>

#include "app_config.h"
#include "svc_can.h"

static void svc_can_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        /* 后续补充 CAN 收发和协议处理逻辑。 */
        rt_kprintf("222\r\n");
        rt_thread_mdelay(APP_CAN_TASK_PERIOD_MS);
    }
}

int svc_can_init(void)
{
    /* 当前阶段先保留统一初始化入口。 */
    return RT_EOK;
}

int svc_can_task_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(APP_CAN_TASK_NAME,
                              svc_can_thread_entry,
                              RT_NULL,
                              APP_CAN_TASK_STACK_SIZE,
                              APP_CAN_TASK_PRIORITY,
                              APP_CAN_TASK_TICK);
    if (thread == RT_NULL)
    {
        rt_kprintf("can thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}
