#ifndef APPLICATIONS_SVC_VEHICLE_IO_H_
#define APPLICATIONS_SVC_VEHICLE_IO_H_

#include "app_types.h"

/*
 * ============================================================
 *  车辆 IO 输入采集服务 (Vehicle I/O Service)
 * ============================================================
 *
 * 功能:
 *   采集车辆的各种开关量和模拟量输入, 包括:
 *   - WK_ACC / WK_ON: 车辆点火信号 (唤醒 MCU 用)
 *   - KL1 ~ KL10: 车辆信号输入 (低电平有效或高电平有效)
 *   - LI_BAT_STDBY: 锂电池待机状态
 *   - LI_BAT_CHRG:  锂电池充电状态
 *   - LI_BAT:       锂电池电压 (通过 ADC 采集)
 *
 * 数据流向:
 *   GPIO 引脚电平 → svc_vehicle_io_update_state() → g_vehicle_io_state
 *                                                      ↓
 *   svc_power (电源管理) 读取 WK_ACC/WK_ON 判断车辆状态
 *   app_can   (CAN 通信) 读取 SW_KL 状态发送到 CAN 总线
 */

/* ---------- 初始化与任务 ---------- */

/**
 * @brief 初始化车辆 IO 采集模块
 *
 * 初始化内容:
 *   1) 调用 board_init_io_pins() 配置 GPIO 引脚方向/上下拉
 *   2) 调用 init_input_switch_pins() 配置 KL 开关引脚
 *   3) 清零软件状态结构体 g_vehicle_io_state
 *   4) 重置调试打印的上次状态记录
 */
int svc_vehicle_io_init(void);

/**
 * @brief 创建车辆 IO 采集线程
 *
 * 线程工作模式:
 *   周期性扫描所有输入引脚 (周期由 APP_IO_TASK_PERIOD_MS 配置)
 *   每次扫描后更新 g_vehicle_io_state, 其他模块通过
 *   svc_vehicle_io_get_state() 获取最新状态
 */
int svc_vehicle_io_task_start(void);

/**
 * @brief 获取当前车辆 IO 状态快照
 * @return 指向只读状态结构体的指针
 *
 * 使用方式:
 *   其他模块(如 svc_power, app_can)通过此函数
 *   获取最新的车辆输入状态, 不需要自己读 GPIO
 *
 * 注意:
 *   返回的指针指向内部静态变量, 调用者只能读不能修改
 *   多线程环境下, 读取瞬间是原子的(rt_uint32_t 级别),
 *   但整体状态可能不是完全一致的瞬间快照
 */
const app_vehicle_io_state_t *svc_vehicle_io_get_state(void);

#endif /* APPLICATIONS_SVC_VEHICLE_IO_H_ */
