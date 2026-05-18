/*
 * svc_adc.c — ADC 采样服务
 *
 * 负责 4 通道 ADC 采样和按键解码。
 *
 * ADC 通道映射：
 *   通道 2  → 24V 主电池（经 1000K:91K 分压）
 *   通道 3  → 锂电池 4.2V（经 91K:91K 分压）
 *   通道 4  → 超级电容（直连，无分压）
 *   通道 11 → 按键输入（分压网络）
 *
 * 数据流向：
 *   ADC 线程（10ms 周期）→ 采样更新 g_adc_snapshot → 其他服务通过 svc_adc_get_snapshot() 读取
 *
 * 按键解码机制：
 *   1. 通过 ADC 电压判断哪个按键被按下（分压网络，不同按键产生不同电压）
 *   2. 防抖确认（连续 2 次采样一致才确认为有效按键）
 *   3. 一次性事件消费（调用 consume_*_event 后清除标志）
 */
#include <rtdevice.h>
#include <drivers/adc.h>

#include "app_config.h"
#include "svc_adc.h"

/* 全局 ADC 采样快照实例（其他服务通过指针读取，不需要拷贝） */
static app_adc_snapshot_t g_adc_snapshot;

/* 按键解码枚举 */
typedef enum
{
    ADC_KEY_NONE = 0,  /* 无按键 */
    ADC_KEY_S1,        /* 按键 S1 */
    ADC_KEY_S2,        /* 按键 S2 */
    ADC_KEY_S3,        /* 按键 S3 */
    ADC_KEY_S4,        /* 按键 S4 */
} adc_key_t;

/* 按键防抖状态 */
static adc_key_t g_adc_key_last = ADC_KEY_NONE;        /* 上次确认的按键 */
static adc_key_t g_adc_key_candidate = ADC_KEY_NONE;    /* 当前候选按键 */
static uint8_t g_adc_key_confirm = 0;                   /* 确认计数（连续采样一致才确认） */

/* 按键事件标志（一次性消费，消费后置 RT_FALSE） */
static rt_bool_t g_adc_s1_event = RT_FALSE;
static rt_bool_t g_adc_s2_event = RT_FALSE;
static rt_bool_t g_adc_s3_event = RT_FALSE;
static rt_bool_t g_adc_s4_event = RT_FALSE;


/*
 * ADC 原始值 → ADC 引脚电压（mV）
 *
 * 公式：Vin = raw × Vref / 65535
 * 其中 raw = ADC 原始值（0~65535），Vref = 3.3V = 3300mV
 */
static rt_uint32_t svc_adc_raw_to_pin_mv(rt_uint32_t raw)
{
    rt_uint64_t pin_mv;

    pin_mv = (rt_uint64_t)raw * APP_ADC_VREF_MV;
    pin_mv /= APP_ADC_FULL_SCALE;

    return (rt_uint32_t)pin_mv;
}

/*
 * 估算实际输入电压（考虑分压电阻比）
 *
 * 公式：Vinput = Vpin × (Rup + Rdown) / Rdown
 *
 * 例如 24V 电池通道：Rup=1000KΩ, Rdown=91KΩ
 *   如果 ADC 引脚读到 300mV = 0.3V（经过分压后的值）
 *   则实际电压 = 0.3 × (1000+91)/91 = 0.3 × 11.99 ≈ 3.6V → 但其实应该是 24V
 *
 * 这个函数用于估算是准确的，RT-Thread 的 rt_adc_voltage() 也提供了类似功能。
 */
static rt_uint32_t svc_adc_estimate_input_mv(rt_uint32_t raw,
                                             rt_uint32_t divider_up_kohm,
                                             rt_uint32_t divider_down_kohm)
{
    rt_uint64_t input_mv;
    rt_uint32_t pin_mv;

    pin_mv = svc_adc_raw_to_pin_mv(raw);
    input_mv = (rt_uint64_t)pin_mv * (divider_up_kohm + divider_down_kohm);
    input_mv /= divider_down_kohm;

    return (rt_uint32_t)input_mv;
}

