/*
 * svc_adc.h — ADC 采样服务接口
 *
 * 提供 4 通道 ADC 采样数据获取和按键事件消费。
 * 数据流向：svc_adc 内部采样 → 其他服务通过 API 读取。
 *
 * 按键事件采用"一次性消费"模式：
 *   调用 svc_adc_consume_s1~4_event() 后事件标志被清除，
 *   只有一次有效，防止重复触发。
 */
#ifndef APPLICATIONS_SVC_ADC_H_
#define APPLICATIONS_SVC_ADC_H_

#include "app_types.h"

/** 初始化 ADC 全局状态变量 */
int svc_adc_init(void);
/** 创建并启动 ADC 采样线程（10ms 周期） */
int svc_adc_task_start(void);
/** 获取最新 ADC 采样快照（指针，非拷贝，线程读取安全需外部保证） */
const app_adc_snapshot_t *svc_adc_get_snapshot(void);
/** 消费 S1 按键事件（一次性，消费后置 false） */
rt_bool_t svc_adc_consume_s1_event(void);
/** 消费 S2 按键事件 */
rt_bool_t svc_adc_consume_s2_event(void);
/** 消费 S3 按键事件 */
rt_bool_t svc_adc_consume_s3_event(void);
/** 消费 S4 按键事件 */
rt_bool_t svc_adc_consume_s4_event(void);
/** 是否有任意按键被按下（不消费事件，只做判断） */
rt_bool_t svc_adc_is_any_key_pressed(void);


#endif /* APPLICATIONS_SVC_ADC_H_ */
