/*
 * svc_power.c — 电源管理服务
 *
 * 这是整个系统最核心的模块，管理 8 阶段电源状态机。
 * 负责：检测主电接入/断开、响应 ACC/ON、控制 SOC 供电、
 *       超级电容管理、CAM 12V 供电、以及最终断电时序。
 *
 * 引脚控制关系：
 *   PC11 → PWREN_24V（24V 主电使能）
 *   PC12 → PWREN_SOC（SOC 系统电源使能）
 *   PC13 → SUPERCAP_CHRG（超级电容充电控制）
 *   PC14 → PWR_HOLD（MCU 自锁保持信号）
 *   PA23 → CAM_12V_EN（摄像头 12V 供电使能）
 *
 * 状态机状态流转：
 *   UNKNOWN → MAIN_OFF ←→ STANDBY ←→ ACC_ACTIVE ←→ ON_ACTIVE
 *                                      ↓ 主电断开
 *                                SUPERCAP_HOLD → SHUTDOWN_PENDING
 *                                                   ↓
 *                                           SHUTDOWN_IN_PROGRESS
 *                                                   ↓
 *                                           最终断电（释放 PWR_HOLD）
 */
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

/* ==================== GPIO 引脚宏定义 ==================== */

#define SVC_POWER_CTRL_GPIO              HPM_GPIO0

#define SVC_POWER_CTRL_GPIOC_INDEX        GPIO_DO_GPIOC
#define SVC_POWER_CTRL_GPIOA_INDEX       GPIO_DO_GPIOA

#define SVC_POWER_CTRL_PWREN_24V_PIN     11   /* PC11：24V 主电使能，输出高电平使后端 DC-DC 工作 */
#define SVC_POWER_CTRL_PWREN_SOC_PIN     12   /* PC12：SOC 系统电源使能，输出高电平让 SOC 上电 */
#define SVC_POWER_CTRL_SUPERCAP_CHRG_PIN 13   /* PC13：超级电容充电控制，高电平允许充电 */
#define SVC_POWER_CTRL_PWR_HOLD_PIN      14   /* PC14：MCU 自锁保持信号，高电平保持系统供电 */
#define SVC_POWER_CTRL_CAM_12V_EN_PIN    23   /* PA23：摄像头 12V 供电使能 */

/*
 * 电源状态机枚举
 *
 * 共 8 个阶段，按运行顺序：
 *   UNKNOWN → MAIN_OFF → STANDBY → ACC_ACTIVE → ON_ACTIVE（正常上行）
 *   ON_ACTIVE/ACC_ACTIVE → SUPERCAP_HOLD → SHUTDOWN_PENDING → SHUTDOWN_IN_PROGRESS（断电下行）
 *
 * 设计思路：把"电源状态"拆得足够细，每个阶段都有明确的进入条件和退出动作，
 * 避免用布尔变量组合来表示状态（那会导致状态爆炸，难以验证）。
 */
typedef enum
{
    /* 初始状态：复位后未确定状态，首次 eval 后会跳出 */
    SVC_POWER_STAGE_UNKNOWN = 0,

    /* 主电断开：没有 24V 输入，系统完全断电 */
    SVC_POWER_STAGE_MAIN_OFF,
    /*
     * 待机：有主电但 ACC 和 ON 都没接通，
     * 此时 SOC 可能未上电，只有 MCU 在低功耗运行
     */
    SVC_POWER_STAGE_STANDBY,
    /*
     * ACC 激活：车辆 ACC 档接通，
     * 附件供电（如点烟器、音响），SOC 需要启动
     */
    SVC_POWER_STAGE_ACC_ACTIVE,
    /*
     * ON 激活：车辆 ON 档接通（全车供电），
     * 此时所有系统全功率运行
     */
    SVC_POWER_STAGE_ON_ACTIVE,
    /*
     * 超级电容保持：主电突然断开，但超级电容还有电，
     * 利用超级电容的电量完成数据保存
     */
    SVC_POWER_STAGE_SUPERCAP_HOLD,
    /*
     * 关机待处理：通知 SOC 准备关机，
     * 等待 SOC 保存数据，超时后强制断电
     */
    SVC_POWER_STAGE_SHUTDOWN_PENDING,
    /*
     * 正在关机中：执行最终断电，
     * 先切 SOC 电源 → 等 1s → 释放 PWR_HOLD → 系统完全掉电
     */
    SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS
} svc_power_stage_t;

