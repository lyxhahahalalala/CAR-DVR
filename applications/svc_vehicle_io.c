#include "board.h"
#include "hpm_gpio_drv.h"
#include "app_config.h"
#include "svc_adc.h"
#include "svc_vehicle_io.h"

/*
 * 车辆输入采集：
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

#define SW_KL1_GPIO_CTRL          HPM_GPIO0
#define SW_KL1_GPIO_INDEX         GPIO_DI_GPIOB
#define SW_KL1_PIN                0

#define SW_KL2_GPIO_CTRL          HPM_GPIO0
#define SW_KL2_GPIO_INDEX         GPIO_DI_GPIOB
#define SW_KL2_PIN                1

#define SW_KL3_GPIO_CTRL          HPM_GPIO0
#define SW_KL3_GPIO_INDEX         GPIO_DI_GPIOB
#define SW_KL3_PIN                11

#define SW_KL4_GPIO_CTRL          HPM_GPIO0
#define SW_KL4_GPIO_INDEX         GPIO_DI_GPIOB
#define SW_KL4_PIN                12

#define SW_KL5_GPIO_CTRL          HPM_GPIO0
#define SW_KL5_GPIO_INDEX         GPIO_DI_GPIOB
#define SW_KL5_PIN                13

#define SW_KL6_GPIO_CTRL          HPM_GPIO0
#define SW_KL6_GPIO_INDEX         GPIO_DI_GPIOB
#define SW_KL6_PIN                14

#define SW_KL7_GPIO_CTRL          HPM_GPIO0
#define SW_KL7_GPIO_INDEX         GPIO_DI_GPIOB
#define SW_KL7_PIN                15

#define SW_KL8_GPIO_CTRL          HPM_GPIO0
#define SW_KL8_GPIO_INDEX         GPIO_DI_GPIOB
#define SW_KL8_PIN                16

#define SW_KL9_GPIO_CTRL          HPM_GPIO0
#define SW_KL9_GPIO_INDEX         GPIO_DI_GPIOB
#define SW_KL9_PIN                17

#define SW_KL10_GPIO_CTRL         HPM_GPIO0
#define SW_KL10_GPIO_INDEX        GPIO_DI_GPIOB
#define SW_KL10_PIN               18


static app_vehicle_io_state_t g_vehicle_io_state;

#define SW_KL_DEBUG_ENABLE        1

static rt_uint16_t g_sw_kl_last_bits = 0xFFFFU;

static rt_uint8_t svc_vehicle_io_apply_level(rt_uint32_t raw_level, rt_uint8_t active_low)
{
    if (active_low != 0U) {
        return (raw_level == 0U) ? 1U : 0U;
    }

    return (raw_level != 0U) ? 1U : 0U;
}

#define SW_KL1_ACTIVE_LOW         1U
#define SW_KL2_ACTIVE_LOW         1U
#define SW_KL3_ACTIVE_LOW         1U
#define SW_KL4_ACTIVE_LOW         1U
#define SW_KL5_ACTIVE_LOW         1U
#define SW_KL6_ACTIVE_LOW         1U
#define SW_KL7_ACTIVE_LOW         1U
#define SW_KL8_ACTIVE_LOW         1U
#define SW_KL9_ACTIVE_LOW         0U
#define SW_KL10_ACTIVE_LOW        0U



static void svc_vehicle_io_update_state(void)
{
    const app_adc_snapshot_t *adc_snapshot;
    rt_uint32_t wk_acc_raw;
    rt_uint32_t wk_on_raw;


    /* WK_ACC / WK_ON 当前板级需求为低有效。 */
    wk_acc_raw = gpio_read_pin(WK_ACC_GPIO_CTRL, WK_ACC_GPIO_INDEX, WK_ACC_PIN);
    wk_on_raw  = gpio_read_pin(WK_ON_GPIO_CTRL,  WK_ON_GPIO_INDEX,  WK_ON_PIN);

    g_vehicle_io_state.wk_acc = (wk_acc_raw == 0U) ? 1U : 0U;
    g_vehicle_io_state.wk_on  = (wk_on_raw  == 0U) ? 1U : 0U;




    /* SW_KL1 ~ SW_KL10 */
    g_vehicle_io_state.sw_kl1 = svc_vehicle_io_apply_level(
        gpio_read_pin(SW_KL1_GPIO_CTRL, SW_KL1_GPIO_INDEX, SW_KL1_PIN),
        SW_KL1_ACTIVE_LOW);

    g_vehicle_io_state.sw_kl2 = svc_vehicle_io_apply_level(
        gpio_read_pin(SW_KL2_GPIO_CTRL, SW_KL2_GPIO_INDEX, SW_KL2_PIN),
        SW_KL2_ACTIVE_LOW);

    g_vehicle_io_state.sw_kl3 = svc_vehicle_io_apply_level(
        gpio_read_pin(SW_KL3_GPIO_CTRL, SW_KL3_GPIO_INDEX, SW_KL3_PIN),
        SW_KL3_ACTIVE_LOW);

    g_vehicle_io_state.sw_kl4 = svc_vehicle_io_apply_level(
        gpio_read_pin(SW_KL4_GPIO_CTRL, SW_KL4_GPIO_INDEX, SW_KL4_PIN),
        SW_KL4_ACTIVE_LOW);

    g_vehicle_io_state.sw_kl5 = svc_vehicle_io_apply_level(
        gpio_read_pin(SW_KL5_GPIO_CTRL, SW_KL5_GPIO_INDEX, SW_KL5_PIN),
        SW_KL5_ACTIVE_LOW);

    g_vehicle_io_state.sw_kl6 = svc_vehicle_io_apply_level(
        gpio_read_pin(SW_KL6_GPIO_CTRL, SW_KL6_GPIO_INDEX, SW_KL6_PIN),
        SW_KL6_ACTIVE_LOW);

    g_vehicle_io_state.sw_kl7 = svc_vehicle_io_apply_level(
        gpio_read_pin(SW_KL7_GPIO_CTRL, SW_KL7_GPIO_INDEX, SW_KL7_PIN),
        SW_KL7_ACTIVE_LOW);

    g_vehicle_io_state.sw_kl8 = svc_vehicle_io_apply_level(
        gpio_read_pin(SW_KL8_GPIO_CTRL, SW_KL8_GPIO_INDEX, SW_KL8_PIN),
        SW_KL8_ACTIVE_LOW);

    g_vehicle_io_state.sw_kl9 = svc_vehicle_io_apply_level(
        gpio_read_pin(SW_KL9_GPIO_CTRL, SW_KL9_GPIO_INDEX, SW_KL9_PIN),
        SW_KL9_ACTIVE_LOW);

    g_vehicle_io_state.sw_kl10 = svc_vehicle_io_apply_level(
        gpio_read_pin(SW_KL10_GPIO_CTRL, SW_KL10_GPIO_INDEX, SW_KL10_PIN),
        SW_KL10_ACTIVE_LOW);



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


