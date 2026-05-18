/*
 * app_can.c — CAN 通信底层实现
 *
 * 管理 3 个 CAN 节点（can0, can2, can3(disabled)），提供：
 *   1. CAN 设备初始化（打开/设置波特率/设置过滤/注册接收回调）
 *   2. 独立接收线程（每个节点一个，信号量触发）
 *   3. 生命监测（监听 ID 0x605/0x16FF0018，5s 超时判离线）
 *   4. 数据发送
 *
 * 设计思路：
 *   - 每个 CAN 节点有独立的接收线程和信号量，互不阻塞
 *   - 接收和发送都通过 RT-Thread 设备驱动框架，不直接操作寄存器
 *   - 生命监测使用独立线程，不干扰数据收发
 *   - 第 3 个 CAN 节点 dev_name 为 NULL，方便后续扩展（不改数组结构）
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include "rtt_board.h"
#include "app_config.h"
#include "app_can.h"

#define DBG_TAG "app_can"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define APP_CAN_STD_LIFE_ID      0x605U       /* CAN 标准帧生命信号 ID */
#define APP_CAN_EXT_LIFE_ID      0x16FF0018UL /* CAN 扩展帧生命信号 ID（行驶记录仪国标） */
#define APP_CAN_LIFE_TIMEOUT_MS  5000U        /* 生命信号超时时间：5s 无消息判离线 */
#define APP_CAN_RX_FIFO_INDEX    0U           /* CAN 接收 FIFO 索引 */

/* CAN 节点数据结构：每个节点独立管理自己的设备句柄/信号量/线程 */
typedef struct
{
    const char *dev_name;        /* CAN 设备名称（"can0"/"can2"/NULL） */
    const char *sem_name;        /* 接收信号量名称 */
    rt_device_t dev;             /* RT-Thread 设备句柄 */
    rt_sem_t rx_sem;             /* 接收信号量（非空时表示有数据可读） */
    rt_thread_t rx_thread;       /* 接收线程句柄 */
    rt_tick_t last_rx_ms;        /* 最后一次收到生命信号的时间戳（ms） */
    rt_uint8_t life;             /* 当前生命值（从生命帧 data[0] 获取） */
    rt_bool_t online;            /* 节点是否在线（由监控线程根据超时更新） */
} app_can_node_t;

/* 三个 CAN 节点的全局实例（第 3 个 dev_name=NULL 表示禁用） */
static app_can_node_t g_can_nodes[CAN_NODE_ALL] = {
    {"can0", "can0_sem", RT_NULL, RT_NULL, RT_NULL, 0U, 0U, RT_FALSE},
    {"can2", "can2_sem", RT_NULL, RT_NULL, RT_NULL, 0U, 0U, RT_FALSE},
    {RT_NULL, RT_NULL, RT_NULL, RT_NULL, RT_NULL, 0U, 0U, RT_FALSE},
};

static rt_thread_t g_can_monitor_thread = RT_NULL;  /* 生命监控线程 */
static rt_bool_t g_can_started = RT_FALSE;           /* CAN 系统是否已启动 */

/* CAN 过滤配置：允许接收所有标准帧和扩展帧（不过滤任何 ID） */
#ifdef RT_CAN_USING_HDR
static struct rt_can_filter_item g_can_filter_items[] = {
    {0x000U, RT_CAN_STDID, RT_CAN_DTR, 0, 0x000U, -1, CAN_RX_FIFO0, RT_NULL, RT_NULL},
    {0x000U, RT_CAN_EXTID, RT_CAN_DTR, 0, 0x000U, -1, CAN_RX_FIFO0, RT_NULL, RT_NULL},
};

static struct rt_can_filter_config g_can_filter_cfg = {
    sizeof(g_can_filter_items) / sizeof(g_can_filter_items[0]),
    1,
    g_can_filter_items,
};
#endif

/*
 * 处理接收到的 CAN 消息
 *
 * 检查消息 ID 是否为生命信号帧。
 * 如果是：
 *   - 提取 data[0] 作为生命值
 *   - 记录接收时间戳（用于后续超时判断）
 *   - 标记节点在线
 *
 * @param node_index 接收节点索引
 * @param msg        接收到的 CAN 帧
 */