/* ---- 全局状态变量（本文件内部使用） ---- */
static svc_power_stage_t g_power_stage = SVC_POWER_STAGE_UNKNOWN;  /* 当前电源阶段 */

/* 各种状态计时器的进入时刻（用于超时判断） */
static rt_tick_t g_supercap_hold_enter_tick = 0;     /* 进入 SUPERCAP_HOLD 的时刻 */
static rt_tick_t g_shutdown_pending_enter_tick = 0;  /* 进入 SHUTDOWN_PENDING 的时刻 */

/* 状态锁存器（用于边缘检测和一次性动作） */
static rt_bool_t g_power_loss_latched = RT_FALSE;    /* 主电丢失已被锁存（防止反复触发） */
static rt_bool_t g_shutdown_prepare_done = RT_FALSE; /* SOC 关机准备已完成 */
static rt_bool_t g_supercap_ready = RT_FALSE;        /* 超级电容已就绪 */

/* 防抖计数器 */
static rt_bool_t g_prev_main_present = RT_FALSE;     /* 上一次的主电存在状态 */
static rt_uint8_t g_main_present_confirm_count = 0;  /* 主电存在确认计数 */
static rt_uint8_t g_main_loss_confirm_count = 0;     /* 主电丢失确认计数 */
static rt_uint8_t g_supercap_ready_confirm_count = 0;/* 超级电容就绪确认计数 */
static rt_uint8_t g_supercap_low_confirm_count = 0;  /* 超级电容电压低确认计数 */

/* 最终断电标志（确保断电动作只执行一次） */
static rt_bool_t g_final_soc_cut_done = RT_FALSE;    /* SOC 电源已切断 */
static rt_bool_t g_final_hold_cut_done = RT_FALSE;   /* PWR_HOLD 已释放 */

/* CAM 12V 供电控制 */
static rt_tick_t g_cam_12v_enable_start_tick = 0;   /* CAM 12V 上电计时起点 */
static rt_bool_t g_cam_12v_enabled = RT_FALSE;       /* CAM 12V 已启用 */


void rt_hw_cpu_reset(void);
static rt_bool_t svc_power_is_shutdown_flow_stage(svc_power_stage_t stage);
static rt_uint32_t svc_power_ticks_to_ms(rt_tick_t start_tick);

/*
 * 状态枚举 → 可读字符串（用于调试日志输出）
 * svc_power_thread_entry() 中调用 rt_kprintf 打印状态切换
 */
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
 * 毫秒 → 确认计数
 *
 * 线程每 APP_POWER_TASK_PERIOD_MS（100ms）执行一次 eval。
 * 防抖需要持续 N 个周期都满足条件才确认，折算为上取整 + 边界保护。
 *
 * 例如：APP_PWR_MAIN_PRESENT_CONFIRM_MS=1000ms, 周期=100ms
 *   → count = (1000+100-1)/100 = 10.99 → 10 次
 *   连续 10 次读到主电存在才确认"主电已接入"
 */
static rt_uint8_t svc_power_ms_to_confirm_count(rt_uint32_t time_ms)
{
    rt_uint32_t count;

    /* 上取整除：确保防抖时间不短于配置值 */
    count = (time_ms + APP_POWER_TASK_PERIOD_MS - 1U) / APP_POWER_TASK_PERIOD_MS;

    /* 防止除零 */
    if (count == 0U)
    {
        count = 1U;
    }

    /* 防止溢出（counter 是 uint8_t） */
    if (count > 255U)
    {
        count = 255U;
    }

    return (rt_uint8_t)count;
}

/*
 * 防抖计数器更新
 *
 * 经典的状态确认模式：
 *   condition 满足 → 计数器加 1（上限 255）
 *   condition 不满足 → 计数器清零
 *
 * 只有连续满足足够多次，counter 才会达到目标阈值。
 * 这是嵌入式中最简单的"软件去抖"方法，比定时器延迟更节省资源。
 *
 * @param condition  当前采样到的条件（如"主电电压是否高于阈值"）
 * @param counter    指向要更新的计数器（传入指针，因为要修改全局变量）
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
 * 初始化电源控制引脚
 *
 * 注意：这里和 rtt_board.c 中的 app_init_led_pins() 都初始化了 PC12/PC13，
 * 存在重复初始化。后续应该统一到 pinmux.c 管理。
 */
