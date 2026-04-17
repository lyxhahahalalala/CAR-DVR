#ifndef APPLICATIONS_APP_TYPES_H_
#define APPLICATIONS_APP_TYPES_H_

#include <rtthread.h>

/*
 * ADC 最新采样快照。
 * 当前先服务于调试打印，后续可扩展为共享状态缓存。
 */
typedef struct
{
    rt_uint32_t raw_bat24;
    rt_uint32_t raw_li_bat;
    rt_uint32_t raw_super_c;
    rt_uint32_t raw_key;

    rt_uint32_t est_bat24_mv;
    rt_uint32_t est_li_bat_mv;
    rt_uint32_t est_super_c_mv;
    rt_int16_t mv_bat24;
    rt_int16_t mv_li_bat;
    rt_int16_t mv_super_c;
    rt_int16_t mv_key;
} app_adc_snapshot_t;

/*
 * 车辆输入与备份域相关状态快照。
 * 当前用于给电源服务提供 ACC/ON、锂电状态等基础观测量。
 */
typedef struct
{
    rt_uint8_t wk_acc;
    rt_uint8_t wk_on;

    rt_uint8_t sw_kl1;
    rt_uint8_t sw_kl2;
    rt_uint8_t sw_kl3;
    rt_uint8_t sw_kl4;
    rt_uint8_t sw_kl5;
    rt_uint8_t sw_kl6;
    rt_uint8_t sw_kl7;
    rt_uint8_t sw_kl8;
    rt_uint8_t sw_kl9;
    rt_uint8_t sw_kl10;

    rt_uint8_t li_bat_stdby;
    rt_uint8_t li_bat_chrg;
    rt_uint32_t li_bat_raw;
    rt_uint32_t li_bat_est_mv;
} app_vehicle_io_state_t;


#endif /* APPLICATIONS_APP_TYPES_H_ */
