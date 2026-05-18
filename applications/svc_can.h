/*
 * svc_can.h — CAN 通信服务接口
 *
 * 这个文件是 svc_can.c 的头文件，提供 CAN 初始化和发送接口。
 * svc_can 是 app_can 的轻量包装，主要为了统一服务层接口风格。
 *
 * 发送接口支持两种模式：
 *   svc_can_send()     — 使用默认 CAN 节点（node 0）
 *   svc_can_send_ex()  — 指定 CAN 节点索引
 */
#ifndef APPLICATIONS_SVC_CAN_H_
#define APPLICATIONS_SVC_CAN_H_

#include <rtthread.h>

/** 初始化 CAN 硬件和内部状态 */
int svc_can_init(void);
/** 创建并启动 CAN 通信线程 */
int svc_can_task_start(void);
/** 在默认 CAN 节点上发送一帧标准/扩展数据 */
int svc_can_send(rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len);
/** 在指定 CAN 节点上发送（node_index = 0/1/2） */
int svc_can_send_ex(rt_uint8_t node_index, rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len);


#endif /* APPLICATIONS_SVC_CAN_H_ */
