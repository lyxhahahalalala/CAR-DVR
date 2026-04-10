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

/*
 * 这份文件是当前 MCU 侧的电源管理核心。
 *
 * 它当前解决的主要问题有 5 个：
 * 1. 判断主电 BAT24 是否真的存在，而不是被瞬时抖动误判。
 * 2. 判断超级电容是否已经充到足够支撑掉电收尾。
 * 3. 主电掉电后，让系统按“保持 -> 关机准备 -> 最终断电”顺序推进。
 * 4. 在超容还没放空时如果主电又恢复，主动复位，避免卡在半掉电状态。
 * 5. 在最终关机阶段按顺序拉低电源控制脚，而不是一口气全部拉掉。
 *
 * 当前这版还没有接 SoC 协同关机请求。
 * 也就是说，它现在主要完成的是：
 * “电源状态判断 + 掉电保持 + 最终硬件断电动作”。
 */

/* 下面这 4 个宏定义的是本文件要操作的电源控制脚。 */

/* 电源控制统一都挂在 GPIO0。 */
#define SVC_POWER_CTRL_GPIO              HPM_GPIO0

/* 这些控制脚都在 GPIOC 输出数据寄存器上。 */
#define SVC_POWER_CTRL_GPIO_INDEX        GPIO_DO_GPIOC

/* PC11 -> PWR_24V_EN。 */
#define SVC_POWER_CTRL_PWREN_24V_PIN     11

/* PC12 -> PWR_SOC_EN。 */
#define SVC_POWER_CTRL_PWREN_SOC_PIN     12

/* PC13 -> MCU_SUPER_C_CHRG。 */
#define SVC_POWER_CTRL_SUPERCAP_CHRG_PIN 13

/* PC14 -> MCU_PWR_HOLD。 */
#define SVC_POWER_CTRL_PWR_HOLD_PIN      14

/*
 * 电源状态机阶段定义。
 *
 * 这里故意把阶段拆得比较清楚，是为了让串口打印和后续调试更直观：
 * 1. 正常运行态
 * 2. 超容保持态
 * 3. 关机准备态
 * 4. 最终断电态
 *
 * 这样后面即使再接 SoC 关机请求，也有明确挂载位置。
 */
typedef enum
{
    /* 初始未知态。 */
    SVC_POWER_STAGE_UNKNOWN = 0,

    /* 当前既没有主电，也没有正式进入掉电流程。 */
    SVC_POWER_STAGE_MAIN_OFF,

    /* 主电在线，但 ACC/ON 都未激活。 */
    SVC_POWER_STAGE_STANDBY,

    /* 主电在线，ACC 有效。 */
    SVC_POWER_STAGE_ACC_ACTIVE,

    /* 主电在线，ON 有效。 */
    SVC_POWER_STAGE_ON_ACTIVE,

    /* 主电掉电后，超级电容正在撑住系统。 */
    SVC_POWER_STAGE_SUPERCAP_HOLD,

    /* 已经决定要关机，正在做关机准备。 */
    SVC_POWER_STAGE_SHUTDOWN_PENDING,

    /* 已经进入最终断电动作阶段。 */
    SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS
} svc_power_stage_t;

/* 当前状态机所处阶段。 */
static svc_power_stage_t g_power_stage = SVC_POWER_STAGE_UNKNOWN;

/* 记录进入超级电容保持阶段的 tick，用于换算 HOLD 已持续多久。 */
static rt_tick_t g_supercap_hold_enter_tick = 0;

/* 记录进入 SHUTDOWN_PENDING 的 tick，用于推进最终断电时序。 */
static rt_tick_t g_shutdown_pending_enter_tick = 0;

/*
 * 掉电流程锁存标志。
 *
 * 一旦主电掉电流程正式开始，就置位。
 * 这样做是为了解决：
 * 主电掉了以后，电压继续变化，状态机又错误退回普通运行态的问题。
 */
static rt_bool_t g_power_loss_latched = RT_FALSE;

/* 这个标志保证进入 SHUTDOWN_PENDING 时只做一次入口动作。 */
static rt_bool_t g_shutdown_prepare_done = RT_FALSE;

/*
 * 超级电容 ready 标志。
 *
 * 它表示“这次运行过程中，超容已经充到足够支撑掉电收尾了”。
 * 这样做是为了解决：
 * 刚上电时超容还没充满，却被误当成可支撑掉电收尾的问题。
 */
static rt_bool_t g_supercap_ready = RT_FALSE;

