#include <rtdevice.h>
#include <drivers/adc.h>

#include "app_config.h"
#include "svc_adc.h"

static app_adc_snapshot_t g_adc_snapshot;

typedef enum
{
    ADC_KEY_NONE = 0,
    ADC_KEY_S1,
    ADC_KEY_S2,
    ADC_KEY_S3,
    ADC_KEY_S4,
} adc_key_t;

static adc_key_t g_adc_key_last = ADC_KEY_NONE;
static adc_key_t g_adc_key_candidate = ADC_KEY_NONE;
static uint8_t g_adc_key_confirm = 0;
static rt_bool_t g_adc_s1_event = RT_FALSE;
static rt_bool_t g_adc_s2_event = RT_FALSE;
static rt_bool_t g_adc_s3_event = RT_FALSE;
static rt_bool_t g_adc_s4_event = RT_FALSE;


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

static adc_key_t svc_adc_decode_key(void)
{
    rt_uint32_t raw = g_adc_snapshot.raw_key;

    /* S1 */
    if (raw <= 200U) {
        return ADC_KEY_S1;
    }

    /* S2 */
    if ((raw >= 17000U) && (raw <= 20000U)) {
        return ADC_KEY_S2;
    }

    /* S3 */
    if ((raw >= 28000U) && (raw <= 31000U)) {
        return ADC_KEY_S3;
    }

    /* S4 */
    if ((raw >= 46000U) && (raw <= 50000U)) {
        return ADC_KEY_S4;
    }

    return ADC_KEY_NONE;
}



static void svc_adc_update_key_events(void)
{
    adc_key_t key_now = svc_adc_decode_key();

//    APP_NON_CAN_LOG("KEY raw=%lu mv=%d key=%d\r\n",


//                    g_adc_snapshot.raw_key,
//                    g_adc_snapshot.mv_key,
//                    key_now);


    if (key_now == g_adc_key_candidate) {
        if (g_adc_key_confirm < 2U) {
            g_adc_key_confirm++;
        }
    } else {
        g_adc_key_candidate = key_now;
        g_adc_key_confirm = 0U;
        return;
    }

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

    g_adc_snapshot.mv_bat24 = rt_adc_voltage(adc_dev, APP_ADC_CH_BAT_24V);
    g_adc_snapshot.mv_li_bat = rt_adc_voltage(adc_dev, APP_ADC_CH_LI_BAT_4V2);
    g_adc_snapshot.mv_super_c = rt_adc_voltage(adc_dev, APP_ADC_CH_SUPER_C_5V);
    g_adc_snapshot.mv_key = rt_adc_voltage(adc_dev, APP_ADC_CH_KEY);

    svc_adc_update_key_events();
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
        rt_thread_mdelay(APP_ADC_SAMPLE_PERIOD_MS);
    }
}

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

rt_bool_t svc_adc_is_any_key_pressed(void)
{
    return (g_adc_key_last != ADC_KEY_NONE) ? RT_TRUE : RT_FALSE;
}


rt_bool_t svc_adc_consume_s1_event(void)
{
    rt_bool_t event = g_adc_s1_event;
    g_adc_s1_event = RT_FALSE;
    return event;
}

rt_bool_t svc_adc_consume_s2_event(void)
{
    rt_bool_t event = g_adc_s2_event;
    g_adc_s2_event = RT_FALSE;
    return event;
}

rt_bool_t svc_adc_consume_s3_event(void)
{
    rt_bool_t event = g_adc_s3_event;
    g_adc_s3_event = RT_FALSE;
    return event;
}


rt_bool_t svc_adc_consume_s4_event(void)
{
    rt_bool_t event = g_adc_s4_event;
    g_adc_s4_event = RT_FALSE;
    return event;
}
