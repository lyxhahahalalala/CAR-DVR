/*
 * ============================================================
 *  svc_vehicle_io.c — 车辆 IO 输入采集服务
 * ============================================================
 *
 * 功能概述:
 *   本模块负责采集车辆的所有开关量和模拟量输入信号。
 *   它是硬件引脚与软件逻辑之间的桥梁——将 GPIO 电平翻译成
 *   有意义的车辆状态(ACC/ON/KL1~KL10/锂电池状态等)。
 *
 * 为什么需要这个模块?
 *   如果直接在 svc_power 或 app_can 中读 GPIO, 会导致:
 *   1) 耦合: 电源管理和 CAN 都要知道引脚定义
 *   2) 重复: 多处读同一个 GPIO 浪费 CPU
 *   3) 不一致: 不同模块可能读到不同时刻的值
 *   本模块将所有输入集中采集, 统一提供状态快照
 *
 * 硬件连接:
 *   PC19 -> WK_ACC     (ACC 唤醒信号, 低有效)
 *   PC20 -> WK_ON      (ON 唤醒信号,  低有效)
 *   PB22 -> LI_BAT_STDBY (锂电池待机标志)
 *   PB23 -> LI_BAT_CHRG  (锂电池充电标志)
 *   PB0~PB18 -> SW_KL1~KL10 (车辆信号输入)
 *
 * 数据流向:
 *   GPIO 电平 → svc_vehicle_io_update_state() → g_vehicle_io_state
 *                                                    ↓
 *   svc_vehicle_io_get_state() 提供给其他模块读取
 *      ├── svc_power.c:  读取 WK_ACC/WK_ON 判断车辆电门状态
 *      └── app_can.c:    读取 SW_KL 打包成 CAN 报文发送
 */
#include "board.h"
#include "hpm_gpio_drv.h"
#include "app_config.h"
#include "svc_adc.h"
#include "svc_vehicle_io.h"

/*
 * ============================================================
 *  GPIO 引脚宏定义
 * ============================================================
 *
 * 为什么使用宏而不是变量?
 *   1) 引脚分配是硬件设计定死的, 运行时不会改变
 *   2) 宏在编译时展开, 没有运行时开销
 *   3) 如果要改硬件, 只需要改这里一处
 */

/* ---- ACC/ON 唤醒信号 ---- */
/*
 * WK_ACC 和 WK_ON:
 *   这两个信号直接来自车辆的 ACC 和 ON 档电门。
 *   低电平表示"有电" (因为外部有上拉, 导通时拉到 GND)。
 *   svc_power 根据这两个信号做电源状态迁移。
 */
#define WK_ACC_GPIO_CTRL          HPM_GPIO0
#define WK_ACC_GPIO_INDEX         GPIO_DI_GPIOC
#define WK_ACC_PIN                19

#define WK_ON_GPIO_CTRL           HPM_GPIO0
#define WK_ON_GPIO_INDEX          GPIO_DI_GPIOC
#define WK_ON_PIN                 20

/* ---- 锂电池状态 ---- */
#define LI_BAT_STDBY_GPIO_CTRL    HPM_GPIO0
#define LI_BAT_STDBY_GPIO_INDEX   GPIO_DI_GPIOB
#define LI_BAT_STDBY_PIN          22

#define LI_BAT_CHRG_GPIO_CTRL     HPM_GPIO0
#define LI_BAT_CHRG_GPIO_INDEX    GPIO_DI_GPIOB
#define LI_BAT_CHRG_PIN           23

/* ---- 车辆信号输入 KL1 ~ KL10 ---- */
/*
 * KL1~KL10 (Klemme, 德语"端子"):
 *   这些是商用车标准的信号输入, 例如:
 *   KL1 = 地线(搭铁), KL15 = 点火开关, KL30 = 常电
 *   此处 KL1~KL10 是自定义的车辆状态输入
 *
 * 电平特性 (见下方 SW_KLx_ACTIVE_LOW 配置):
 *   KL1~KL8:  低电平有效 (外部上拉, 开关导通→GND)
 *   KL9~KL10: 高电平有效 (外部下拉, 开关导通→VCC)
 */
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


/* 全局车辆 IO 状态结构体, 其他模块通过 svc_vehicle_io_get_state() 访问 */
static app_vehicle_io_state_t g_vehicle_io_state;

/* 调试开关: 1=使能 KL 状态变化打印, 0=关闭 */
#define SW_KL_DEBUG_ENABLE        1

/*
 * 上次打印的 KL 位域, 用于检测变化:
 *   初始值 0xFFFF 保证第一次一定打印(因为和任何真实值都不等)
 */
static rt_uint16_t g_sw_kl_last_bits = 0xFFFFU;

