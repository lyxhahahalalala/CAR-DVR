/*
 * svc_power.h — 电源管理服务接口
 *
 * 提供电源状态机的初始化和线程启动接口。
 * 其他模块通过 svc_vehicle_io 获取 ACC/ON 状态，
 * 通过 svc_adc 获取电压数据，不需要直接调用 svc_power。
 */
#ifndef APPLICATIONS_SVC_POWER_H_
#define APPLICATIONS_SVC_POWER_H_

/** 初始化电源控制引脚和全局状态变量 */
int svc_power_init(void);
/** 创建并启动电源管理线程（优先级 8，最高） */
int svc_power_task_start(void);

#endif /* APPLICATIONS_SVC_POWER_H_ */