static void app_can_handle_rx(uint8_t node_index, const struct rt_can_msg *msg)
{
    app_can_node_t *node;

    if ((node_index >= CAN_NODE_ALL) || (msg == RT_NULL)) {
        return;
    }

    node = &g_can_nodes[node_index];

    /* 检查是否是生命信号帧（标准 ID 0x605 或 扩展 ID 0x16FF0018） */
    if ((msg->id == APP_CAN_STD_LIFE_ID) || (msg->id == APP_CAN_EXT_LIFE_ID)) {
        node->life = msg->data[0];                  /* 生命值 */
        node->last_rx_ms = rt_tick_get_millisecond(); /* 记录接收时刻 */
        node->online = RT_TRUE;                      /* 标记在线 */
    }
}

/*
 * CAN 节点 0（can0）接收回调（中断上下文调用）
 *
 * 当 can0 收到数据时，RT-Thread 驱动调用此函数。
 * 此函数释放信号量通知接收线程去读取数据。
 * 注意：回调在中断上下文中，不能做耗时操作和加锁。
 */
static rt_err_t app_can_rx_indicate_0(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);
    RT_UNUSED(size);

    if (g_can_nodes[CAN_NODE1].rx_sem != RT_NULL) {
        rt_sem_release(g_can_nodes[CAN_NODE1].rx_sem);
    }
    return RT_EOK;
}

/* CAN 节点 1（can2）接收回调 */
static rt_err_t app_can_rx_indicate_2(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);
    RT_UNUSED(size);

    if (g_can_nodes[CAN_NODE2].rx_sem != RT_NULL) {
        rt_sem_release(g_can_nodes[CAN_NODE2].rx_sem);
    }
    return RT_EOK;
}

/* CAN 节点 2（can3，禁用）接收回调 */
static rt_err_t app_can_rx_indicate_3(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);
    RT_UNUSED(size);

    if (g_can_nodes[CAN_NODE3].rx_sem != RT_NULL) {
        rt_sem_release(g_can_nodes[CAN_NODE3].rx_sem);
    }
    return RT_EOK;
}

/* 函数指针表：节点索引 → 接收回调函数 */
static rt_err_t (* const g_can_rx_indicate[CAN_NODE_ALL])(rt_device_t dev, rt_size_t size) = {
    app_can_rx_indicate_0,
    app_can_rx_indicate_2,
    app_can_rx_indicate_3,
};

/*
 * CAN 接收线程
 *
 * 每个激活的 CAN 节点都有一个独立的接收线程。
 * 线程阻塞在信号量上，收到信号量后循环读取 FIFO 中所有消息。
 * 用 while(1) 内层循环确保 FIFO 非空时全部读完，避免多次信号量释放。
 *
 * @param parameter 节点索引（void* 转 uint8_t）
 */
static void app_can_rx_thread(void *parameter)
{
    uint8_t node_index = (uint8_t)(uintptr_t)parameter;
    app_can_node_t *node;
    struct rt_can_msg msg;

    if (node_index >= CAN_NODE_ALL) {
        return;
    }

    node = &g_can_nodes[node_index];

    while (1)
    {
        /* 等待信号量（有数据/新消息时释放） */
        if (rt_sem_take(node->rx_sem, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }

        /* 循环读取所有待处理消息，直到 FIFO 为空 */
        while (1)
        {
            memset(&msg, 0, sizeof(msg));
#ifdef RT_CAN_USING_HDR
            msg.hdr_index = 0;
#endif
            if (rt_device_read(node->dev, APP_CAN_RX_FIFO_INDEX, &msg, sizeof(msg)) <= 0) {
                break;  /* 没有更多数据了 */
            }

            if (msg.len > 8U) {
                continue;  /* 帧长度异常，跳过 */
            }

            app_can_handle_rx(node_index, &msg);
        }
    }
}

/*
 * CAN 生命监控线程
 *
 * 每隔 100ms 检查所有节点的生命信号超时。
 * 如果一个节点在 APP_CAN_LIFE_TIMEOUT_MS（5s）内没有收到生命帧，
 * 则标记为离线（online=RT_FALSE），生命值清零。
 */
static void app_can_monitor_thread(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        rt_tick_t now = rt_tick_get_millisecond();
        uint8_t i;

        for (i = 0; i < CAN_NODE_ALL; i++) {
            if ((g_can_nodes[i].online == RT_TRUE) &&
                ((now - g_can_nodes[i].last_rx_ms) >= APP_CAN_LIFE_TIMEOUT_MS)) {
                g_can_nodes[i].online = RT_FALSE;
                g_can_nodes[i].life = 0U;
            }
        }

        rt_thread_mdelay(100);
    }
}