static void svc_power_init_ctrl_pins(void)
{
    /* PC11 → PWREN_24V：24V 主电使能，默认输出高电平 */
    HPM_IOC->PAD[IOC_PAD_PC11].FUNC_CTL = IOC_PC11_FUNC_CTL_GPIO_C_11;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 11, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWREN_24V_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIOC_INDEX, SVC_POWER_CTRL_PWREN_24V_PIN, 1);

    /* PC12 → PWREN_SOC：SOC 系统电源使能，默认输出高电平 */
    HPM_IOC->PAD[IOC_PAD_PC12].FUNC_CTL = IOC_PC12_FUNC_CTL_GPIO_C_12;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 12, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWREN_SOC_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIOC_INDEX, SVC_POWER_CTRL_PWREN_SOC_PIN, 1);

    /* PC13 → SUPERCAP_CHRG：超级电容充电控制，默认输出低电平（禁止充电） */
    HPM_IOC->PAD[IOC_PAD_PC13].FUNC_CTL = IOC_PC13_FUNC_CTL_GPIO_C_13;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 13, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIOC_INDEX, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN, 0);

    /* PC14 → PWR_HOLD：MCU 自锁保持，默认输出高电平（保持供电） */
    HPM_IOC->PAD[IOC_PAD_PC14].FUNC_CTL = IOC_PC14_FUNC_CTL_GPIO_C_14;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, 14, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOC, SVC_POWER_CTRL_PWR_HOLD_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIOC_INDEX, SVC_POWER_CTRL_PWR_HOLD_PIN, 1);

    /* PA23 → CAM_12V_EN：摄像头 12V 供电，默认输出低电平（关闭） */
    HPM_IOC->PAD[IOC_PAD_PA23].FUNC_CTL = IOC_PA23_FUNC_CTL_GPIO_A_23;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, SVC_POWER_CTRL_CAM_12V_EN_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(SVC_POWER_CTRL_GPIO, GPIO_OE_GPIOA, SVC_POWER_CTRL_CAM_12V_EN_PIN);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIOA_INDEX, SVC_POWER_CTRL_CAM_12V_EN_PIN, 0);
}

/*
 * 切断 SOC 相关输出
 *
 * 这是最终断电的第一步：先切断 SOC 的供电，让 SOC 完全掉电。
 * 之后 MCU 还在运行（由超级电容供电），等待一段时间后释放 PWR_HOLD。
 *
 * 为什么先切 SOC 再释放 PWR_HOLD？
 *   因为 SOC 掉电时如果 PWR_HOLD 仍有效，系统可能重新上电，
 *   导致 SOC 反复重启。必须确保 SOC 完全掉电后再释放自锁。
 */
static void svc_power_cut_soc_outputs(void)
{
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIOC_INDEX, SVC_POWER_CTRL_PWREN_SOC_PIN, 0);
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIOC_INDEX, SVC_POWER_CTRL_PWREN_24V_PIN, 0);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIOC_INDEX, SVC_POWER_CTRL_SUPERCAP_CHRG_PIN, 0);
#endif
}

/*
 * 释放 MCU 自锁保持信号
 *
 * 这是最终断电的最后一步：PWR_HOLD 输出低电平。
 * 硬件上 PWR_HOLD 连接到系统的自锁电路，一旦拉低，
 * 整个系统的输入电源被切断（除非主电重新接入）。
 *
 * 释放后 MCU 也会掉电，所以这是"临死前"的最后一条指令。
 */
static void svc_power_release_mcu_hold(void)
{
    gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIOC_INDEX, SVC_POWER_CTRL_PWR_HOLD_PIN, 0);
}

/* ==================== 电压检测辅助函数 ==================== */

/* 主电是否存在（单次采样，未经防抖） */
static rt_bool_t svc_power_is_main_present_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_bat24_mv >= APP_PWR_MAIN_PRESENT_THRESHOLD_MV);
}

/* 超级电容是否就绪（单次采样） */
static rt_bool_t svc_power_is_supercap_ready_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_super_c_mv >= APP_PWR_SUPERCAP_READY_THRESHOLD_MV);
}

/* 超级电容是否可用（电压足够支撑数据保存） */
static rt_bool_t svc_power_is_supercap_available_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_super_c_mv >= APP_PWR_SUPERCAP_HOLD_THRESHOLD_MV);
}