/*
 * 上一拍主电是否存在。
 *
 * 这个量专门用于做掉电沿和恢复沿检测。
 * 因为我们现在不是看“当前有没有主电”这么简单，
 * 而是要区分“主电刚掉了”还是“主电刚恢复了”。
 */
static rt_bool_t g_prev_main_present = RT_FALSE;

/*
 * 下面 4 个计数器全部用于去抖。
 *
 * 原因是车上电压和输入不会像实验室电源那样干净，
 * 如果一拍采样就切状态，状态机会非常容易抖。
 */

/* 主电连续存在确认计数器。 */
static rt_uint8_t g_main_present_confirm_count = 0;

/* 主电连续丢失确认计数器。 */
static rt_uint8_t g_main_loss_confirm_count = 0;

/* 超级电容连续达到 ready 阈值确认计数器。 */
static rt_uint8_t g_supercap_ready_confirm_count = 0;

/* 超级电容连续低于低压阈值确认计数器。 */
static rt_uint8_t g_supercap_low_confirm_count = 0;

/*
 * 下面两个标志用于保证最终断电动作只执行一次。
 *
 * 最终断电被拆成两步：
 * 1. 先切 SoC/24V/超容充电相关输出
 * 2. 再释放 MCU 自身保持
 */

/* 第一步最终断电动作是否已经执行。 */
static rt_bool_t g_final_soc_cut_done = RT_FALSE;

/* 第二步最终断电动作是否已经执行。 */
static rt_bool_t g_final_hold_cut_done = RT_FALSE;

/* 这里显式声明一下复位函数，后面主电恢复时会主动调用。 */
void rt_hw_cpu_reset(void);

/* 把状态枚举转成人能直接看懂的字符串，便于串口打印观察。 */
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

/*
 * 把“毫秒时间”换算成“线程周期计数次数”。
 *
 * 例如：
 * 如果线程周期是 1000ms，确认时间是 3000ms，
 * 那就需要连续 3 次都满足条件，才算确认成立。
 *
 * 这样做是为了把去抖统一成“计数器模型”，后面逻辑更稳定。
 */
static rt_uint8_t svc_power_ms_to_confirm_count(rt_uint32_t time_ms)
{
    rt_uint32_t count;

    /* 向上取整，避免少算一次。 */
    count = (time_ms + APP_POWER_TASK_PERIOD_MS - 1U) / APP_POWER_TASK_PERIOD_MS;

    /* 最少确认 1 次，避免传入 0ms 时逻辑失效。 */
    if (count == 0U)
    {
        count = 1U;
    }

    /* 当前计数器是 8bit，最大只能到 255。 */
    if (count > 255U)
    {
        count = 255U;
    }

    return (rt_uint8_t)count;
}

/*
 * 通用确认计数器更新函数。
 *
 * condition 为真：
 * 计数器累加，表示条件在连续满足。
 *
 * condition 为假：
 * 计数器清零，表示去抖过程被打断，需要重新确认。
 */
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

/*
 * 初始化电源控制脚。
 *
 * 这里的原则是：
 * 先把所有关键控制脚都初始化到一个“明确、可预测”的状态，
 * 不依赖芯片上电默认值。
 *
 * 这样做是为了避免：
 * 板子冷启动、热启动、超容残压启动时，
 * 不同上电路径带来不同默认电平，导致行为不一致。
 */
static void svc_power_init_ctrl_pins(void)
{
    /* PC11 -> PWR_24V_EN，默认先拉高。 */
    HPM_IOC->PAD[IOC_PAD_PC11].FUNC_CTL = IOC_PC11_FUNC_CTL_GPIO_C_11;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 11, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWREN_24V_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_24V_PIN, 1);

    /* PC12 -> PWR_SOC_EN，默认先拉高。 */
    HPM_IOC->PAD[IOC_PAD_PC12].FUNC_CTL = IOC_PC12_FUNC_CTL_GPIO_C_12;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 12, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWREN_SOC_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_SOC_PIN, 1);

    /* PC13 -> MCU_SUPER_C_CHRG，默认允许超级电容充电。 */
    HPM_IOC->PAD[IOC_PAD_PC13].FUNC_CTL = IOC_PC13_FUNC_CTL_GPIO_C_13;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 13, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN, 1);

    /* PC14 -> MCU_PWR_HOLD，默认先保持 MCU。 */
    HPM_IOC->PAD[IOC_PAD_PC14].FUNC_CTL = IOC_PC14_FUNC_CTL_GPIO_C_14;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 14, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWR_HOLD_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWR_HOLD_PIN, 1);
}

