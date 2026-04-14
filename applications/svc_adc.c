#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/adc.h>

#include "app_config.h"
#include "svc_adc.h"

/* 保存 ADC 最近一次采样结果，便于后续其它模块读取。 */
static app_adc_snapshot_t g_adc_snapshot;

static rt_uint32_t svc_adc_raw_to_pin_mv(rt_uint32_t raw)
{
    rt_uint64_t pin_mv;

    pin_mv = (rt_uint64_t)raw * APP_ADC_VREF_MV;
    pin_mv /= APP_ADC_FULL_SCALE;

    return (rt_uint32_t)pin_mv;
}

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

static void svc_adc_sample_update(rt_adc_device_t adc_dev)
{
    g_adc_snapshot.raw_bat24 = rt_adc_read(adc_dev, APP_ADC_CH_BAT_24V);
    g_adc_snapshot.raw_li_bat = rt_adc_read(adc_dev, APP_ADC_CH_LI_BAT_4V2);
    g_adc_snapshot.raw_super_c = rt_adc_read(adc_dev, APP_ADC_CH_SUPER_C_5V);
    g_adc_snapshot.raw_key = rt_adc_read(adc_dev, APP_ADC_CH_KEY);

    g_adc_snapshot.est_bat24_mv = svc_adc_estimate_input_mv(g_adc_snapshot.raw_bat24,
                                                            APP_ADC_BAT24_DIV_UP_KOHM,
                                                            APP_ADC_BAT24_DIV_DOWN_KOHM);
    g_adc_snapshot.est_li_bat_mv = svc_adc_estimate_input_mv(g_adc_snapshot.raw_li_bat,
                                                             APP_ADC_LI_BAT_DIV_UP_KOHM,
                                                             APP_ADC_LI_BAT_DIV_DOWN_KOHM);
    g_adc_snapshot.est_super_c_mv = svc_adc_raw_to_pin_mv(g_adc_snapshot.raw_super_c);

    /* 保留原有接口字段，后续可继续用于校验 BSP 提供的电压换算能力。 */
    g_adc_snapshot.mv_bat24 = rt_adc_voltage(adc_dev, APP_ADC_CH_BAT_24V);
    g_adc_snapshot.mv_li_bat = rt_adc_voltage(adc_dev, APP_ADC_CH_LI_BAT_4V2);
    g_adc_snapshot.mv_super_c = rt_adc_voltage(adc_dev, APP_ADC_CH_SUPER_C_5V);
    g_adc_snapshot.mv_key = rt_adc_voltage(adc_dev, APP_ADC_CH_KEY);
}

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

static void svc_adc_thread_entry(void *arg)
{
    rt_adc_device_t adc_dev;

    RT_UNUSED(arg);

    rt_thread_mdelay(APP_ADC_STARTUP_DELAY_MS);

    adc_dev = (rt_adc_device_t)rt_device_find(APP_ADC_DEV_NAME);
    if (adc_dev == RT_NULL)
    {
        APP_NON_CAN_LOG("adc0 not found\r\n");
        return;
    }

    if (svc_adc_channel_enable(adc_dev) != RT_EOK)
    {
        return;
    }

    APP_NON_CAN_LOG("adc0 test start\r\n");

    while (1)
    {
        svc_adc_sample_update(adc_dev);

        /* 当前重点观察主电、超级电容和锂电的原始值及估算电压。 */
//        APP_NON_CAN_LOG("ADC: BAT24 raw=%lu est=%lumV SUPER5V raw=%lu est=%lumV LI4V2 raw=%lu est=%lumV KEY=%lu\r\n",
//                        g_adc_snapshot.raw_bat24,
//                        g_adc_snapshot.est_bat24_mv,
//                        g_adc_snapshot.raw_super_c,
//                        g_adc_snapshot.est_super_c_mv,
//                        g_adc_snapshot.raw_li_bat,
//                        g_adc_snapshot.est_li_bat_mv,
//                        g_adc_snapshot.raw_key);

        rt_thread_mdelay(APP_ADC_SAMPLE_PERIOD_MS);
    }
}

int svc_adc_init(void)
{
    /* 当前阶段没有硬件预初始化动作，保留统一服务初始化入口。 */
    rt_memset(&g_adc_snapshot, 0, sizeof(g_adc_snapshot));
    return RT_EOK;
}

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

const app_adc_snapshot_t *svc_adc_get_snapshot(void)
{
    return &g_adc_snapshot;
}