/* 超级电容是否电压过低（即将耗尽） */
static rt_bool_t svc_power_is_supercap_low_raw(const app_adc_snapshot_t *adc_snapshot)
{
    return (adc_snapshot->est_super_c_mv < APP_PWR_SUPERCAP_SHUTDOWN_THRESHOLD_MV);
}

/* 判断是否处于"关机流程"中的某个阶段 */
static rt_bool_t svc_power_is_shutdown_flow_stage(svc_power_stage_t stage)
{
    return ((stage == SVC_POWER_STAGE_SUPERCAP_HOLD)
         || (stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
         || (stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS));
}

/* 从某个起始 tick 到当前时间的毫秒数（用于超时判断） */
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

/* 返回在 SUPERCAP_HOLD 阶段已经过了多少毫秒 */
static rt_uint32_t svc_power_get_hold_elapsed_ms(void)
{
    return svc_power_ticks_to_ms(g_supercap_hold_enter_tick);
}

/* 返回在 SHUTDOWN_PENDING 阶段已经过了多少毫秒 */
static rt_uint32_t svc_power_get_shutdown_pending_elapsed_ms(void)
{
    return svc_power_ticks_to_ms(g_shutdown_pending_enter_tick);
}

/*
 * 评估正常工况下的电源阶段
 *
 * 当主电存在且稳定时，根据 ACC/ON 信号决定当前阶段：
 *   ON 接通 → ON_ACTIVE（最高功率）
 *   ACC 接通且 ON 未接通 → ACC_ACTIVE（中等功率）
 *   都未接通 → SHUTDOWN_PENDING（空闲待关机）
 *
 * @param vehicle_state  车辆 IO 状态（包含 ACC/ON 信号）
 * @return 对应的电源阶段
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

    return SVC_POWER_STAGE_SHUTDOWN_PENDING;
}

/*
 * ★★★ 核心函数：评估当前应该处于哪个电源阶段 ★★★
 *
 * 这是电源状态机的主判断逻辑，每次线程循环（100ms）调用一次。
 * 判断流程（按优先级从高到低）：
 *
 *   1. 主电存在且稳定（防抖通过）→ 进入正常评估
 *      - 如果从关机流程恢复 → 触发 CPU 复位
 *      - 正常情况 → 根据 ACC/ON 信号决定 STANDBY/ACC_ACTIVE/ON_ACTIVE
 *
 *   2. 主电丢失已确认（防抖通过）→ 进入断电流程
 *      - 如果有超级电容储能 → SUPERCAP_HOLD（利用电容余电完成数据保存）
 *      - 否则 → 直接进入 SHUTDOWN_PENDING
 *
 *   3. 主电已丢失但防抖未完成 → 保持上次的关机阶段
 *
 *   4. 其他情况 → MAIN_OFF
 *
 * @param adc_snapshot    ADC 采样数据（电压值）
 * @param vehicle_state   车辆 IO 状态（ACC/ON）
 * @return 当前应该处于的阶段
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

    /* 计算防抖目标值（从配置毫秒折算为周期计数） */
    main_present_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_MAIN_PRESENT_CONFIRM_MS);
    main_loss_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_MAIN_LOSS_CONFIRM_MS);
    supercap_ready_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_SUPERCAP_READY_CONFIRM_MS);
    supercap_low_confirm_target = svc_power_ms_to_confirm_count(APP_PWR_SUPERCAP_LOW_CONFIRM_MS);

    /* 更新所有防抖计数器 */
    main_present_raw = svc_power_is_main_present_raw(adc_snapshot);
    svc_power_update_confirm_counter(main_present_raw, &g_main_present_confirm_count);
    svc_power_update_confirm_counter(!main_present_raw, &g_main_loss_confirm_count);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    svc_power_update_confirm_counter(svc_power_is_supercap_ready_raw(adc_snapshot), &g_supercap_ready_confirm_count);
    svc_power_update_confirm_counter(svc_power_is_supercap_low_raw(adc_snapshot), &g_supercap_low_confirm_count);
#else
    /* 超级电容管理关闭时，相关计数器保持清零 */
    g_supercap_ready_confirm_count = 0U;
    g_supercap_low_confirm_count = 0U;
    g_supercap_ready = RT_FALSE;
