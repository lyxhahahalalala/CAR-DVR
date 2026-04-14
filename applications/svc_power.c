#include <rtthread.h>

#include "board.h"
#include "hpm_gpio_drv.h"
#include "hpm_gpiom_drv.h"
#include "hpm_gpiom_soc_drv.h"
#include "app_config.h"
#include "svc_adc.h"
#include "svc_lcd.h"
#include "svc_power.h"
#include "svc_vehicle_io.h"

/* 电源服务：管理主电、ACC/ON、SOC_EN、PWR_HOLD 和关机流程。 */

#define SVC_POWER_CTRL_GPIO              HPM_GPIO0

#define SVC_POWER_CTRL_GPIO_INDEX        GPIO_DO_GPIOC

#define SVC_POWER_CTRL_PWREN_24V_PIN     11

#define SVC_POWER_CTRL_PWREN_SOC_PIN     12

#define SVC_POWER_CTRL_SUPERCAP_CHRG_PIN 13

#define SVC_POWER_CTRL_PWR_HOLD_PIN      14

#define SVC_POWER_WK_ON_GPIO_CTRL         HPM_GPIO0
#define SVC_POWER_WK_ON_GPIO_INDEX        GPIO_DI_GPIOC
#define SVC_POWER_WK_ON_PIN               19

typedef enum
{
    
    SVC_POWER_STAGE_UNKNOWN = 0,

    
    SVC_POWER_STAGE_MAIN_OFF,

    
    SVC_POWER_STAGE_STANDBY,

    
    SVC_POWER_STAGE_ACC_ACTIVE,

    
    SVC_POWER_STAGE_ON_ACTIVE,

    
    SVC_POWER_STAGE_SUPERCAP_HOLD,

    
    SVC_POWER_STAGE_SHUTDOWN_PENDING,

    
    SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS
} svc_power_stage_t;

static svc_power_stage_t g_power_stage = SVC_POWER_STAGE_UNKNOWN;

static rt_tick_t g_supercap_hold_enter_tick = 0;

static rt_tick_t g_shutdown_pending_enter_tick = 0;

static rt_bool_t g_power_loss_latched = RT_FALSE;

static rt_bool_t g_shutdown_prepare_done = RT_FALSE;

static rt_bool_t g_supercap_ready = RT_FALSE;

static rt_bool_t g_prev_main_present = RT_FALSE;

static rt_uint8_t g_main_present_confirm_count = 0;

static rt_uint8_t g_main_loss_confirm_count = 0;

static rt_uint8_t g_supercap_ready_confirm_count = 0;

static rt_uint8_t g_supercap_low_confirm_count = 0;

static rt_bool_t g_final_soc_cut_done = RT_FALSE;

static rt_bool_t g_final_hold_cut_done = RT_FALSE;
static rt_bool_t g_soc_en_state = RT_FALSE;
static rt_tick_t g_wk_on_rise_tick = 0;

void rt_hw_cpu_reset(void);
static rt_bool_t svc_power_is_shutdown_flow_stage(svc_power_stage_t stage);
static rt_uint32_t svc_power_ticks_to_ms(rt_tick_t start_tick);

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

    case SVC_POWER_STAGE_SHUTDOWN_PENDING:
        return "SHUTDOWN_PENDING";

    case SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS:
        return "SHUTDOWN_IN_PROGRESS";

    case SVC_POWER_STAGE_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

static rt_uint8_t svc_power_ms_to_confirm_count(rt_uint32_t time_ms)
{
    rt_uint32_t count;

    
    count = (time_ms + APP_POWER_TASK_PERIOD_MS - 1U) / APP_POWER_TASK_PERIOD_MS;

    
    if (count == 0U)
    {
        count = 1U;
    }

    
    if (count > 255U)
    {
        count = 255U;
    }

    return (rt_uint8_t)count;
}

static void svc_power_update_confirm_counter(rt_bool_t condition, rt_uint8_t *counter)
{
    if (condition)
    {
        if (*counter < 255U)
        {
            (*counter)++;
        }
    }
    else
    {
        *counter = 0U;
    }
}

static void svc_power_init_ctrl_pins(void)
{
    
    HPM_IOC->PAD[IOC_PAD_PC11].FUNC_CTL = IOC_PC11_FUNC_CTL_GPIO_C_11;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 11, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWREN_24V_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_24V_PIN, 1);

    
    HPM_IOC->PAD[IOC_PAD_PC12].FUNC_CTL = IOC_PC12_FUNC_CTL_GPIO_C_12;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 12, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWREN_SOC_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_SOC_PIN, 0);

    
    HPM_IOC->PAD[IOC_PAD_PC13].FUNC_CTL = IOC_PC13_FUNC_CTL_GPIO_C_13;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 13, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN, 1);
