#include <rtthread.h>

#include "app_config.h"
#include "svc_storage.h"

static void svc_storage_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        /* 后续补充参数保存和异步存储逻辑。 */
        APP_NON_CAN_LOG("444\r\n");
        rt_thread_mdelay(APP_STORAGE_TASK_PERIOD_MS);
    }
}

int svc_storage_init(void)
{
    /* 当前阶段先保留统一初始化入口。 */
    return RT_EOK;
}

int svc_storage_task_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(APP_STORAGE_TASK_NAME,
                              svc_storage_thread_entry,
                              RT_NULL,
                              APP_STORAGE_TASK_STACK_SIZE,
                              APP_STORAGE_TASK_PRIORITY,
                              APP_STORAGE_TASK_TICK);
    if (thread == RT_NULL)
    {
        APP_NON_CAN_LOG("storage thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}