#endif

    /* 计算防抖后的主电状态和边缘 */
    main_present = (g_main_present_confirm_count >= main_present_confirm_target);
    main_falling_edge = (g_prev_main_present == RT_TRUE) && (g_main_loss_confirm_count >= main_loss_confirm_target);
    main_rising_edge = (g_prev_main_present == RT_FALSE) && (main_present == RT_TRUE);
#if APP_PWR_SUPERCAP_MGMT_ENABLE
    supercap_available = svc_power_is_supercap_available_raw(adc_snapshot);
#else
    supercap_available = RT_FALSE;
#endif

    /*
     * ---- 分支 1：主电存在 ----
     * 如果有主电，按 ACC/ON 信号走正常状态。
     * 特殊处理：如果刚从关机流程恢复（主电重新接入），触发 CPU 复位以确保干净启动。
     */
    if (main_present)
    {
        if (main_rising_edge && svc_power_is_shutdown_flow_stage(g_power_stage))
        {
            /* 主电在关机流程中重新接入 → 延时后软复位，让系统重新初始化 */
            rt_thread_mdelay(APP_PWR_RECOVERY_RESET_DELAY_MS);
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

            /*
             * 特殊判断：如果已经处于 SHUTDOWN_PENDING 且超过了关机超时，
             * 即使主电存在也要进入 SHUTDOWN_IN_PROGRESS（确保最终断电执行）。
             * 这是为了防止 ACC/ON 信号不稳定导致关机流程卡死。
             */
            if ((normal_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
                && (g_power_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
                && (svc_power_get_shutdown_pending_elapsed_ms() >= APP_PWR_SHUTDOWN_IN_PROGRESS_MS))
            {
                return SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS;
            }
            return normal_stage;
        }
    }

    /*
     * ---- 分支 2：主电丢失防抖确认 ----
     * 更新 prev_main_present 标志
     */
    if (g_main_loss_confirm_count >= main_loss_confirm_target)
    {
        g_prev_main_present = RT_FALSE;
    }

    /*
     * ---- 分支 3：主电丢失的下降沿 ----
     * 刚检测到主电丢失（防抖通过），锁存丢失事件
     */
    if ((main_falling_edge == RT_TRUE) && (g_power_loss_latched == RT_FALSE))
    {
        g_power_loss_latched = RT_TRUE;

        /*
         * 如果超级电容已就绪且有足够电量 →
         * 进入 SUPERCAP_HOLD 阶段，利用超级电容余电完成数据保存
         */
#if APP_PWR_SUPERCAP_MGMT_ENABLE
        if (g_supercap_ready && supercap_available)
        {
            return SVC_POWER_STAGE_SUPERCAP_HOLD;
        }
#endif

        /*
         * 否则直接进入 SHUTDOWN_PENDING
         * 此时只能靠 SOC 自身的掉电保护完成数据保存
         */
        return SVC_POWER_STAGE_SHUTDOWN_PENDING;
    }

    /*
     * ---- 分支 4：主电丢失已锁存 ----
     * 持续处于断电流程中
     */
    if (g_power_loss_latched)
    {
        /*
         * 如果当前已经是 SHUTDOWN_PENDING 或 SHUTDOWN_IN_PROGRESS →
         * 等待超时后进入 SHUTDOWN_IN_PROGRESS
         */
        if ((g_power_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING)
            || (g_power_stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS))
        {
            if (svc_power_get_shutdown_pending_elapsed_ms() >= APP_PWR_SHUTDOWN_IN_PROGRESS_MS)
            {
                return SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS;
            }

            return SVC_POWER_STAGE_SHUTDOWN_PENDING;
        }

        /*
         * 当前在 SUPERCAP_HOLD 阶段 →
         * 如果超级电容电压低于阈值 或 超时 → 进入 SHUTDOWN_PENDING
         */
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

    /*
     * ---- 分支 5：主电确认不在（防抖都没通过） ----
     * 系统应该处于无电状态
     */
    return SVC_POWER_STAGE_MAIN_OFF;
}

/*
 * 处理最终断电时序
 *
 * 当进入 SHUTDOWN_IN_PROGRESS 后，按严格时序执行：
 *   1. 等待 APP_PWR_FINAL_SOC_CUT_DELAY_MS（1s）→ 切断 SOC 供电
 *      （给 SOC 1s 时间完成最后的数据 flush）
 *   2. 等待 APP_PWR_FINAL_HOLD_CUT_DELAY_MS（1.5s）→ 释放 PWR_HOLD
 *      （SOC 完全掉电后再释放自锁，防止重启）
 *
 * 这个时序设计非常关键：
 *   如果先释放 PWR_HOLD 再切 SOC → SOC 可能利用 PWR_HOLD 放电路径残存供电，
 *   导致 SOC 处于"半死不活"的状态，下次上电可能启动异常。
 */
static void svc_power_handle_final_power_cut(void)
{
    rt_uint32_t pending_ms;

    if (g_power_stage != SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS)
    {
        return;
    }

    /* 获取进入 SHUTDOWN_PENDING 后经过了多少毫秒 */
    pending_ms = svc_power_get_shutdown_pending_elapsed_ms();

    /* 第 1 步：延迟 1s 后切断 SOC 电源 */
    if ((g_final_soc_cut_done == RT_FALSE)
        && (pending_ms >= APP_PWR_FINAL_SOC_CUT_DELAY_MS))
    {
        g_final_soc_cut_done = RT_TRUE;
        svc_power_cut_soc_outputs();
#if APP_PWR_SUPERCAP_MGMT_ENABLE
#else
#endif
    }

    /* 第 2 步：再延迟 1.5s 后释放 PWR_HOLD，系统完全断电 */
    if ((g_final_hold_cut_done == RT_FALSE)
        && (pending_ms >= APP_PWR_FINAL_HOLD_CUT_DELAY_MS))
    {
        g_final_hold_cut_done = RT_TRUE;
        rt_kprintf("PWR action: release MCU_PWR_HOLD\r\n");
        svc_power_release_mcu_hold();
    }
}

/*
 * 处理电源阶段切换
 *
 * 当 svc_power_eval_stage() 返回的阶段与当前阶段不同时，执行进入/退出动作。
 * 每个阶段切换时可能需要：
 *   - 记录进入时间（用于超时判断）
 *   - 设置/清除标志位
 *   - 控制外设（如关闭 LCD 背光以省电）
 *   - 打印日志
 */
static void svc_power_handle_stage_transition(svc_power_stage_t new_stage)
{
    if (new_stage == g_power_stage)
    {
        return;  /* 阶段未变化，不做任何操作 */
    }

    if (new_stage == SVC_POWER_STAGE_SUPERCAP_HOLD)
    {
        /* 进入超级电容保持阶段：记录进入时刻，关闭 LCD 背光省电 */
        g_supercap_hold_enter_tick = rt_tick_get();
        g_shutdown_pending_enter_tick = 0;
        g_shutdown_prepare_done = RT_FALSE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;

        /* 关闭 LCD 背光以节省超级电容电量 */
        lcd_backlight_off();
    }
    else if ((new_stage == SVC_POWER_STAGE_SHUTDOWN_PENDING) && (g_shutdown_prepare_done == RT_FALSE))
    {
        /* 第一次进入 SHUTDOWN_PENDING：记录所有计时器，准备通知 SOC 关机 */
        if (g_supercap_hold_enter_tick == 0)
        {
            g_supercap_hold_enter_tick = rt_tick_get();
        }

        g_shutdown_pending_enter_tick = rt_tick_get();
        g_shutdown_prepare_done = RT_TRUE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;

        /* 关闭 LCD 背光，打印关机日志 */
        lcd_backlight_off();
        rt_kprintf("PWR event: enter SHUTDOWN_PENDING, hold=%lums ready=%d\r\n",
                        svc_power_get_hold_elapsed_ms(),
                        g_supercap_ready);
    }
    else if ((new_stage == SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS)
          && (g_power_stage != SVC_POWER_STAGE_SHUTDOWN_IN_PROGRESS))
    {
        /* 开始执行最终断电 */
        rt_kprintf("PWR event: enter SHUTDOWN_IN_PROGRESS, pending=%lums\r\n",
                        svc_power_get_shutdown_pending_elapsed_ms());
    }
    else if ((new_stage == SVC_POWER_STAGE_STANDBY)
          || (new_stage == SVC_POWER_STAGE_ACC_ACTIVE)
          || (new_stage == SVC_POWER_STAGE_ON_ACTIVE)
          || (new_stage == SVC_POWER_STAGE_MAIN_OFF))
    {
        /* 恢复到正常状态：清除所有关机相关的计时器和标志 */
        g_supercap_hold_enter_tick = 0;
        g_shutdown_pending_enter_tick = 0;
        g_shutdown_prepare_done = RT_FALSE;
        g_final_soc_cut_done = RT_FALSE;
        g_final_hold_cut_done = RT_FALSE;
    }

    g_power_stage = new_stage;
}

/*
 * 处理摄像头 12V 供电启用
 *
 * 系统启动后延迟 APP_CAM_12V_ENABLE_DELAY_MS（5s）才开启摄像头供电。
 * 主要原因：SOC 需要先完成自身启动和驱动程序加载，
 * 如果摄像头过早供电，SOC 可能还没有准备好初始化摄像头模组。
 */
static void svc_power_handle_cam_12v_enable(void)
{
    if (g_cam_12v_enabled)
    {
        return;
    }

    if (svc_power_ticks_to_ms(g_cam_12v_enable_start_tick) >= APP_CAM_12V_ENABLE_DELAY_MS)
    {
        gpio_write_pin(SVC_POWER_CTRL_GPIO, SVC_POWER_CTRL_GPIOA_INDEX, SVC_POWER_CTRL_CAM_12V_EN_PIN, 1);
        g_cam_12v_enabled = RT_TRUE;
    }
}


/*
 * 电源管理线程入口
 *
 * 每 APP_POWER_TASK_PERIOD_MS（100ms）执行一次完整的"采样 → 评估 → 执行"循环：
 *   1. 获取 ADC 最新采样数据（24V 电池电压、超级电容电压）
 *   2. 获取车辆 IO 状态（ACC/ON 信号）
 *   3. 评估当前应该处于哪个电源阶段
 *   4. 如果阶段变化，执行进入/退出动作
 *   5. 如果在关机流程中，执行最终断电时序
 *
 * 注意：这里没有使用队列，而是通过"共享状态快照"的方式（svc_adc_get_snapshot）
 * 获取最新 ADC 数据。这是典型的生产者-消费者模式：ADC 线程写入、电源线程读取。
 */
static void svc_power_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        const app_adc_snapshot_t *adc_snapshot;
        const app_vehicle_io_state_t *vehicle_state;
        svc_power_stage_t power_stage;

        /* 第 1 步：获取 ADC 最新采样（由 svc_adc 线程定期更新） */
        adc_snapshot = svc_adc_get_snapshot();

        /* 第 2 步：获取车辆 IO 状态（由 svc_vehicle_io 线程定期更新） */
        vehicle_state = svc_vehicle_io_get_state();

        /* 第 3 步：评估当前电源阶段（核心状态机判断） */
        power_stage = svc_power_eval_stage(adc_snapshot, vehicle_state);

        /* 第 4 步：处理阶段切换（进入/退出动作） */
        svc_power_handle_stage_transition(power_stage);

        /* 第 5 步：如果在关机流程中，执行最终断电时序 */
        svc_power_handle_final_power_cut();

        /* 第 6 步：延迟开启摄像头 12V 供电 */
        svc_power_handle_cam_12v_enable();

        /* 等待下一个周期 */
        rt_thread_mdelay(APP_POWER_TASK_PERIOD_MS);
    }
}

/*
 * 电源服务初始化
 *
 * 初始化控制引脚，将所有全局状态变量清零。
 * 特别注意：必须在 svc_adc_init() 之后调用，因为电源状态机依赖 ADC 数据。
 */
int svc_power_init(void)
{
    /* 初始化电源控制 GPIO 引脚 */
    svc_power_init_ctrl_pins();

    /* 清零所有全局状态变量 */
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

    g_cam_12v_enable_start_tick = rt_tick_get();
    g_cam_12v_enabled = RT_FALSE;


    return RT_EOK;
}

/*
 * 创建并启动电源管理线程
 *
 * 线程配置：
 *   优先级：8（应用层最高优先级，电源变化需要最快响应）
 *   栈大小：2048 字节（状态机局部变量较多）
 *   周期：100ms（平衡响应速度和 CPU 占用）
 */
int svc_power_task_start(void)
{
    rt_thread_t thread;

    /* 创建线程（此时并未开始运行） */
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

    /* 启动线程（加入就绪队列，由调度器决定何时运行） */
    rt_thread_startup(thread);
    return RT_EOK;
}
