#include <rtthread.h>
#include <rtdevice.h>                          //GLM5.1

#include "app_config.h"
#include "svc_can.h"

/* CAN 设备句柄 //GLM5.1 */
static rt_device_t g_can_dev = RT_NULL;        //GLM5.1

/* 接收帧计数器，用于调试观察 //GLM5.1 */
static rt_uint32_t g_can_rx_count = 0;         //GLM5.1

/* 发送帧计数器，用于调试观察 //GLM5.1 */
static rt_uint32_t g_can_tx_count = 0;         //GLM5.1

/*
 * CAN 接收回调函数 //GLM5.1
 * 当 CAN 设备收到帧时，RT-Thread 的 CAN 框架会调用此函数。
 * 这里只做简单打印，把收到的帧 ID、数据长度和数据内容打出来。
 * //GLM5.1
 */
static rt_err_t svc_can_rx_callback(rt_device_t dev, rt_size_t size)  //GLM5.1
{                                                                         //GLM5.1
    struct rt_can_msg rx_msg;                                             //GLM5.1
    rt_size_t recv_len;                                                   //GLM5.1
                                                                          //GLM5.1
    /* 从 CAN 设备读取一帧 //GLM5.1 */                                     //GLM5.1
    recv_len = rt_device_read(dev, 0, &rx_msg, sizeof(rx_msg));          //GLM5.1
    if (recv_len == sizeof(rx_msg))                                       //GLM5.1
    {                                                                     //GLM5.1
        g_can_rx_count++;                                                 //GLM5.1
                                                                          //GLM5.1
        rt_kprintf("[CAN_RX] #%lu ID=0x%03X IDE=%d RTR=%d LEN=%d DATA=", //GLM5.1
                   g_can_rx_count,                                        //GLM5.1
                   rx_msg.id,                                             //GLM5.1
                   rx_msg.ide,                                            //GLM5.1
                   rx_msg.rtr,                                            //GLM5.1
                   rx_msg.len);                                           //GLM5.1
                                                                          //GLM5.1
        /* 打印数据字节 //GLM5.1 */                                        //GLM5.1
        for (rt_uint8_t i = 0; i < rx_msg.len; i++)                      //GLM5.1
        {                                                                 //GLM5.1
            rt_kprintf("%02X ", rx_msg.data[i]);                          //GLM5.1
        }                                                                 //GLM5.1
        rt_kprintf("\r\n");                                               //GLM5.1
    }                                                                     //GLM5.1
                                                                          //GLM5.1
    return RT_EOK;                                                        //GLM5.1
}                                                                         //GLM5.1

/*
 * CAN 发送一帧测试数据 //GLM5.1
 * 构造一个标准数据帧，ID 为 APP_CAN_TEST_TX_ID，
 * 数据为发送计数器的递增值，方便在接收端观察是否连续。
 * //GLM5.1
 */
static rt_err_t svc_can_send_test_frame(void)   //GLM5.1
{                                                 //GLM5.1
    struct rt_can_msg tx_msg;                     //GLM5.1
                                                  //GLM5.1
    tx_msg.id  = APP_CAN_TEST_TX_ID;             //GLM5.1
    tx_msg.ide = 0;   /* 0=标准帧 //GLM5.1 */    //GLM5.1
    tx_msg.rtr = 0;   /* 0=数据帧 //GLM5.1 */    //GLM5.1
    tx_msg.len = 8;   /* 8 字节数据 //GLM5.1 */  //GLM5.1
                                                  //GLM5.1
    /* 填充测试数据：第 0 字节放计数器低 8 位，其余字节放固定模式 //GLM5.1 */
    tx_msg.data[0] = (g_can_tx_count & 0xFF);    //GLM5.1
    tx_msg.data[1] = 0xDE;                       //GLM5.1
    tx_msg.data[2] = 0xAD;                       //GLM5.1
    tx_msg.data[3] = 0xBE;                       //GLM5.1
    tx_msg.data[4] = 0xEF;                       //GLM5.1
    tx_msg.data[5] = 0xCA;                       //GLM5.1
    tx_msg.data[6] = 0xFE;                       //GLM5.1
    tx_msg.data[7] = 0x00;                       //GLM5.1
                                                  //GLM5.1
    if (rt_device_write(g_can_dev, 0, &tx_msg, sizeof(tx_msg)) != sizeof(tx_msg)) //GLM5.1
    {                                             //GLM5.1
        rt_kprintf("[CAN_TX] send failed\r\n");   //GLM5.1
        return -RT_ERROR;                         //GLM5.1
    }                                             //GLM5.1
                                                  //GLM5.1
    g_can_tx_count++;                             //GLM5.1
    rt_kprintf("[CAN_TX] #%lu ID=0x%03X sent OK\r\n", g_can_tx_count, APP_CAN_TEST_TX_ID);  //GLM5.1
    return RT_EOK;                                //GLM5.1
}                                                 //GLM5.1

