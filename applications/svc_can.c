/*
 * svc_can.c — CAN 通信服务（app_can 的服务层包装）
 *
 * 本文件是 app_can 模块的薄封装层，目的是：
 *   1. 统一服务层命名风格（svc_ 前缀）
 *   2. 隔离底层实现变化（如果 app_can 接口变化，只需改这里）
 *   3. 后续可在这一层添加 CAN 数据分发逻辑（如消息路由到不同模块）
 *
 * 当前实现是纯委托模式，所有函数直接转发到 app_can。
 */
#include "svc_can.h"
#include "app_can.h"

int svc_can_init(void)
{
    return app_can_init();
}

int svc_can_task_start(void)
{
    return app_can_start();
}

int svc_can_send(rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len)
{
    return app_can_send(id, data, len);
}

int svc_can_send_ex(rt_uint8_t node_index, rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len)
{
    return app_can_send_ex(node_index, id, data, len);
}
