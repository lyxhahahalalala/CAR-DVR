#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include "rtt_board.h"
#include "app_config.h"
#include "app_can.h"

#define DBG_TAG "app_can"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define APP_CAN_STD_LIFE_ID      0x605U
#define APP_CAN_EXT_LIFE_ID      0x16FF0018UL
#define APP_CAN_LIFE_TIMEOUT_MS  5000U
#define APP_CAN_RX_FIFO_INDEX    0U

typedef struct
{
    const char *dev_name;
    const char *sem_name;
    rt_device_t dev;
    rt_sem_t rx_sem;
    rt_thread_t rx_thread;
    rt_tick_t last_rx_ms;
    rt_uint8_t life;
    rt_bool_t online;
} app_can_node_t;

static app_can_node_t g_can_nodes[CAN_NODE_ALL] = {
    {"can0", "can0_sem", RT_NULL, RT_NULL, RT_NULL, 0U, 0U, RT_FALSE},
    {"can2", "can2_sem", RT_NULL, RT_NULL, RT_NULL, 0U, 0U, RT_FALSE},
    {RT_NULL, RT_NULL, RT_NULL, RT_NULL, RT_NULL, 0U, 0U, RT_FALSE},
};

static rt_thread_t g_can_monitor_thread = RT_NULL;
static rt_bool_t g_can_started = RT_FALSE;

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

static void app_can_handle_rx(uint8_t node_index, const struct rt_can_msg *msg)
{
    app_can_node_t *node;

    if ((node_index >= CAN_NODE_ALL) || (msg == RT_NULL)) {
        return;
    }

    node = &g_can_nodes[node_index];

    LOG_I("CAN%u rx: id=0x%08lx len=%u data0=0x%02x",
          (unsigned int)(node_index + 1U),
          (unsigned long)msg->id,
          (unsigned int)msg->len,
          (unsigned int)msg->data[0]);

    if ((msg->id == APP_CAN_STD_LIFE_ID) || (msg->id == APP_CAN_EXT_LIFE_ID)) {
        node->life = msg->data[0];
        node->last_rx_ms = rt_tick_get_millisecond();
        node->online = RT_TRUE;

        LOG_I("CAN%u life=%u online=1",
              (unsigned int)(node_index + 1U),
              (unsigned int)node->life);
    }
}


static rt_err_t app_can_rx_indicate_0(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);
    APP_NON_CAN_LOG("can0 rx indicate, size=%u\r\n", (unsigned int)size);

    if (g_can_nodes[CAN_NODE1].rx_sem != RT_NULL) {
        rt_sem_release(g_can_nodes[CAN_NODE1].rx_sem);
    }
    return RT_EOK;
}

static rt_err_t app_can_rx_indicate_2(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);
    APP_NON_CAN_LOG("can2 rx indicate, size=%u\r\n", (unsigned int)size);

    if (g_can_nodes[CAN_NODE2].rx_sem != RT_NULL) {
        rt_sem_release(g_can_nodes[CAN_NODE2].rx_sem);
    }
    return RT_EOK;
}

static rt_err_t app_can_rx_indicate_3(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);
    APP_NON_CAN_LOG("can3 rx indicate, size=%u\r\n", (unsigned int)size);

    if (g_can_nodes[CAN_NODE3].rx_sem != RT_NULL) {
        rt_sem_release(g_can_nodes[CAN_NODE3].rx_sem);
    }
    return RT_EOK;
}


static rt_err_t (* const g_can_rx_indicate[CAN_NODE_ALL])(rt_device_t dev, rt_size_t size) = {
    app_can_rx_indicate_0,
    app_can_rx_indicate_2,
    app_can_rx_indicate_3,
};


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
        if (rt_sem_take(node->rx_sem, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }

        while (1)
        {
            memset(&msg, 0, sizeof(msg));
#ifdef RT_CAN_USING_HDR
            msg.hdr_index = 0;
#endif
            if (rt_device_read(node->dev, APP_CAN_RX_FIFO_INDEX, &msg, sizeof(msg)) <= 0) {
                break;
            }

            if (msg.len > 8U) {
                continue;
            }

            app_can_handle_rx(node_index, &msg);
        }
    }
}