/*
 * 按键解码
 *
 * 通过 ADC 原始电压值判断哪个按键被按下。
 * 4 个按键（S1~S4）通过不同的分压电阻接到同一个 ADC 通道，
 * 不同按键产生不同的电压，用电压阈值区分。
 *
 * 注意：这些阈值（200/17000-20000/28000-31000/46000-50000）
 * 是 16 位 ADC 原始值，不是毫伏电压。
 * 如果更换了分压电阻或 VREF 变化，需要重新标定这些阈值。
 */
static adc_key_t svc_adc_decode_key(void)
{
    rt_uint32_t raw = g_adc_snapshot.raw_key;

    /* S1：ADC 值 ≤ 200（电压接近 0，电阻最小） */
    if (raw <= 200U) {
        return ADC_KEY_S1;
    }

    /* S2：ADC 值 17000~20000 */
    if ((raw >= 17000U) && (raw <= 20000U)) {
        return ADC_KEY_S2;
    }

    /* S3：ADC 值 28000~31000 */
    if ((raw >= 28000U) && (raw <= 31000U)) {
        return ADC_KEY_S3;
    }

    /* S4：ADC 值 46000~50000 */
    if ((raw >= 46000U) && (raw <= 50000U)) {
        return ADC_KEY_S4;
    }

    /* 没有匹配到任何按键 */
    return ADC_KEY_NONE;
}


/*
 * 更新按键事件
 *
 * 实现"防抖 + 边缘检测"的按键逻辑：
 *   1. g_adc_key_candidate 记录当前采样到的按键
 *   2. 如果连续 2 次采样一致 → 确认为有效按键
 *   3. 如果确认的按键和上次不同（边缘）→ 产生事件
 *
 * confirm>=1（而不是 confirm>=2）就触发事件，意味着防抖只有 1 次确认，
 * 这个防抖深度相对较浅（相当于 10ms 防抖，因为采样周期是 10ms）。
 */
static void svc_adc_update_key_events(void)
{
    adc_key_t key_now = svc_adc_decode_key();

//    APP_NON_CAN_LOG("KEY raw=%lu mv=%d key=%d\r\n",
//                    g_adc_snapshot.raw_key,
//                    g_adc_snapshot.mv_key,
//                    key_now);


    if (key_now == g_adc_key_candidate) {
        /* 连续采样一致 → 确认计数增加 */
        if (g_adc_key_confirm < 2U) {
            g_adc_key_confirm++;
        }
    } else {
        /* 采样不一致 → 切换候选按键，清零计数 */
        g_adc_key_candidate = key_now;
        g_adc_key_confirm = 0U;
        return;
    }

    /*
     * 确认次数 ≥1 且和上次按键不同 → 产生事件
     * 注意：这里用 >=1 而不是 >=2，防抖深度较浅，
     * 实际效果：连续 2 次采样相同即确认（第 1 次设置 candidate，第 2 次触发生成事件）
     */
    if ((g_adc_key_confirm >= 1U) && (key_now != g_adc_key_last)) {
        g_adc_key_last = key_now;

        if (key_now == ADC_KEY_S1) {
            g_adc_s1_event = RT_TRUE;
        } else if (key_now == ADC_KEY_S2) {
            g_adc_s2_event = RT_TRUE;
        } else if (key_now == ADC_KEY_S3) {
            g_adc_s3_event = RT_TRUE;
        } else if (key_now == ADC_KEY_S4) {
            g_adc_s4_event = RT_TRUE;
        }
    }
}

/*
 * ADC 采样更新：读取所有通道并填充到快照结构体
 *
 * 每次采样更新以下数据：
 *   - raw_*：ADC 原始值（0~65535）
 *   - est_*_mv：分压折算后的估算输入电压
 *   - mv_*：使用 RT-Thread API 计算的电压
 */
