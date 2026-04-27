#include <rtthread.h>

#include "app_config.h"
#include "svc_storage.h"


static svc_storage_mileage_t g_storage_mileage_shadow = {
    .odo_km = 0U,
    .odo_rem_m = 0U,
    .reserved = 0U
};


static void svc_storage_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        /* 后续补充参数保存和异步存储逻辑。 */
        //APP_NON_CAN_LOG("444\r\n");
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


rt_bool_t svc_storage_load_mileage(svc_storage_mileage_t *mileage)
{
    if (mileage == RT_NULL) {
        return RT_FALSE;
    }

    *mileage = g_storage_mileage_shadow;
    return RT_TRUE;
}

rt_bool_t svc_storage_save_mileage(const svc_storage_mileage_t *mileage)
{
    if (mileage == RT_NULL) {
        return RT_FALSE;
    }

    g_storage_mileage_shadow = *mileage;

    /*
     * TODO:
     * Replace this shadow write with real EEPROM/NVM write.
     * Example flow:
     * 1. serialize mileage struct
     * 2. write to EEPROM fixed address
     * 3. optionally verify by readback
     */

    return RT_TRUE;
}