/*
 * 第一步最终断电动作。
 *
 * 先切掉：
 * 1. PWR_SOC_EN
 * 2. PWR_24V_EN
 * 3. MCU_SUPER_C_CHRG
 *
 * 这样做是为了先收掉外部和高功耗相关链路，
 * 再由 MCU 自己完成最后的保持释放。
 */
static void svc_power_cut_soc_outputs(void)
{
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_SOC_PIN, 0);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWREN_24V_PIN, 0);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN, 0);
}

/*
 * 第二步最终断电动作。
 *
 * 最后再释放 MCU 自身保持。
 * 这一脚拉低后，MCU 将进入真正掉电。
 */
static void svc_power_release_mcu_hold(void)
{
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIO_INDEX, SVC_POWER_CTRL_PWR_HOLD_PIN, 0);
}

/* 下面这几个 raw 判断函数，全部只做“原始阈值判断”，不带去抖。 */

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

/* 判断当前是否已经处于掉电后半程。 */
static rt_bool_t svc_power_is_shutdown_flow_stage(svc_power_stage_t stage)
{
    return ((stage == SVC_POWER_STAGE_SUPERCAP_HOLD)
         || (stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
         || (stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS));
}

/*
 * 把 tick 差值换算成毫秒。
 * 这里统一封装，后面 hold/pending 时间统计都用同一套算法。
 */
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

/* 当前掉电保持已经持续多久。 */
static rt_uint32_t svc_power_get_hold_elapsed_ms(void)
{
    return svc_power_ticks_to_ms(g_supercap_hold_enter_tick);
}

/* 当前关机准备阶段已经持续多久。 */
static rt_uint32_t svc_power_get_shutdown_pending_elapsed_ms(void)
{
    return svc_power_ticks_to_ms(g_shutdown_pending_enter_tick);
}

/*
 * 主电在线时的正常阶段判断。
 *
 * 这里不再掺杂超容判断，
 * 因为超容在主电在线阶段只负责后台充电和状态观察，
 * 不该反过来阻止系统进入正常运行。
 */
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

    return SVC_POWER_STAGE_STANDBY;
}

/*
 * 电源状态机核心判断函数。
 *
 * 这里做的事情有 4 步：
 * 1. 先更新所有确认计数器
 * 2. 再把原始采样量变成“已确认”的逻辑状态
 * 3. 检测主电掉电沿和恢复沿
 * 4. 决定本轮目标状态机阶段
 */
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

    /* 先把毫秒配置换算成确认次数。 */
    main_present_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_MAIN_PRESENT_CONFIRM_MS);
    main_loss_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_MAIN_LOSS_CONFIRM_MS);
    supercap_ready_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_SUPERCAP_READY_CONFIRM_MS);
    supercap_low_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_SUPERCAP_LOW_CONFIRM_MS);

    /* 更新所有去抖计数器。 */
    main_present_raw = svc_power_is_main_present_raw(adc_snapshot);
    svc_power_update_confirm_counter(main_present_raw, &g_main_present_confirm_count);
    svc_power_update_confirm_counter(!main_present_raw, &g_main_loss_confirm_count);
    svc_power_update_confirm_counter(svc_power_is_supercap_ready_raw(adc_snapshot), &g_supercap_ready_confirm_count);
    svc_power_update_confirm_counter(svc_power_is_supercap_low_raw(adc_snapshot), &g_supercap_low_confirm_count);

    /* 再把原始判断转换成“确认后”的逻辑状态。 */
    main_present = (g_main_present_confirm_count >= main_present_confirm_target);
    main_falling_edge = (g_prev_main_present == RT_TRUE) && (g_main_loss_confirm_count >= main_loss_confirm_target);
    main_rising_edge = (g_prev_main_present == RT_FALSE) && (main_present == RT_TRUE);
    supercap_available = svc_power_is_supercap_available_raw(adc_snapshot);

    /*
     * 主电在线时：
     * 1. 如果当前是掉电流程中来电恢复，就主动复位
     * 2. 如果超容已经充到 ready，就记住
     * 3. 然后回到正常运行态判断
     */
    if (main_present)
    {
        if (main_rising_edge && svc_power_is_shutdown_flow_stage(g_power_stage))
        {
            rt_kprintf("PWR event: main power restored during %s, reset now\r\n",
                       svc_power_stage_to_str(g_power_stage));
            rt_thread_mdelay(APP_PWR_RECOVERY_RESET_DELAY_MS);
            rt_hw_cpu_reset();
        }

        g_power_loss_latched = RT_FALSE;

        if (g_supercap_ready_confirm_count >= supercap_ready_confirm_target)
        {
            g_supercap_ready = RT_TRUE;
        }

        g_prev_main_present = RT_TRUE;
        return svc_power_eval_normal_stage(vehicle_state);
    }

    /* 连续确认到主电丢失后，才把上一拍主电存在标志清掉。 */
    if (g_main_loss_confirm_count >= main_loss_confirm_target)
    {
        g_prev_main_present = RT_FALSE;
    }

    /*
     * 只有真正发生“主电掉电沿”时，才启动掉电流程。
     * 这样做是为了避免刚上电时主电还没稳定、超容也没充好，
     * 状态机却误进掉电流程。
     */
    if ((main_falling_edge == RT_TRUE) && (g_power_loss_latched == RT_FALSE))
    {
        g_power_loss_latched = RT_TRUE;

        /* 如果超容已经 ready 且当前还够，就进保持阶段。 */
        if (g_supercap_ready && supercap_available)
        {
            return SVC_POWER_STAGE_SUPERCAP_HOLD;
        }

        /* 否则直接进关机准备。 */
        return SVC_POWER_STAGE_SHUTDOWN_PENDING;
    }

    /*
     * 掉电流程一旦锁存，就继续沿掉电流程推进，
     * 不允许再回退到普通状态。
     */
    if (g_power_loss_latched)
    {
        /* 如果已经进入关机后半程，就只在 pending 和 in_progress 之间推进。 */
        if ((g_power_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
            || (g_power_stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS))
        {
            if (svc_power_get_shutdown_pending_elapsed_ms() >= APP_PWR_SHUTDOWN_IN_PROGRESS_MS)
            {
                return SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS;
            }

            return SVC_POWER_STAGE_SHUTDOWN_PENDING;
        }

        /* 如果超容连续低压，或者保持时间已经超时，就推进到关机准备。 */
        if ((g_supercap_low_confirm_count >= supercap_low_confirm_target)
            || (svc_power_get_hold_elapsed_ms() >= APP_PWR_HOLD_PREPARE_TIMEOUT_MS))
        {
            return SVC_POWER_STAGE_SHUTDOWN_PENDING;
        }

        /* 其余情况继续保持。 */
        return SVC_POWER_STAGE_SUPERCAP_HOLD;
    }

    /* 既没有主电，也没有正式进入掉电流程时，保持在 MAIN_OFF。 */
    return SVC_POWER_STAGE_MAIN_OFF;
}