static rt_uint16_t svc_vehicle_io_pack_sw_kl_bits(const app_vehicle_io_state_t *state)
{
    rt_uint16_t bits = 0U;

    bits |= (state->sw_kl1  ? (1U << 0) : 0U);
    bits |= (state->sw_kl2  ? (1U << 1) : 0U);
    bits |= (state->sw_kl3  ? (1U << 2) : 0U);
    bits |= (state->sw_kl4  ? (1U << 3) : 0U);
    bits |= (state->sw_kl5  ? (1U << 4) : 0U);
    bits |= (state->sw_kl6  ? (1U << 5) : 0U);
    bits |= (state->sw_kl7  ? (1U << 6) : 0U);
    bits |= (state->sw_kl8  ? (1U << 7) : 0U);
    bits |= (state->sw_kl9  ? (1U << 8) : 0U);
    bits |= (state->sw_kl10 ? (1U << 9) : 0U);

    return bits;
}

static void svc_vehicle_io_debug_print_sw_kl(void)
{
#if SW_KL_DEBUG_ENABLE
    rt_uint16_t bits = svc_vehicle_io_pack_sw_kl_bits(&g_vehicle_io_state);

    if (bits != g_sw_kl_last_bits) {
        APP_NON_CAN_LOG("SW_KL: 1=%d 2=%d 3=%d 4=%d 5=%d 6=%d 7=%d 8=%d 9=%d 10=%d\r\n",
                        g_vehicle_io_state.sw_kl1,
                        g_vehicle_io_state.sw_kl2,
                        g_vehicle_io_state.sw_kl3,
                        g_vehicle_io_state.sw_kl4,
                        g_vehicle_io_state.sw_kl5,
                        g_vehicle_io_state.sw_kl6,
                        g_vehicle_io_state.sw_kl7,
                        g_vehicle_io_state.sw_kl8,
                        g_vehicle_io_state.sw_kl9,
                        g_vehicle_io_state.sw_kl10);
        g_sw_kl_last_bits = bits;
    }
#endif
}



static void svc_vehicle_io_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        /* 周期更新车辆输入状态，供电源管理和后续业务读取。 */
        svc_vehicle_io_update_state();
        svc_vehicle_io_debug_print_sw_kl();
//      APP_NON_CAN_LOG("333\r\n");
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
    /* 初始化输入脚并清零软件状态。 */
    board_init_io_pins();
    init_input_switch_pins();
    rt_memset(&g_vehicle_io_state, 0, sizeof(g_vehicle_io_state));
    g_sw_kl_last_bits = 0xFFFFU;
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