/*
 * 配置单个 CAN 节点
 *
 * 执行 CAN 设备的完整初始化序列：
 *   1. 查找设备 → 2. 创建信号量 → 3. 打开设备 → 4. 设置波特率
 *   5. 设置模式 → 6. 设置过滤 → 7. 注册接收回调
 *
 * 如果节点的 dev_name == NULL（如第 3 个节点），跳过配置。
 *
 * @param node_index 节点索引
 * @return RT_EOK 成功，其他值表示失败
 */
static rt_err_t app_can_config_node(uint8_t node_index)
{
    app_can_node_t *node;
    rt_err_t result;

    if (node_index >= CAN_NODE_ALL) {
        return -RT_EINVAL;
    }

    node = &g_can_nodes[node_index];

    /* dev_name 为 NULL 表示该节点未启用（如 CAN_NODE3） */
    if ((node->dev_name == RT_NULL) || (node->sem_name == RT_NULL)) {
        return RT_EOK;
    }

    /* 第 1 步：查找 CAN 设备 */
    node->dev = rt_device_find(node->dev_name);
    if (node->dev == RT_NULL) {
        LOG_E("%s not found", node->dev_name);
        return -RT_ENOSYS;
    }

    /* 第 2 步：创建接收信号量（初始计数=0） */
    node->rx_sem = rt_sem_create(node->sem_name, 0, RT_IPC_FLAG_FIFO);
    if (node->rx_sem == RT_NULL) {
        LOG_E("create %s failed", node->sem_name);
        return -RT_ENOMEM;
    }

    /* 第 3 步：以中断方式打开设备（INT_TX | INT_RX） */
    result = rt_device_open(node->dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    if (result != RT_EOK) {
        LOG_E("open %s failed: %d", node->dev_name, result);
        return result;
    }

    /* 第 4 步：设置波特率（250kbps） */
    result = rt_device_control(node->dev, RT_CAN_CMD_SET_BAUD, (void *)APP_CAN_BAUDRATE);
    if (result != RT_EOK) {
        LOG_E("set %s baud failed: %d", node->dev_name, result);
        return result;
    }

    /* 第 5 步：设置为正常模式 */
    result = rt_device_control(node->dev, RT_CAN_CMD_SET_MODE, (void *)RT_CAN_MODE_NORMAL);
    if (result != RT_EOK) {
        LOG_E("set %s mode failed: %d", node->dev_name, result);
        return result;
    }

    /* 第 6 步：设置过滤（允许所有 ID 通过） */
#ifdef RT_CAN_USING_HDR
    result = rt_device_control(node->dev, RT_CAN_CMD_SET_FILTER, &g_can_filter_cfg);
    if (result != RT_EOK) {
        LOG_E("set %s filter failed: %d", node->dev_name, result);
        return result;
    }
#endif

    /* 第 7 步：注册接收通知回调（中断级） */
    result = rt_device_set_rx_indicate(node->dev, g_can_rx_indicate[node_index]);
    if (result != RT_EOK) {
        LOG_E("set %s rx callback failed: %d", node->dev_name, result);
        return result;
    }

    return RT_EOK;
}

/*
 * CAN 模块初始化
 *
 * 重置所有节点的状态变量。不执行任何硬件操作。
 * 硬件操作在 app_can_start() 中延迟执行。
 */
int app_can_init(void)
{
    uint8_t i;

    for (i = 0; i < CAN_NODE_ALL; i++) {
        g_can_nodes[i].dev = RT_NULL;
        g_can_nodes[i].rx_sem = RT_NULL;
        g_can_nodes[i].rx_thread = RT_NULL;
        g_can_nodes[i].last_rx_ms = 0U;
        g_can_nodes[i].life = 0U;
        g_can_nodes[i].online = RT_FALSE;
    }

    g_can_monitor_thread = RT_NULL;
    g_can_started = RT_FALSE;

    return RT_EOK;
}

/*
 * 启动 CAN 系统
 *
 * 1. 配置每个 CAN 节点的硬件（打开设备/设置波特率等）
 * 2. 为激活的节点创建并启动接收线程
 * 3. 创建并启动生命监控线程
 *
 * 注意：g_can_started 保护防止重复启动。
 */
int app_can_start(void)
{
    uint8_t i;
    rt_err_t result;

    if (g_can_started == RT_TRUE) {
        return RT_EOK;
    }

    for (i = 0; i < CAN_NODE_ALL; i++) {
        result = app_can_config_node(i);
        if (result != RT_EOK) {
            return result;
        }

        if (g_can_nodes[i].dev == RT_NULL) {
            continue;  /* 该节点未启用 */
        }

        /* 创建接收线程（每个节点独立线程） */
        g_can_nodes[i].rx_thread = rt_thread_create(
            g_can_nodes[i].dev_name,
            app_can_rx_thread,
            (void *)(uintptr_t)i,
            APP_CAN_RX_THREAD_STACK,
            APP_CAN_RX_THREAD_PRIORITY,
            APP_CAN_RX_THREAD_TICK);
        if (g_can_nodes[i].rx_thread == RT_NULL) {
            LOG_E("create %s rx thread failed", g_can_nodes[i].dev_name);
            return -RT_ENOMEM;
        }

        rt_thread_startup(g_can_nodes[i].rx_thread);
    }

    /* 创建监控线程（生命超时监测） */
    g_can_monitor_thread = rt_thread_create(
        APP_CAN_TX_THREAD_NAME,
        app_can_monitor_thread,
        RT_NULL,
        APP_CAN_TX_THREAD_STACK,
        APP_CAN_TX_THREAD_PRIORITY,
        APP_CAN_TX_THREAD_TICK);
    if (g_can_monitor_thread == RT_NULL) {
        LOG_E("create can monitor thread failed");
        return -RT_ENOMEM;
    }

    rt_thread_startup(g_can_monitor_thread);
    g_can_started = RT_TRUE;

    return RT_EOK;
}

/*
 * 在指定 CAN 节点发送一帧数据
 *
 * 自动判断帧类型：
 *   标准 ID（≤0x7FF）→ STID
 *   扩展 ID（>0x7FF）→ EXTID
 *
 * @param node_index 节点索引（0/1/2）
 * @param id         CAN ID
 * @param data       数据缓冲区
 * @param len        数据长度（≤8）
 * @return RT_EOK 成功
 */
int app_can_send_ex(uint8_t node_index, uint32_t id, const uint8_t *data, uint8_t len)
{
    struct rt_can_msg msg;

    if (node_index >= CAN_NODE_ALL) {
        return -RT_EINVAL;
    }

    if (g_can_nodes[node_index].dev == RT_NULL) {
        return -RT_ENOSYS;
    }

    if (len > 8U) {
        return -RT_EINVAL;
    }

    memset(&msg, 0, sizeof(msg));
    msg.id = id;
    msg.ide = (id > 0x7FFU) ? RT_CAN_EXTID : RT_CAN_STDID;  /* 自动判断标准/扩展帧 */
    msg.rtr = RT_CAN_DTR;
    msg.len = len;

    if ((data != RT_NULL) && (len > 0U)) {
        memcpy(msg.data, data, len);
    }

#ifdef RT_CAN_USING_HDR
    msg.hdr_index = 0;
#endif

    if (rt_device_write(g_can_nodes[node_index].dev, 0, &msg, sizeof(msg)) <= 0) {
        LOG_E("CAN%u tx failed: id=0x%08lx len=%u",
              (unsigned int)(node_index + 1U),
              (unsigned long)id,
              (unsigned int)len);
        return -RT_ERROR;
    }

    return RT_EOK;
}

/*
 * 在默认 CAN 节点（节点 0）发送一帧数据
 * 是 app_can_send_ex 的简化版本
 */
int app_can_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    return app_can_send_ex(CAN_NODE1, id, data, len);
}

/*
 * 获取指定节点的当前生命值
 * @param node_index 节点索引
 * @return 生命值（最近生命帧的 data[0]），节点无效返回 0
 */
rt_uint8_t app_can_get_life(uint8_t node_index)
{
    if (node_index >= CAN_NODE_ALL) {
        return 0U;
    }

    return g_can_nodes[node_index].life;
}

/*
 * 查询指定节点是否在线
 * @param node_index 节点索引
 * @return RT_TRUE 在线（5s 内收到过生命帧），RT_FALSE 离线
 */
rt_bool_t app_can_is_online(uint8_t node_index)
{
    if (node_index >= CAN_NODE_ALL) {
        return RT_FALSE;
    }

    return g_can_nodes[node_index].online;
}