/*
 * ============================================================
 *  电平极性转换函数
 * ============================================================
 *
 * 为什么需要这个函数?
 *   硬件设计上, 有些信号是低电平有效(开关导通→GND),
 *   有些是高电平有效(开关导通→VCC)。
 *   软件需要统一成"1=有效, 0=无效"的逻辑值。
 *
 * @param raw_level GPIO 读到的原始电平 (0 或 1)
 * @param active_low 1=低电平有效, 0=高电平有效
 * @return 1=信号有效, 0=信号无效
 */
static rt_uint8_t svc_vehicle_io_apply_level(rt_uint32_t raw_level, rt_uint8_t active_low)
{
    if (active_low != 0U) {
        return (raw_level == 0U) ? 1U : 0U;
    }

    return (raw_level != 0U) ? 1U : 0U;
}

/*
 * KL1~KL8:  低电平有效 (active_low = 1)
 *           外部电路: GPIO 有上拉电阻, 开关导通拉到 GND
 *           所以 GPIO=0 → 开关接通 → 信号有效
 *
 * KL9~KL10: 高电平有效 (active_low = 0)
 *           外部电路: GPIO 有下拉电阻, 开关导通拉到 VCC
 *           所以 GPIO=1 → 开关接通 → 信号有效
 *
 * 为什么 KL1~KL8 和 KL9~KL10 极性不同?
 *   这是车辆电气系统设计决定的, 不同 KL 信号来源不同,
 *   有些是"搭铁控制"(低有效), 有些是"电源控制"(高有效)
 */
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



/*
 * ============================================================
 *  svc_vehicle_io_update_state — 更新所有车辆输入状态
 * ============================================================
 *
 * 这个函数是模块的核心, 每次调用都会:
 *   1) 读取所有 GPIO 引脚电平
 *   2) 做电平极性转换
 *   3) 从 ADC 快照读取锂电池电压
 *   4) 保存到全局状态结构体
 *
 * 为什么不在这里做软件消抖?
 *   - GPIO 输入本身有硬件 RC 滤波(在 board 原理图上)
 *   - WK_ACC/WK_ON 的消抖在 svc_power 中做的(那里有完整的
 *     状态机和确认计数器, 更适合做消抖)
 *   - 本模块只负责"当前时刻的原始值", 不负责判断
 */
static void svc_vehicle_io_update_state(void)
{
    const app_adc_snapshot_t *adc_snapshot;
    rt_uint32_t wk_acc_raw;
    rt_uint32_t wk_on_raw;


    /* ====== 1. 读取 WK_ACC / WK_ON ====== */
    /*
     * WK_ACC 和 WK_ON 当前板级需求为低有效。
     * 即 GPIO 读到 0 表示"有 ACC/ON 信号"。
     *
     * 为什么这里直接内联而不调用 svc_vehicle_io_apply_level?
     *   因为在可预见的将来 WK_ACC/WK_ON 一直是低有效,
     *   直接写可以少一次函数调用开销 (虽然很小, 但 IO 采集
     *   是 50ms 周期执行的, 累积效应可观)
     */
    wk_acc_raw = gpio_read_pin(WK_ACC_GPIO_CTRL, WK_ACC_GPIO_INDEX, WK_ACC_PIN);
    wk_on_raw  = gpio_read_pin(WK_ON_GPIO_CTRL,  WK_ON_GPIO_INDEX,  WK_ON_PIN);

    g_vehicle_io_state.wk_acc = (wk_acc_raw == 0U) ? 1U : 0U;
    g_vehicle_io_state.wk_on  = (wk_on_raw  == 0U) ? 1U : 0U;



    /* ====== 2. 读取 KL1 ~ KL10 信号 ====== */
    /*
     * 每个 KL 信号都通过 svc_vehicle_io_apply_level() 转换电平极性。
     * 这样硬件改线(例如 KL3 从低有效改为高有效)只需要改宏,
     * 不需要改这段代码。
     *
     * 为什么没用循环?
     *   虽然可以用 GPIO 端口寄存器批量读取, 但每个 KL 可能分布
     *   在不同的 GPIO 组(虽然这里都在 GPIOB), 且 HPM SDK 的
     *   gpio_read_pin 已经封装好。用逐行读取更清晰。
     *
     * 隐含的陷阱:
     *   如果未来某个 KL 被重新分配到不同的 GPIO 端口,
     *   需要同步修改上面的 SW_KLx_GPIO_CTRL/INDEX/PIN 宏
     */
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



    /* ====== 3. 读取锂电池状态 ====== */
    /*
     * LI_BAT_STDBY 和 LI_BAT_CHRG 是纯电平信号:
     *   1 = 电池管理 IC 发出待机/充电状态指示
     *
     * 注意: 这两个信号的电平逻辑由电池管理 IC 决定,
     * 不经过 svc_vehicle_io_apply_level() 处理,
     * 因为它们的"有效"含义已经由硬件定义了
     */
    g_vehicle_io_state.li_bat_stdby = gpio_read_pin(LI_BAT_STDBY_GPIO_CTRL,
                                                    LI_BAT_STDBY_GPIO_INDEX,
                                                    LI_BAT_STDBY_PIN);
    g_vehicle_io_state.li_bat_chrg = gpio_read_pin(LI_BAT_CHRG_GPIO_CTRL,
                                                   LI_BAT_CHRG_GPIO_INDEX,
                                                   LI_BAT_CHRG_PIN);

    /*
     * 锂电池电压从 ADC 快照获取, 而不是自己读 ADC:
     *   svc_adc 已经以 10ms 周期采集所有通道并做电压换算,
     *   这里直接取换算结果, 避免重复计算
     *
     *   raw      = ADC 原始值 (0~65535)
     *   est_mv   = 估计的电池电压 (mV)
     *              通过分压比反推: Vin = ADC_pin_mV × (R1+R2)/R2
     */
    adc_snapshot = svc_adc_get_snapshot();
    g_vehicle_io_state.li_bat_raw = adc_snapshot->raw_li_bat;
    g_vehicle_io_state.li_bat_est_mv = adc_snapshot->est_li_bat_mv;
}