#else
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN, 0);
#endif

    
    HPM_IOC->PAD[IOC_PAD_PC14].FUNC_CTL = IOC_PC14_FUNC_CTL_GPIO_C_14;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 14, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWR_HOLD_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWR_HOLD_PIN, 1);
}

static void svc_power_cut_soc_outputs(void)
{
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_SOC_PIN, 0);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_24V_PIN, 0);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN, 0);
#endif
}

static void svc_power_set_soc_en(rt_bool_t enable)
{
    rt_uint8_t level = enable ? 1U : 0U;

    if (g_soc_en_state == enable)
    {
        return;
    }

    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_SOC_PIN, level);
    g_soc_en_state = enable;}

static rt_bool_t svc_power_is_wk_on_raw(void)
{
    rt_uint32_t raw_level;

    raw_level = gpio_read_pin(SVC_POWER_WK_ON_GPIO_CTRL, SVC_POWER_WK_ON_GPIO_INDEX, SVC_POWER_WK_ON_PIN);
    return (raw_level == 0U);
}

static void svc_power_update_soc_en_by_wk_on(svc_power_stage_t stage)
{
    rt_bool_t wk_on;

    wk_on = svc_power_is_wk_on_raw();

    if ((!wk_on) || svc_power_is_shutdown_flow_stage(stage) || (stage == SVC_POWER_STAGE_MAIN_OFF))
    {
        g_wk_on_rise_tick = 0;
        svc_power_set_soc_en(RT_FALSE);
        return;
    }

    if (g_wk_on_rise_tick == 0)
    {
        g_wk_on_rise_tick = rt_tick_get();
    }

    if (svc_power_ticks_to_ms(g_wk_on_rise_tick) >= APP_PWR_SOC_ON_DELAY_MS)
    {
        svc_power_set_soc_en(RT_TRUE);
    }
}

static void svc_power_release_mcu_hold(void)
{
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWR_HOLD_PIN, 0);
}

static rt_bool_t svc_power_is_main_present_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_bat24_mv >= APP_PWR_MAIN_PRESENT_THRESHOLD_MV);
}

static rt_bool_t svc_power_is_supercap_ready_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_super_c_mv >= APP_PWR_SUPERCAP_READY_THRESHOLD_MV);
}

static rt_bool_t svc_power_is_supercap_available_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_super_c_mv >= APP_PWR_SUPERCAP_HOLD_THRESHOLD_MV);
}

static rt_bool_t svc_power_is_supercap_low_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_super_c_mv < APP_PWR_SUPERCAP_SHUTDOWN_THRESHOLD_MV);
}

static rt_bool_t svc_power_is_shutdown_flow_stage(svc_power_stage_t stage)
{
    return ((stage == SVC_POWER_STAGE_SUPERCAP_HOLD)
         || (stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
         || (stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS));
}

static rt_uint32_t svc_power_ticks_to_ms(rt_tick_t start_tick)
{
    rt_tick_t now_tick;
    rt_tick_t delta_tick;

    if (start_tick == 0)
    {
        return 0;
    }

    now_tick = rt_tick_get();
    delta_tick = now_tick - start_tick;

    return (rt_uint32_t)((rt_uint64_t)delta_tick * 1000UL / RT_TICK_PER_SECOND);
}

static rt_uint32_t svc_power_get_hold_elapsed_ms(void)
{
    return svc_power_ticks_to_ms(g_supercap_hold_enter_tick);
}

static rt_uint32_t svc_power_get_shutdown_pending_elapsed_ms(void)
{
    return svc_power_ticks_to_ms(g_shutdown_pending_enter_tick);
}

static svc_power_stage_t svc_power_eval_normal_stage(const app_vehicle_io_state_t *vehicle_state)
{
    if (vehicle_state->wk_on != 0U)
    {
        return SVC_POWER_STAGE_ON_ACTIVE;
    }

    if (vehicle_state->wk_acc != 0U)
    {
        return SVC_POWER_STAGE_ACC_ACTIVE;
    }

    return SVC_POWER_STAGE_SHUTDOWN_PENDING;
}

static svc_power_stage_t svc_power_eval_stage(const app_adc_snapshot_t *adc_snapshot,
                                              const app_vehicle_io_state_t *vehicle_state)
{
    rt_bool_t main_present_raw;
    rt_bool_t main_present;
    rt_bool_t main_falling_edge;
    rt_bool_t main_rising_edge;
    rt_bool_t supercap_available;
    rt_uint8_t main_present_confirm_target;
    rt_uint8_t main_loss_confirm_target;
    rt_uint8_t supercap_ready_confirm_target;
    rt_uint8_t supercap_low_confirm_target;

    
    main_present_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_MAIN_PRESENT_CONFIRM_MS);
    main_loss_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_MAIN_LOSS_CONFIRM_MS);
    supercap_ready_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_SUPERCAP_READY_CONFIRM_MS);
    supercap_low_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_SUPERCAP_LOW_CONFIRM_MS);

    
    main_present_raw = svc_power_is_main_present_raw(adc_snapshot);
    svc_power_update_confirm_counter(main_present_raw, &g_main_present_confirm_count);
    svc_power_update_confirm_counter(!main_present_raw, &g_main_loss_confirm_count);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    svc_power_update_confirm_counter(svc_power_is_supercap_ready_raw(adc_snapshot), &g_supercap_ready_confirm_count);
    svc_power_update_confirm_counter(svc_power_is_supercap_low_raw(adc_snapshot), &g_supercap_low_confirm_count);
