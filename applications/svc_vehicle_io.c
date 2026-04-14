#include <rtthread.h>

#include "board.h"
#include "hpm_gpio_drv.h"
#include "app_config.h"
#include "svc_adc.h"
#include "svc_vehicle_io.h"

/*
 * 车辆输入与锂电状态：
 * PC19 -> WK_ACC
 * PC20 -> WK_ON
 * PB22 -> LI_BAT_STDBY
 * PB23 -> LI_BAT_CHRG
 */
#define WK_ACC_GPIO_CTRL          HPM_GPIO0
#define WK_ACC_GPIO_INDEX         GPIO_DI_GPIOC
#define WK_ACC_PIN                19

#define WK_ON_GPIO_CTRL           HPM_GPIO0
#define WK_ON_GPIO_INDEX          GPIO_DI_GPIOC
#define WK_ON_PIN                 20

#define LI_BAT_STDBY_GPIO_CTRL    HPM_GPIO0
#define LI_BAT_STDBY_GPIO_INDEX   GPIO_DI_GPIOB
#define LI_BAT_STDBY_PIN          22

#define LI_BAT_CHRG_GPIO_CTRL     HPM_GPIO0
#define LI_BAT_CHRG_GPIO_INDEX    GPIO_DI_GPIOB
#define LI_BAT_CHRG_PIN           23

static app_vehicle_io_state_t g_vehicle_io_state;

static void svc_vehicle_io_update_state(void)
{
    const app_adc_snapshot_t *adc_snapshot;

    g_vehicle_io_state.wk_acc = gpio_read_pin(WK_ACC_GPIO_CTRL,
                                              WK_ACC_GPIO_INDEX,
                                              WK_ACC_PIN);
    g_vehicle_io_state.wk_on = gpio_read_pin(WK_ON_GPIO_CTRL,
                                             WK_ON_GPIO_INDEX,
                                             WK_ON_PIN);
    g_vehicle_io_state.li_bat_stdby = gpio_read_pin(LI_BAT_STDBY_GPIO_CTRL,
                                                    LI_BAT_STDBY_GPIO_INDEX,
                                                    LI_BAT_STDBY_PIN);
    g_vehicle_io_state.li_bat_chrg = gpio_read_pin(LI_BAT_CHRG_GPIO_CTRL,
                                                   LI_BAT_CHRG_GPIO_INDEX,
                                                   LI_BAT_CHRG_PIN);

    adc_snapshot = svc_adc_get_snapshot();
    g_vehicle_io_state.li_bat_raw = adc_snapshot->raw_li_bat;
    g_vehicle_io_state.li_bat_est_mv = adc_snapshot->est_li_bat_mv;
}

static void svc_vehicle_io_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        /* 先把电源相关输入收进服务层，后续再扩展其它硬线输入。 */
        svc_vehicle_io_update_state();

        APP_NON_CAN_LOG("333\r\n");
//        APP_NON_CAN_LOG("VIN: ACC=%d ON=%d LI_STDBY=%d LI_CHRG=%d LI_RAW=%lu LI_EST=%lumV\r\n",
//                        g_vehicle_io_state.wk_acc,
//                        g_vehicle_io_state.wk_on,
//                        g_vehicle_io_state.li_bat_stdby,
//                        g_vehicle_io_state.li_bat_chrg,
//                        g_vehicle_io_state.li_bat_raw,
//                        g_vehicle_io_state.li_bat_est_mv);
        rt_thread_mdelay(APP_IO_TASK_PERIOD_MS);
    }
}

int svc_vehicle_io_init(void)
{
    /* 初始化与电源状态相关的输入引脚。 */
    board_init_io_pins();
    init_input_switch_pins();
    rt_memset(&g_vehicle_io_state, 0, sizeof(g_vehicle_io_state));

    return RT_EOK;
}

int svc_vehicle_io_task_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(APP_IO_TASK_NAME,
                              svc_vehicle_io_thread_entry,
                              RT_NULL,
                              APP_IO_TASK_STACK_SIZE,
                              APP_IO_TASK_PRIORITY,
                              APP_IO_TASK_TICK);
    if (thread == RT_NULL)
    {
        APP_NON_CAN_LOG("vehicle io thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}

const app_vehicle_io_state_t *svc_vehicle_io_get_state(void)
{
    return &g_vehicle_io_state;
}
