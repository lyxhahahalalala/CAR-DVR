#include <rtthread.h>

#include "app_config.h"
#include "svc_adc.h"
#include "svc_power.h"
#include "svc_vehicle_io.h"

/*
 * 当前先围绕整机电源主轴做第一版状态建模：
 * 1. BAT24 是否存在
 * 2. SUPER_C_5V 是否还有缓冲能力
 * 3. ACC / ON 当前是否有效
 * 锂电先作为附带监控项保留在输出里。
 */
#define SVC_POWER_MAIN_PRESENT_THRESHOLD_MV      18000UL
#define SVC_POWER_SUPERCAP_HOLD_THRESHOLD_MV     4200UL

typedef enum
{
    SVC_POWER_STAGE_UNKNOWN = 0,
    SVC_POWER_STAGE_MAIN_OFF,
    SVC_POWER_STAGE_STANDBY,
    SVC_POWER_STAGE_ACC_ACTIVE,
    SVC_POWER_STAGE_ON_ACTIVE,
    SVC_POWER_STAGE_SUPERCAP_HOLD
} svc_power_stage_t;

static const char *svc_power_stage_to_str(svc_power_stage_t stage)
{
    switch (stage)
    {
    case SVC_POWER_STAGE_MAIN_OFF:
        return "MAIN_OFF";

    case SVC_POWER_STAGE_STANDBY:
        return "STANDBY";

    case SVC_POWER_STAGE_ACC_ACTIVE:
        return "ACC_ACTIVE";

    case SVC_POWER_STAGE_ON_ACTIVE:
        return "ON_ACTIVE";

    case SVC_POWER_STAGE_SUPERCAP_HOLD:
        return "SUPERCAP_HOLD";

    case SVC_POWER_STAGE_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

static svc_power_stage_t svc_power_eval_stage(const app_adc_snapshot_t *adc_snapshot,
                                              const app_vehicle_io_state_t *vehicle_state)
{
    rt_bool_t main_present;
    rt_bool_t supercap_has_hold;

    main_present = (adc_snapshot->est_bat24_mv >= SVC_POWER_MAIN_PRESENT_THRESHOLD_MV);
    supercap_has_hold = (adc_snapshot->est_super_c_mv >= SVC_POWER_SUPERCAP_HOLD_THRESHOLD_MV);

    if (main_present)
    {
        if (vehicle_state->wk_on != 0U)
        {
            return SVC_POWER_STAGE_ON_ACTIVE;
        }

        if (vehicle_state->wk_acc != 0U)
        {
            return SVC_POWER_STAGE_ACC_ACTIVE;
        }

        return SVC_POWER_STAGE_STANDBY;
    }

    if (supercap_has_hold)
    {
        return SVC_POWER_STAGE_SUPERCAP_HOLD;
    }

    return SVC_POWER_STAGE_MAIN_OFF;
}

static void svc_power_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        const app_adc_snapshot_t *adc_snapshot;
        const app_vehicle_io_state_t *vehicle_state;
        svc_power_stage_t power_stage;

        adc_snapshot = svc_adc_get_snapshot();
        vehicle_state = svc_vehicle_io_get_state();
        power_stage = svc_power_eval_stage(adc_snapshot, vehicle_state);

        rt_kprintf("PWR: BAT24=%lumV SUPER=%lumV ACC=%d ON=%d LI=%lumV CHRG=%d STDBY=%d STAGE=%s\r\n",
                   adc_snapshot->est_bat24_mv,
                   adc_snapshot->est_super_c_mv,
                   vehicle_state->wk_acc,
                   vehicle_state->wk_on,
                   vehicle_state->li_bat_est_mv,
                   vehicle_state->li_bat_chrg,
                   vehicle_state->li_bat_stdby,
                   svc_power_stage_to_str(power_stage));
        rt_thread_mdelay(APP_POWER_TASK_PERIOD_MS);
    }
}

int svc_power_init(void)
{
    /* 当前阶段先保留统一初始化入口。 */
    return RT_EOK;
}

int svc_power_task_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(APP_POWER_TASK_NAME,
                              svc_power_thread_entry,
                              RT_NULL,
                              APP_POWER_TASK_STACK_SIZE,
                              APP_POWER_TASK_PRIORITY,
                              APP_POWER_TASK_TICK);
    if (thread == RT_NULL)
    {
        rt_kprintf("power thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}