static void svc_can_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        /* 后续补充 CAN 收发和协议处理逻辑。 */
        rt_kprintf("222\r\n");

        /* 周期性发送测试帧，验证 CAN 发送链路 //GLM5.1 */
        if (g_can_dev != RT_NULL)                 //GLM5.1
        {                                         //GLM5.1
            svc_can_send_test_frame();            //GLM5.1
        }                                         //GLM5.1

        rt_thread_mdelay(APP_CAN_TASK_PERIOD_MS);
    }
}

int svc_can_init(void)
{
    rt_err_t ret;                                 //GLM5.1

    /* 当前阶段先保留统一初始化入口。 */

    /* ---- 以下为 CAN 验证初始化 ---- //GLM5.1 */

    /* 1. 查找 CAN 设备 //GLM5.1 */
    g_can_dev = rt_device_find(APP_CAN_DEV_NAME);  //GLM5.1
    if (g_can_dev == RT_NULL)                     //GLM5.1
    {                                             //GLM5.1
        rt_kprintf("[CAN] device %s not found!\r\n", APP_CAN_DEV_NAME);  //GLM5.1
        return -RT_ERROR;                         //GLM5.1
    }                                             //GLM5.1

    /* 2. 以读写方式打开 CAN 设备 //GLM5.1 */
    ret = rt_device_open(g_can_dev, RT_DEVICE_OFLAG_RDWR);  //GLM5.1
    if (ret != RT_EOK)                           //GLM5.1
    {                                             //GLM5.1
        rt_kprintf("[CAN] open failed, ret=%d\r\n", ret);  //GLM5.1
        g_can_dev = RT_NULL;                     //GLM5.1
        return -RT_ERROR;                         //GLM5.1
    }                                             //GLM5.1

    /* 3. 设置 CAN 波特率 //GLM5.1 */
    ret = rt_device_control(g_can_dev, RT_CAN_CMD_SET_BAUD, (void *)APP_CAN_BAUDRATE);  //GLM5.1
    if (ret != RT_EOK)                           //GLM5.1
    {                                             //GLM5.1
        rt_kprintf("[CAN] set baudrate failed, ret=%d\r\n", ret);  //GLM5.1
        rt_device_close(g_can_dev);               //GLM5.1
        g_can_dev = RT_NULL;                     //GLM5.1
        return -RT_ERROR;                         //GLM5.1
    }                                             //GLM5.1

    /* 4. 注册接收回调，收到帧时自动触发 //GLM5.1 */
    ret = rt_device_set_rx_indicate(g_can_dev, svc_can_rx_callback);  //GLM5.1
    if (ret != RT_EOK)                           //GLM5.1
    {                                             //GLM5.1
        rt_kprintf("[CAN] set rx indicate failed\r\n");  //GLM5.1
        rt_device_close(g_can_dev);               //GLM5.1
        g_can_dev = RT_NULL;                     //GLM5.1
        return -RT_ERROR;                         //GLM5.1
    }                                             //GLM5.1

    rt_kprintf("[CAN] init OK, dev=%s baud=500kbps\r\n", APP_CAN_DEV_NAME);  //GLM5.1

    return RT_EOK;
}

int svc_can_task_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(APP_CAN_TASK_NAME,
                              svc_can_thread_entry,
                              RT_NULL,
                              APP_CAN_TASK_STACK_SIZE,
                              APP_CAN_TASK_PRIORITY,
                              APP_CAN_TASK_TICK);
    if (thread == RT_NULL)
    {
        rt_kprintf("can thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}