static void svc_adc_sample_update(rt_adc_device_t adc_dev)
{
    /* 读取 4 个通道的 ADC 原始值 */
    g_adc_snapshot.raw_bat24 = rt_adc_read(adc_dev, APP_ADC_CH_BAT_24V);
    g_adc_snapshot.raw_li_bat = rt_adc_read(adc_dev, APP_ADC_CH_LI_BAT_4V2);
    g_adc_snapshot.raw_super_c = rt_adc_read(adc_dev, APP_ADC_CH_SUPER_C_5V);
    g_adc_snapshot.raw_key = rt_adc_read(adc_dev, APP_ADC_CH_KEY);

    /* 估算实际输入电压（考虑分压比） */
    g_adc_snapshot.est_bat24_mv = svc_adc_estimate_input_mv(g_adc_snapshot.raw_bat24,
                                                            APP_ADC_BAT24_DIV_UP_KOHM,
                                                            APP_ADC_BAT24_DIV_DOWN_KOHM);
    g_adc_snapshot.est_li_bat_mv = svc_adc_estimate_input_mv(g_adc_snapshot.raw_li_bat,
                                                             APP_ADC_LI_BAT_DIV_UP_KOHM,
                                                             APP_ADC_LI_BAT_DIV_DOWN_KOHM);
    /* 超级电容直连，无分压 */
    g_adc_snapshot.est_super_c_mv = svc_adc_raw_to_pin_mv(g_adc_snapshot.raw_super_c);

    /* 使用 RT-Thread API 获取电压（作为参考/冗余计算） */
    g_adc_snapshot.mv_bat24 = rt_adc_voltage(adc_dev, APP_ADC_CH_BAT_24V);
    g_adc_snapshot.mv_li_bat = rt_adc_voltage(adc_dev, APP_ADC_CH_LI_BAT_4V2);
    g_adc_snapshot.mv_super_c = rt_adc_voltage(adc_dev, APP_ADC_CH_SUPER_C_5V);
    g_adc_snapshot.mv_key = rt_adc_voltage(adc_dev, APP_ADC_CH_KEY);

    /* 按键事件更新（解码 + 防抖） */
    svc_adc_update_key_events();
}

/*
 * 启用所有 ADC 通道（逐个检查返回值）
 *
 * @return RT_EOK 全部成功，-RT_ERROR 任一通道失败
 */
static int svc_adc_channel_enable(rt_adc_device_t adc_dev)
{
    if (rt_adc_enable(adc_dev, APP_ADC_CH_BAT_24V) != RT_EOK)
    {
        APP_NON_CAN_LOG("enable ADC_CH_BAT_24V failed\r\n");
        return -RT_ERROR;
    }

    if (rt_adc_enable(adc_dev, APP_ADC_CH_LI_BAT_4V2) != RT_EOK)
    {
        APP_NON_CAN_LOG("enable ADC_CH_LI_BAT_4V2 failed\r\n");
        return -RT_ERROR;
    }

    if (rt_adc_enable(adc_dev, APP_ADC_CH_SUPER_C_5V) != RT_EOK)
    {
        APP_NON_CAN_LOG("enable ADC_CH_SUPER_C_5V failed\r\n");
        return -RT_ERROR;
    }

    if (rt_adc_enable(adc_dev, APP_ADC_CH_KEY) != RT_EOK)
    {
        APP_NON_CAN_LOG("enable ADC_CH_KEY failed\r\n");
        return -RT_ERROR;
    }

    return RT_EOK;
}

/*
 * ADC 采样线程入口
 *
 * 启动流程：
 *   1. 延迟 APP_ADC_STARTUP_DELAY_MS（500ms）等待系统稳定
 *   2. 查找 ADC 设备（"adc0"）
 *   3. 启用所有通道
 *   4. 以 APP_ADC_SAMPLE_PERIOD_MS（10ms）周期循环采样
 *
 * 10ms 的采样频率对于按键检测（人机交互 50ms 级别响应即可）和
 * 电压监控（电源状态机 100ms 周期）来说都足够快。
 */