#else
    g_supercap_ready_confirm_count = 0U;
    g_supercap_low_confirm_count = 0U;
    g_supercap_ready = RT_FALSE;
#endif

    
    main_present = (g_main_present_confirm_count >= main_present_confirm_target);
    main_falling_edge = (g_prev_main_present == RT_TRUE) && (g_main_loss_confirm_count >= main_loss_confirm_target);
    main_rising_edge = (g_prev_main_present == RT_FALSE) && (main_present == RT_TRUE);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    supercap_available = svc_power_is_supercap_available_raw(adc_snapshot);
#else
    supercap_available = RT_FALSE;
#endif

    
    if (main_present)
    {
        if (main_rising_edge && svc_power_is_shutdown_flow_stage(g_power_stage))
        {            rt_thread_mdelay(APP_PWR_RECOVERY_RESET_DELAY_MS);
            rt_hw_cpu_reset();
        }

        g_power_loss_latched = RT_FALSE;

#if APP_PWR_SUPERCAP_MGMT_ENABLE
        if (g_supercap_ready_confirm_count >= supercap_ready_confirm_target)
        {
            g_supercap_ready = RT_TRUE;
        }
#else
        g_supercap_ready = RT_FALSE;
#endif

        g_prev_main_present = RT_TRUE;
        {
            svc_power_stage_t normal_stage = svc_power_eval_normal_stage(vehicle_state);
            
            if ((normal_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
                && (g_power_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
                && (svc_power_get_shutdown_pending_elapsed_ms() >= APP_PWR_SHUTDOWN_IN_PROGRESS_MS))
            {
                return SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS;
            }
            return normal_stage;
        }
    }

    
    if (g_main_loss_confirm_count >= main_loss_confirm_target)
    {
        g_prev_main_present = RT_FALSE;
    }

    
    if ((main_falling_edge == RT_TRUE) && (g_power_loss_latched == RT_FALSE))
    {
        g_power_loss_latched = RT_TRUE;

        
#if APP_PWR_SUPERCAP_MGMT_ENABLE
        if (g_supercap_ready && supercap_available)
        {
            return SVC_POWER_STAGE_SUPERCAP_HOLD;
        }
#endif

        
        return SVC_POWER_STAGE_SHUTDOWN_PENDING;
    }

    
    if (g_power_loss_latched)
    {
        
        if ((g_power_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
            || (g_power_stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS))
        {
            if (svc_power_get_shutdown_pending_elapsed_ms() >= APP_PWR_SHUTDOWN_IN_PROGRESS_MS)
            {
                return SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS;
            }

            return SVC_POWER_STAGE_SHUTDOWN_PENDING;
        }

        
        if ((g_supercap_low_confirm_count >= supercap_low_confirm_target)
            || (svc_power_get_hold_elapsed_ms() >= APP_PWR_HOLD_PREPARE_TIMEOUT_MS))
        {
            return SVC_POWER_STAGE_SHUTDOWN_PENDING;
        }

#if APP_PWR_SUPERCAP_MGMT_ENABLE
        return SVC_POWER_STAGE_SUPERCAP_HOLD;
#else
        return SVC_POWER_STAGE_SHUTDOWN_PENDING;
#endif
    }

    
    return SVC_POWER_STAGE_MAIN_OFF;
}

static void svc_power_handle_final_power_cut(void)
{
    rt_uint32_t pending_ms;

    if (g_power_stage != SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS)
    {
        return;
    }

    
    pending_ms = svc_power_get_shutdown_pending_elapsed_ms();

    
    if ((g_final_soc_cut_done == RT_FALSE)
        && (pending_ms >= APP_PWR_FINAL_SOC_CUT_DELAY_MS))
    {
        g_final_soc_cut_done = RT_TRUE;
        svc_power_cut_soc_outputs();
#if APP_PWR_SUPERCAP_MGMT_ENABLE
#else
#endif
    }

    
    if ((g_final_hold_cut_done == RT_FALSE)
        && (pending_ms >= APP_PWR_FINAL_HOLD_CUT_DELAY_MS))
    {
        g_final_hold_cut_done = RT_TRUE;
        rt_kprintf("PWR action: release MCU_PWR_HOLD\r\n");
        svc_power_release_mcu_hold();
    }
}

static void svc_power_handle_stage_transition(svc_power_stage_t new_stage)
{
    if (new_stage == g_power_stage)
    {
        return;
    }

    if (new_stage == SVC_POWER_STAGE_SUPERCAP_HOLD)
    {
        
        g_supercap_hold_enter_tick = rt_tick_get();
        g_shutdown_pending_enter_tick = 0;
        g_shutdown_prepare_done = RT_FALSE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;

        
        lcd_backlight_off();    }
    else if ((new_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING) && (g_shutdown_prepare_done == RT_FALSE))
    {
        
        if (g_supercap_hold_enter_tick == 0)
        {
            g_supercap_hold_enter_tick = rt_tick_get();
        }

        g_shutdown_pending_enter_tick = rt_tick_get();
        g_shutdown_prepare_done = RT_TRUE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;

        
        lcd_backlight_off();
        rt_kprintf("PWR event: enter SHUTDOWN_PENDING, hold=%lums ready=%d\r\n",
                        svc_power_get_hold_elapsed_ms(),
                        g_supercap_ready);
    }
    else if ((new_stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS)
          && (g_power_stage != SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS))
    {
        
        rt_kprintf("PWR event: enter SHUTDOWN_IN_PROGRESS, pending=%lums\r\n",
                        svc_power_get_shutdown_pending_elapsed_ms());
    }
    else if ((new_stage == SVC_POWER_STAGE_STANDBY)
          || (new_stage == SVC_POWER_STAGE_ACC_ACTIVE)
          || (new_stage == SVC_POWER_STAGE_ON_ACTIVE)
          || (new_stage == SVC_POWER_STAGE_MAIN_OFF))
    {
        
        g_supercap_hold_enter_tick = 0;
        g_shutdown_pending_enter_tick = 0;
        g_shutdown_prepare_done = RT_FALSE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;
    }

    g_power_stage = new_stage;
}

static void svc_power_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        const app_adc_snapshot_t *adc_snapshot;
        const app_vehicle_io_state_t *vehicle_state;
        svc_power_stage_t power_stage;
        rt_uint32_t hold_ms;

        
        adc_snapshot = svc_adc_get_snapshot();

        
        vehicle_state = svc_vehicle_io_get_state();

        
        power_stage = svc_power_eval_stage(adc_snapshot, vehicle_state);

        
        svc_power_handle_stage_transition(power_stage);

        
        svc_power_handle_final_power_cut();

        
        hold_ms = svc_power_get_hold_elapsed_ms();        svc_power_update_soc_en_by_wk_on(power_stage);

        rt_thread_mdelay(APP_POWER_TASK_PERIOD_MS);
    }
}

int svc_power_init(void)
{
    
    svc_power_init_ctrl_pins();

    
    g_power_stage = SVC_POWER_STAGE_UNKNOWN;
    g_supercap_hold_enter_tick = 0;
    g_shutdown_pending_enter_tick = 0;
    g_power_loss_latched = RT_FALSE;
    g_shutdown_prepare_done = RT_FALSE;
    g_supercap_ready = RT_FALSE;
    g_prev_main_present = RT_FALSE;
    g_main_present_confirm_count = 0;
    g_main_loss_confirm_count = 0;
    g_supercap_ready_confirm_count = 0;
    g_supercap_low_confirm_count = 0;
    g_final_soc_cut_done = RT_FALSE;
    g_final_hold_cut_done = RT_FALSE;
    g_soc_en_state = RT_FALSE;
    g_wk_on_rise_tick = 0;

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