/*
 * 最终断电动作执行函数。
 *
 * 当前只在 SHUTDOWN_IN_PROGRESS 阶段执行真正拉脚动作。
 * 这样做是为了把“状态推进”和“动作执行”拆开，后续更容易调时序。
 */
static void svc_power_handle_final_power_cut(void)
{
    rt_uint32_t pending_ms;

    if (g_power_stage != SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS)
    {
        return;
    }

    /* 最终动作的相对时间以进入 pending 的时刻为基准。 */
    pending_ms = svc_power_get_shutdown_pending_elapsed_ms();

    /* 先切 SoC/24V/超容充电相关输出。 */
    if ((g_final_soc_cut_done == RT_FALSE)
        && (pending_ms >= APP_PWR_FINAL_SOC_CUT_DELAY_MS))
    {
        g_final_soc_cut_done = RT_TRUE;
        svc_power_cut_soc_outputs();
        rt_kprintf("PWR action: cut PWR_SOC_EN/PWR_24V_EN/SUPER_C_CHRG\r\n");
    }

    /* 再释放 MCU 保持。 */
    if ((g_final_hold_cut_done == RT_FALSE)
        && (pending_ms >= APP_PWR_FINAL_HOLD_CUT_DELAY_MS))
    {
        g_final_hold_cut_done = RT_TRUE;
        rt_kprintf("PWR action: release MCU_PWR_HOLD\r\n");
        svc_power_release_mcu_hold();
    }
}

/*
 * 状态切换入口动作处理。
 *
 * 注意：
 * 这里处理的是“进入某个阶段时要做什么”，
 * 而不是“每一拍都做什么”。
 * 这样可以避免重复执行入口动作。
 */
