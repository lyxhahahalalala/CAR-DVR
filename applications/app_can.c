#include <rtthread.h>
#include <rtdevice.h>

#include "app_can.h"
#include "app_config.h"
#include "rtt_board.h"

typedef struct
{
    rt_uint32_t id;
    rt_uint8_t ide;
    rt_uint8_t rtr;
    rt_uint8_t len;
    rt_uint8_t data[8];
} app_can_tx_msg_t;

static rt_device_t g_can_dev;
static rt_sem_t g_can_rx_sem;
static rt_mq_t g_can_tx_mq;
static rt_bool_t g_can_started;

static rt_thread_t g_can_rx_thread;
static rt_thread_t g_can_tx_thread;
static rt_thread_t g_can_err_thread;
#if APP_CAN_TEST_TX_ENABLE
static rt_thread_t g_can_test_tx_thread;
#endif

static rt_err_t app_can_rx_indicate(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);
    RT_UNUSED(size);

    if (g_can_rx_sem != RT_NULL)
    {
        rt_sem_release(g_can_rx_sem);
    }

    return RT_EOK;
}

static void app_can_dump_frame(const struct rt_can_msg *msg)
{
    APP_CAN_LOG("CAN RX: id=0x%08x len=%d data=%02x %02x %02x %02x %02x %02x %02x %02x\r\n",
               msg->id,
               msg->len,
               msg->data[0],
               msg->data[1],
               msg->data[2],
               msg->data[3],
               msg->data[4],
               msg->data[5],
               msg->data[6],
               msg->data[7]);
}

static void app_can_rx_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        struct rt_can_msg rx_msg = {0};

        rt_sem_take(g_can_rx_sem, RT_WAITING_FOREVER);

#ifdef RT_CAN_USING_HDR
        rx_msg.hdr_index = BOARD_CAN_HWFILTER_INDEX;
#endif

        if (rt_device_read(g_can_dev, 0, &rx_msg, sizeof(rx_msg)) > 0)
        {
            if (rx_msg.len <= sizeof(rx_msg.data))
            {
                app_can_dump_frame(&rx_msg);
            }
        }
    }
}

static void app_can_tx_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        app_can_tx_msg_t queued_msg = {0};

        if (rt_mq_recv(g_can_tx_mq, &queued_msg, sizeof(queued_msg), RT_WAITING_FOREVER) > 0)
        {
            struct rt_can_msg tx_msg = {0};

            tx_msg.id = queued_msg.id;
            tx_msg.ide = queued_msg.ide;
            tx_msg.rtr = queued_msg.rtr;
            tx_msg.len = queued_msg.len;
            rt_memcpy(tx_msg.data, queued_msg.data, queued_msg.len);

            if (rt_device_write(g_can_dev, 0, &tx_msg, sizeof(tx_msg)) <= 0)
            {
                APP_CAN_LOG("CAN TX: send failed, id=0x%08x\r\n", tx_msg.id);
            }
        }
    }
}

static void app_can_err_thread_entry(void *parameter)
{
    rt_uint32_t last_errcode = 0xFFFFFFFFUL;
    rt_uint32_t last_lasterr = 0xFFFFFFFFUL;

    RT_UNUSED(parameter);

    while (1)
    {
        struct rt_can_status status = {0};

        if (rt_device_control(g_can_dev, RT_CAN_CMD_GET_STATUS, &status) == RT_EOK)
        {
            if ((status.errcode != last_errcode) || (status.lasterrtype != last_lasterr))
            {
                APP_CAN_LOG("CAN ST: err=%lu last=%lu rxerr=%lu txerr=%lu\r\n",
                           status.errcode,
                           status.lasterrtype,
                           status.rcverrcnt,
                           status.snderrcnt);
                last_errcode = status.errcode;
                last_lasterr = status.lasterrtype;
            }
        }

        rt_thread_mdelay(APP_CAN_TASK_PERIOD_MS);
    }
}

#if APP_CAN_TEST_TX_ENABLE
static void app_can_test_tx_thread_entry(void *parameter)
{
    static const rt_uint8_t test_data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

    RT_UNUSED(parameter);

    while (1)
    {
        if (app_can_send(APP_CAN_TEST_TX_ID, test_data, sizeof(test_data)) != RT_EOK)
        {
            APP_CAN_LOG("CAN TEST: queue failed\r\n");
        }
        else
        {
           // APP_CAN_LOG("CAN TEST: queued id=0x%03x\r\n", APP_CAN_TEST_TX_ID);
        }

        rt_thread_mdelay(APP_CAN_TEST_TX_PERIOD_MS);
    }
}
#endif

int app_can_send(rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len)
{
    app_can_tx_msg_t queued_msg = {0};

    if ((g_can_tx_mq == RT_NULL) || (data == RT_NULL) || (len > sizeof(queued_msg.data)))
    {
        return -RT_ERROR;
    }

    queued_msg.id = id;
    queued_msg.ide = (id > 0x7FFU) ? RT_CAN_EXTID : RT_CAN_STDID;
    queued_msg.rtr = RT_CAN_DTR;
    queued_msg.len = len;
    rt_memcpy(queued_msg.data, data, len);

    return (rt_mq_send(g_can_tx_mq, &queued_msg, sizeof(queued_msg)) == RT_EOK) ? RT_EOK : -RT_ERROR;
}