static void app_can_monitor_thread(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        rt_tick_t now = rt_tick_get_millisecond();
        uint8_t i;

        for (i = 0; i < CAN_NODE_ALL; i++) {
            if (g_can_nodes[i].online == RT_TRUE) {
                if ((now - g_can_nodes[i].last_rx_ms) >= APP_CAN_LIFE_TIMEOUT_MS) {
                    g_can_nodes[i].online = RT_FALSE;
                    g_can_nodes[i].life = 0U;
                    LOG_W("CAN%u offline", (unsigned int)(i + 1U));
                }

            }
        }

        rt_thread_mdelay(100);
    }
}

static rt_err_t app_can_config_node(uint8_t node_index)
{
    app_can_node_t *node;
    rt_err_t result;

    if (node_index >= CAN_NODE_ALL) {
        return -RT_EINVAL;
    }

    node = &g_can_nodes[node_index];

    if ((node->dev_name == RT_NULL) || (node->sem_name == RT_NULL)) {
        return RT_EOK;
    }

    node->dev = rt_device_find(node->dev_name);
    if (node->dev == RT_NULL) {
        LOG_E("%s not found", node->dev_name);
        return -RT_ENOSYS;
    }

    node->rx_sem = rt_sem_create(node->sem_name, 0, RT_IPC_FLAG_FIFO);
    if (node->rx_sem == RT_NULL) {
        LOG_E("create %s failed", node->sem_name);
        return -RT_ENOMEM;
    }

    result = rt_device_open(node->dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    if (result != RT_EOK) {
        LOG_E("open %s failed: %d", node->dev_name, result);
        return result;
    }

    result = rt_device_control(node->dev, RT_CAN_CMD_SET_BAUD, (void *)APP_CAN_BAUDRATE);
    if (result != RT_EOK) {
        LOG_E("set %s baud failed: %d", node->dev_name, result);
        return result;
    }

    result = rt_device_control(node->dev, RT_CAN_CMD_SET_MODE, (void *)RT_CAN_MODE_NORMAL);
    if (result != RT_EOK) {
        LOG_E("set %s mode failed: %d", node->dev_name, result);
        return result;
    }

#ifdef RT_CAN_USING_HDR
    result = rt_device_control(node->dev, RT_CAN_CMD_SET_FILTER, &g_can_filter_cfg);
    if (result != RT_EOK) {
        LOG_E("set %s filter failed: %d", node->dev_name, result);
        return result;
    }
#endif

    result = rt_device_set_rx_indicate(node->dev, g_can_rx_indicate[node_index]);
    if (result != RT_EOK) {
        LOG_E("set %s rx callback failed: %d", node->dev_name, result);
        return result;
    }

    return RT_EOK;
}

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
            continue;
        }

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
    LOG_I("CAN started");

    return RT_EOK;
}

int app_can_send(rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len)
{
    struct rt_can_msg msg;

    if ((g_can_started == RT_FALSE) || (g_can_nodes[CAN_NODE1].dev == RT_NULL)) {
        return -RT_ERROR;
    }

    if (len > 8U) {
        return -RT_EINVAL;
    }

    memset(&msg, 0, sizeof(msg));
    msg.id = id;
    msg.ide = (id > 0x7FFU) ? RT_CAN_EXTID : RT_CAN_STDID;
    msg.rtr = RT_CAN_DTR;
    msg.len = len;
    if ((data != RT_NULL) && (len > 0U)) {
        memcpy(msg.data, data, len);
    }

    if (rt_device_write(g_can_nodes[CAN_NODE1].dev, 0, &msg, sizeof(msg)) <= 0) {
        return -RT_ERROR;
    }

    return RT_EOK;
}

rt_uint8_t app_can_get_life(uint8_t node_index)
{
    if (node_index >= CAN_NODE_ALL) {
        return 0U;
    }

    return g_can_nodes[node_index].life;
}

rt_bool_t app_can_is_online(uint8_t node_index)
{
    if (node_index >= CAN_NODE_ALL) {
        return RT_FALSE;
    }

    return g_can_nodes[node_index].online;
}