static void svc_adc_thread_entry(void *arg)
{
    rt_adc_device_t adc_dev;

    RT_UNUSED(arg);

    /* 启动延时，等待电源稳定后再开始采样 */
    rt_thread_mdelay(APP_ADC_STARTUP_DELAY_MS);

    /* 查找 ADC 设备 */
    adc_dev = (rt_adc_device_t)rt_device_find(APP_ADC_DEV_NAME);
    if (adc_dev == RT_NULL)
    {
        APP_NON_CAN_LOG("adc0 not found\r\n");
        return;
    }

    /* 启用所有通道 */
    if (svc_adc_channel_enable(adc_dev) != RT_EOK)
    {
        return;
    }

    APP_NON_CAN_LOG("adc0 test start\r\n");

    /* 主循环：采样 → 等待 */
    while (1)
    {
        svc_adc_sample_update(adc_dev);
        rt_thread_mdelay(APP_ADC_SAMPLE_PERIOD_MS);
    }
}

/*
 * ADC 服务初始化
 *
 * 清零采样快照和按键状态变量。
 */
int svc_adc_init(void)
{
    rt_memset(&g_adc_snapshot, 0, sizeof(g_adc_snapshot));
    g_adc_key_last = ADC_KEY_NONE;
    g_adc_key_candidate = ADC_KEY_NONE;
    g_adc_key_confirm = 0U;
    g_adc_s1_event = RT_FALSE;
    g_adc_s2_event = RT_FALSE;
    g_adc_s3_event = RT_FALSE;
    g_adc_s4_event = RT_FALSE;
    return RT_EOK;
}

/*
 * 创建并启动 ADC 采样线程
 *
 * 优先级 11，栈 2048 字节，周期 10ms。
 * ADC 优先级高于 LCD 但低于电源管理，因为电源需要及时获取电压数据。
 */
int svc_adc_task_start(void)
{
    rt_thread_t adc_thread;

    adc_thread = rt_thread_create(APP_ADC_TASK_NAME,
                                  svc_adc_thread_entry,
                                  RT_NULL,
                                  APP_ADC_TASK_STACK_SIZE,
                                  APP_ADC_TASK_PRIORITY,
                                  APP_ADC_TASK_TICK);
    if (adc_thread == RT_NULL)
    {
        APP_NON_CAN_LOG("adc thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(adc_thread);
    return RT_EOK;
}

/* ---- 对外 API ---- */

/** 获取最新 ADC 采样快照（指针） */
const app_adc_snapshot_t *svc_adc_get_snapshot(void)
{
    return &g_adc_snapshot;
}

/** 是否有任意按键被按下（不消费事件） */
rt_bool_t svc_adc_is_any_key_pressed(void)
{
    return (g_adc_key_last != ADC_KEY_NONE) ? RT_TRUE : RT_FALSE;
}


/** 消费 S1 按键事件（一次性，消费后清除标志） */
rt_bool_t svc_adc_consume_s1_event(void)
{
    rt_bool_t event = g_adc_s1_event;
    g_adc_s1_event = RT_FALSE;
    return event;
}

/** 消费 S2 按键事件 */
rt_bool_t svc_adc_consume_s2_event(void)
{
    rt_bool_t event = g_adc_s2_event;
    g_adc_s2_event = RT_FALSE;
    return event;
}

/** 消费 S3 按键事件 */
rt_bool_t svc_adc_consume_s3_event(void)
{
    rt_bool_t event = g_adc_s3_event;
    g_adc_s3_event = RT_FALSE;
    return event;
}


/** 消费 S4 按键事件 */
rt_bool_t svc_adc_consume_s4_event(void)
{
    rt_bool_t event = g_adc_s4_event;
    g_adc_s4_event = RT_FALSE;
    return event;
}