int app_can_init(void)
{
    rt_err_t result;

    g_can_dev = rt_device_find(APP_CAN_DEV_NAME);
    if (g_can_dev == RT_NULL)
    {
        APP_CAN_LOG("CAN: device %s not found\r\n", APP_CAN_DEV_NAME);
        return -RT_ERROR;
    }

    g_can_rx_sem = rt_sem_create(APP_CAN_RX_THREAD_NAME, 0, RT_IPC_FLAG_FIFO);
    if (g_can_rx_sem == RT_NULL)
    {
        APP_CAN_LOG("CAN: rx semaphore create failed\r\n");
        return -RT_ERROR;
    }

    g_can_tx_mq = rt_mq_create(APP_CAN_TX_MQ_NAME,
                               sizeof(app_can_tx_msg_t),
                               APP_CAN_TX_MQ_DEPTH,
                               RT_IPC_FLAG_FIFO);
    if (g_can_tx_mq == RT_NULL)
    {
        APP_CAN_LOG("CAN: tx queue create failed\r\n");
        return -RT_ERROR;
    }

    result = rt_device_open(g_can_dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    if (result != RT_EOK)
    {
        APP_CAN_LOG("CAN: open %s failed, ret=%d\r\n", APP_CAN_DEV_NAME, result);
        return -RT_ERROR;
    }

    result = rt_device_control(g_can_dev, RT_CAN_CMD_SET_BAUD, (void *)APP_CAN_BAUDRATE);
    if (result != RT_EOK)
    {
        APP_CAN_LOG("CAN: set baud failed, ret=%d\r\n", result);
        return -RT_ERROR;
    }

    result = rt_device_control(g_can_dev, RT_CAN_CMD_SET_MODE, (void *)RT_CAN_MODE_NORMAL);
    if (result != RT_EOK)
    {
        APP_CAN_LOG("CAN: set mode failed, ret=%d\r\n", result);
        return -RT_ERROR;
    }

    result = rt_device_set_rx_indicate(g_can_dev, app_can_rx_indicate);
    if (result != RT_EOK)
    {
        APP_CAN_LOG("CAN: set rx callback failed, ret=%d\r\n", result);
        return -RT_ERROR;
    }

    return RT_EOK;
}

int app_can_start(void)
{
    if (g_can_started)
    {
        return RT_EOK;
    }

    g_can_rx_thread = rt_thread_create(APP_CAN_RX_THREAD_NAME,
                                       app_can_rx_thread_entry,
                                       RT_NULL,
                                       APP_CAN_RX_THREAD_STACK,
                                       APP_CAN_RX_THREAD_PRIORITY,
                                       APP_CAN_RX_THREAD_TICK);
    if (g_can_rx_thread == RT_NULL)
    {
        APP_CAN_LOG("CAN: create rx thread failed\r\n");
        return -RT_ERROR;
    }

    g_can_tx_thread = rt_thread_create(APP_CAN_TX_THREAD_NAME,
                                       app_can_tx_thread_entry,
                                       RT_NULL,
                                       APP_CAN_TX_THREAD_STACK,
                                       APP_CAN_TX_THREAD_PRIORITY,
                                       APP_CAN_TX_THREAD_TICK);
    if (g_can_tx_thread == RT_NULL)
    {
        APP_CAN_LOG("CAN: create tx thread failed\r\n");
        return -RT_ERROR;
    }

    g_can_err_thread = rt_thread_create(APP_CAN_ERR_THREAD_NAME,
                                        app_can_err_thread_entry,
                                        RT_NULL,
                                        APP_CAN_ERR_THREAD_STACK,
                                        APP_CAN_ERR_THREAD_PRIORITY,
                                        APP_CAN_ERR_THREAD_TICK);
    if (g_can_err_thread == RT_NULL)
    {
        APP_CAN_LOG("CAN: create error thread failed\r\n");
        return -RT_ERROR;
    }

#if APP_CAN_TEST_TX_ENABLE
    g_can_test_tx_thread = rt_thread_create(APP_CAN_TEST_TX_THREAD_NAME,
                                            app_can_test_tx_thread_entry,
                                            RT_NULL,
                                            APP_CAN_TEST_TX_THREAD_STACK,
                                            APP_CAN_TEST_TX_THREAD_PRIORITY,
                                            APP_CAN_TEST_TX_THREAD_TICK);
    if (g_can_test_tx_thread == RT_NULL)
    {
        APP_CAN_LOG("CAN: create test tx thread failed\r\n");
        return -RT_ERROR;
    }
#endif

    rt_thread_startup(g_can_rx_thread);
    rt_thread_startup(g_can_tx_thread);
    rt_thread_startup(g_can_err_thread);
#if APP_CAN_TEST_TX_ENABLE
    rt_thread_startup(g_can_test_tx_thread);
#endif

    g_can_started = RT_TRUE;
    return RT_EOK;
}

#if 0
/*
 * The original TZ3A app_can module also contains:
 * - can2/can3 multi-channel support
 * - transceiver EN/WAKE control
 * - protocol decode tables
 * - 10ms/100ms periodic senders
 * - UDS/DM1 and other vehicle business logic
 *
 * These parts are intentionally disabled in the first migration round.
 */
#endif