static void svc_power_handle_stage_transition(svc_power_stage_t new_stage)
{
    if (new_stage == g_power_stage)
    {
        return;
    }

    if (new_stage == SVC_POWER_STAGE_SUPERCAP_HOLD)
    {
        /* 进入超级电容保持时，记录起点，并清掉后续阶段标志。 */
        g_supercap_hold_enter_tick = rt_tick_get();
        g_shutdown_pending_enter_tick = 0;
        g_shutdown_prepare_done = RT_FALSE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;

        /* 第一时间关背光，降低功耗。 */
        lcd_backlight_off();
        rt_kprintf("PWR event: enter SUPERCAP_HOLD, backlight off\r\n");
    }
    else if ((new_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING) && (g_shutdown_prepare_done == RT_FALSE))
    {
        /*
         * 如果是“主电掉电后直接进 SHUTDOWN_PENDING”，
         * 此时还没有正式进入过 hold，这里补记一个起点，便于日志和计时统一。
         */
        if (g_supercap_hold_enter_tick == 0)
        {
            g_supercap_hold_enter_tick = rt_tick_get();
        }

        g_shutdown_pending_enter_tick = rt_tick_get();
        g_shutdown_prepare_done = RT_TRUE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;

        /* 进入关机准备阶段时也确保背光关闭。 */
        lcd_backlight_off();
        rt_kprintf("PWR event: enter SHUTDOWN_PENDING, hold=%lums ready=%d\r\n",
                   svc_power_get_hold_elapsed_ms(),
                   g_supercap_ready);
    }
    else if ((new_stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS)
          && (g_power_stage != SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS))
    {
        /* 第一次进入最终断电阶段时，打一条日志，便于板级验证时序。 */
        rt_kprintf("PWR event: enter SHUTDOWN_IN_PROGRESS, pending=%lums\r\n",
                   svc_power_get_shutdown_pending_elapsed_ms());
    }
    else if ((new_stage == SVC_POWER_STAGE_STANDBY)
          || (new_stage == SVC_POWER_STAGE_ACC_ACTIVE)
          || (new_stage == SVC_POWER_STAGE_ON_ACTIVE)
          || (new_stage == SVC_POWER_STAGE_MAIN_OFF))
    {
        /* 回到普通状态时，把掉电流程相关变量清掉。 */
        g_supercap_hold_enter_tick = 0;
        g_shutdown_pending_enter_tick = 0;
        g_shutdown_prepare_done = RT_FALSE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;
    }

    g_power_stage = new_stage;
}

/*
 * 电源线程入口。
 *
 * 线程内每一拍做的事情很固定：
 * 1. 读最新快照
 * 2. 评估状态机目标阶段
 * 3. 处理状态切换入口动作
 * 4. 处理最终断电动作
 * 5. 打印当前状态
 */
static void svc_power_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        const app_adc_snapshot_t *adc_snapshot;
        const app_vehicle_io_state_t *vehicle_state;
        svc_power_stage_t power_stage;
        rt_uint32_t hold_ms;

        /* 读取最新 ADC 电源观测快照。 */
        adc_snapshot = svc_adc_get_snapshot();

        /* 读取最新车辆输入与锂电状态快照。 */
        vehicle_state = svc_vehicle_io_get_state();

        /* 用最新输入评估本轮状态机目标阶段。 */
        power_stage = svc_power_eval_stage(adc_snapshot, vehicle_state);

        /* 如果阶段变化了，执行对应的入口动作。 */
        svc_power_handle_stage_transition(power_stage);

        /* 如果已经进入最终断电阶段，则执行真正的拉脚动作。 */
        svc_power_handle_final_power_cut();

        /* 计算当前掉电流程已经持续了多久。 */
        hold_ms = svc_power_get_hold_elapsed_ms();

        /* 统一打印当前电源状态，便于板级联调。 */
        rt_kprintf("PWR: BAT24=%lumV SUPER=%lumV READY=%d ACC=%d ON=%d LI=%lumV CHRG=%d STDBY=%d HOLD=%lums STAGE=%s\r\n",
                   adc_snapshot->est_bat24_mv,
                   adc_snapshot->est_super_c_mv,
                   g_supercap_ready,
                   vehicle_state->wk_acc,
                   vehicle_state->wk_on,
                   vehicle_state->li_bat_est_mv,
                   vehicle_state->li_bat_chrg,
                   vehicle_state->li_bat_stdby,
                   hold_ms,
                   svc_power_stage_to_str(power_stage));
        rt_thread_mdelay(APP_POWER_TASK_PERIOD_MS);
    }
}

int svc_power_init(void)
{
    /*
     * 先初始化所有电源控制脚，
     * 保证每次上电都从统一的脚位状态开始。
     */
    svc_power_init_ctrl_pins();

    /*
     * 再把状态机运行时变量清零。
     * 这样做是为了确保每次启动都从同样的初始上下文开始，
     * 不受上一次运行残留状态影响。
     */
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

    return RT_EOK;
}

int svc_power_task_start(void)
{
    rt_thread_t thread;

    /* 创建电源管理线程。 */
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

    /* 启动电源管理线程。 */
    rt_thread_startup(thread);
    return RT_EOK;
}
