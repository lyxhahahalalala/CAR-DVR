/*
 * app_types.h — 应用层共享数据类型
 *
 * 这个文件定义多个服务模块之间传递的数据结构。
 * 每个结构的注释说明了"谁写入、谁读取"，方便理解数据流向。
 */
#ifndef APPLICATIONS_APP_TYPES_H_
#define APPLICATIONS_APP_TYPES_H_

#include <rtthread.h>

/*
 * ADC 最新采样快照
 *
 * 数据流向：svc_adc 写入 → svc_power / svc_lcd 读取
 *
 * raw_* = ADC 原始值（0~65535，16 位 ADC 满量程）
 * est_*_mv = 估算电压（未经分压折算的 ADC 引脚电压）
 * mv_* = 实际电压（已考虑分压电阻比，单位 mV）
 *
 * 注意：目前 raw 和 est 同时存在是过渡状态，后续可简化。
 */
typedef struct
{
    /* ---- ADC 原始采样值（由 svc_adc.c 的 ADC 中断/轮询填充） ---- */
    rt_uint32_t raw_bat24;      /* 通道 2：24V 主电池（经过 1000K:91K 分压） */
    rt_uint32_t raw_li_bat;     /* 通道 3：锂电池 4.2V（经过 91K:91K 分压） */
    rt_uint32_t raw_super_c;    /* 通道 4：超级电容（直连，无分压） */
    rt_uint32_t raw_key;        /* 通道 11：按键 ADC（分压网络识别不同按键） */

    /* ---- 估算电压（ADC 引脚电压 = raw * VREF / 65535） ---- */
    rt_uint32_t est_bat24_mv;   /* ADC 引脚处的 24V 电池采样电压 */
    rt_uint32_t est_li_bat_mv;  /* ADC 引脚处的锂电池采样电压 */
    rt_uint32_t est_super_c_mv; /* ADC 引脚处的超级电容采样电压 */

    /* ---- 实际电压（已折算分压比） ---- */
    rt_int16_t mv_bat24;        /* 实际 24V 电池电压 = est_bat24_mv × (1000+91)/91 */
    rt_int16_t mv_li_bat;       /* 实际锂电池电压 = est_li_bat_mv × (91+91)/91 */
    rt_int16_t mv_super_c;      /* 实际超级电容电压 = est_super_c_mv（直连无分压） */
    rt_int16_t mv_key;          /* 按键 ADC 电压，用于判断哪个按键被按下 */
} app_adc_snapshot_t;

/*
 * 车辆输入与备份域状态快照
 *
 * 数据流向：svc_vehicle_io 写入 → svc_power / svc_lcd / app_usart_cmd 读取
 *
 * wk_acc / wk_on = 车辆点火开关信号
 * sw_kl1~sw_kl10 = 10 路开关量输入（KL1~KL8 低有效，KL9~KL10 高有效）
 * li_bat_* = 锂电池状态（待机/充电/电压）
 */
typedef struct
{
    /* ---- 点火开关信号（由 svc_vehicle_io 从 PC19/PC20 读取） ---- */
    rt_uint8_t wk_acc;          /* ACC 档位（1=ACC 接通，车辆附件供电） */
    rt_uint8_t wk_on;           /* ON 档位（1=ON 接通，车辆全车供电） */

    /* ---- 10 路开关量输入 ---- */
    rt_uint8_t sw_kl1;          /* 小灯（KL1，低有效） */
    rt_uint8_t sw_kl2;          /* 制动（KL2，低有效） */
    rt_uint8_t sw_kl3;          /* 左转（KL3，低有效） */
    rt_uint8_t sw_kl4;          /* 右转（KL4，低有效） */
    rt_uint8_t sw_kl5;          /* 远光（KL5，低有效） */
    rt_uint8_t sw_kl6;          /* 近光（KL6，低有效） */
    rt_uint8_t sw_kl7;          /* 后雾灯（KL7，低有效） */
    rt_uint8_t sw_kl8;          /* 倒车（KL8，低有效） */
    rt_uint8_t sw_kl9;          /* 安全带（KL9，高有效） */
    rt_uint8_t sw_kl10;         /* 车门（KL10，高有效） */

    /* ---- 锂电池状态 ---- */
    rt_uint8_t li_bat_stdby;    /* 锂电池待机状态（1=待机模式） */
    rt_uint8_t li_bat_chrg;     /* 锂电池充电状态（1=正在充电） */
    rt_uint32_t li_bat_raw;     /* 锂电池 ADC 原始值 */
    rt_uint32_t li_bat_est_mv;  /* 锂电池估算电压（mV） */
} app_vehicle_io_state_t;


#endif /* APPLICATIONS_APP_TYPES_H_ */