/*
 * ============================================================
 *  svc_vehicle_io_pack_sw_kl_bits — 将 KL 状态打包成位域
 * ============================================================
 *
 * 为什么需要打包?
 *   CAN 总线传输效率要求: 把 10 个布尔值打包成 1 个 uint16_t,
 *   这样发送一条 CAN 报文(8 字节数据域)就可以带上所有 KL 状态,
 *   而不需要每个 KL 占一个字节。
 *
 * 位分配:
 *   bit 0  = KL1, bit 1 = KL2, ..., bit 9 = KL10
 *   bit 10~15 = 保留 (未来扩展)
 */
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

/*
 * ============================================================
 *  svc_vehicle_io_debug_print_sw_kl — KL 状态变化调试打印
 * ============================================================
 *
 * 只在 KL 状态变化时打印, 避免串口淹没在重复数据中:
 *   - 保存上一次的 KL 位域 (g_sw_kl_last_bits)
 *   - 每次更新后比较, 不同才打印
 *   - 初始值 0xFFFF 确保开机第一次一定会打印
 */
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



/*
 * ============================================================
 *  svc_vehicle_io_thread_entry — IO 采集线程入口
 * ============================================================
 *
 * 执行周期: APP_IO_TASK_PERIOD_MS (通常在 50ms 左右)
 *
 * 为什么是 50ms?
 *   车辆信号的变化速度远低于 50ms (机械开关的抖动在 ~5ms),
 *   更快的采样没有意义, 反而浪费 CPU。
 *   慢速采样 + 软件消抖(在 svc_power) 是经典的嵌入式方案。
 *
 * 线程优先级建议: 中等 (15 左右)
 *   比 CAN 和电源管理低, 比 LCD 刷新高
 */
static void svc_vehicle_io_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        /* 周期更新车辆输入状态, 供电源管理和后续业务读取。 */
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

/*
 * ============================================================
 *  模块初始化
 * ============================================================
 */
int svc_vehicle_io_init(void)
{
    /*
     * 初始化 GPIO 引脚:
     *   board_init_io_pins()     — 配置 WK_ACC/WK_ON 和锂电池引脚
     *   init_input_switch_pins() — 配置 KL1~KL10 引脚
     *
     * 为什么分成两个函数?
     *   board_init_io_pins() 是 board 层的通用 IO 初始化
     *   init_input_switch_pins() 是这个项目的特定开关引脚初始化
     *   它们可能分别由不同的硬件工程师维护
     *
     * 为什么不在这里检查返回值?
     *   这些函数都是 void 类型(见 board 层), 没有返回值。
     *   如果 GPIO 初始化失败, 后续读引脚会读到不确定的值,
     *   这种情况应该由硬件工程师保证, 软件不处理。
     */
    board_init_io_pins();
    init_input_switch_pins();

    /* 清零软件状态, 避免上电时读到随机值 */
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

/*
 * ============================================================
 *  svc_vehicle_io_get_state — 获取车辆状态快照
 * ============================================================
 *
 * 返回指针而不是拷贝结构体的原因:
 *   1) 效率: 避免 10+ 字节的结构体拷贝
 *   2) 调用者通常只需要读 1~2 个字段(如 WK_ACC),
 *      拷贝整个结构体浪费 CPU
 *
 * 线程安全性:
 *   调用者应该在短时间内读取完需要的数据,
 *   因为 g_vehicle_io_state 每 50ms 会被 IO 线程更新一次。
 *   如果需要一致的"瞬时快照"(所有字段来自同一次采集),
 *   外部应该加锁或关闭调度。
 *
 *   在这个项目中, 读取方(svc_power, app_can)总是在各自的
 *   线程循环中读取, 少量不一致是可以接受的。
 */
const app_vehicle_io_state_t *svc_vehicle_io_get_state(void)
{
    return &g_vehicle_io_state;
}
