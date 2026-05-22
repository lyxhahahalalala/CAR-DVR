/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-04-14     Jasmine       the first version
 */

//typedef enum
//{
//    ADC_KEY_NONE  = 0,
//    ADC_KEY_S1,
//    ADC_KEY_S2,
//    ADC_KEY_S3,
//    ADC_KEY_S4,
//}adc_key_t;
//
//static adc_key_t g_adc_key_last = ADC_KEY_NONE;
//static adc_key_t g_adc_key_candidate = ADC_KEY_NONE;
//static uint8_t g_adc_key_confirm = 0;
//
//static rt_bool_t g_adc_s1_event = RT_FALSE;
//static rt_bool_t g_adc_s2_event = RT_FALSE;
//static rt_bool_t g_adc_s3_event = RT_FALSE;
//static rt_bool_t g_adc_s4_event = RT_FALSE;
//
//
//int main(void)
//{
//
//    app_framework_init();
//    app_task_start();
//    return 0;
//
//}
//
//int app_framework_init(void)
//{
//    app_init_led_pins();
//    svc_led_init();
//    svc_adc_init();
//    svc_lcd_init();
//    svc_power_init();
//    svc_can_init();
//    svc_vehicle_io_init();
//    svc_storage_init();
//
//    if(app_usart_cmd_init()!=RT_EOK)
//    {
//      return -RT_ERROR;
//    }
//    return RT_OK;
//
//}
//
//int app_task_start(void)
//{
//    int result;
//
//    result = svc_led_task_start();
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//    result = svc_power_task_start();
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//    result = svc_can_task_start();
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//    result = svc_lcd_task_start();
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//    result = svc_vehicle_io_task_start();
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//    result = svc_adc_task_start();
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//    result = svc_storage_task_start();
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//    result = svc_usart_cmd_task_start();
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//        return RT_EOK;
//}
//
//static void svc_led_thread_entry(void)
//{
//
//        RT_UNUSED(arg);
//        while(1)
//        {
//            app_led_write(APP_LED0,APP_LED_ON);
//            rt_thread_mdelay(APP_LED_TOGGLE_PERIOD_MS);
//            app_led_write(APP_LED0,APP_LED_OFF);
//            rt_thread_mdelay(APP_LED_TOGGLE_PERIOD_MS);
//
//        }
//}
//
//int svc_led_init(void)
//{
//    return RT_EOK;
//}
//
//
//int svc_led_task_start(void)
//{
//    rt_thread_t led_thread;
//
//    led_thread = rt_thread_create(APP_LED_TASK_NAME,
//                                  svc_led_thread_entry,
//                                  RT_NULL,
//                                  APP_LED_TASK_STACK_SIZE,
//                                  APP_LED_TASK_PRIORITY,
//                                  APP_LED_TASK_TICK);
//    if(led_thread == RT_NULL)
//    {
//        rt_printf("led thread create failed\r\n");
//        return -RT_ERROR;
//    }
//
//    rt_thread_startup(led_thread);
//    return RT_EOK;
//
//}
//
//int svc_adc_init(void)
//{
//    rt_memset(&g_adc_snapshot,0,sizeof(g_adc_snapshot));
//    g_adc_key_last = ADC_KEY_NONE;
//    g_adc_key_candidate = ADC_KEY_NONE;
//    g_adc_key_confirm = 0U;
//    g_adc_s1_event = RT_FALSE;
//    g_adc_s2_event = RT_FALSE;
//    g_adc_s3_event = RT_FALSE;
//    g_adc_s4_event = RT_FALSE;
//
//    return RT_EOK;
//
//}
//
//int svc_adc_task_start(void)
//{
//    rt_thread_t adc_thread;
//    adc_thread = rt_thread_create(APP_ADC_TASK_NAME,
//                                  svc_adc_thread_entry,
//                                  RT_NULL,
//                                  APP_ADC_TASK_STACK_SIZE,
//                                  APP_ADC_TASK_PRIORITY, APP_ADC_TASK_TICK);
//
//    if(adc_thread == RT_NULL)
//    {
//        rt_printf("adc thread create failed\r\n");
//        return -RT_ERROR;
//
//    }
//    rt_thread_startup(adc_thread);
//    return RT_EOK;
//}
//
//static void svc_adc_thread_entry(void *arg)
//{
//    rt_adc_device_t adc_dev;
//
//    RT_UNUSED(arg);
//
//    rt_thread_mdelay(APP_ADC_STARTUP_DELAY_MS);
//    adc_dev = (rt_adc_device_t)rt_device_find(APP_ADC_DEV_NAME);
//    if(adc_dev == RT_NULL)
//    {
//        rt_printf("adc0 not found\r\n");
//        return;
//    }
//    if(svc_adc_channel_enable(adc_dev)!=RT_EOK)
//    {
//        return;
//    }
//    while(1)
//    {
//        svc_adc_sample_update(adc_dev);
//        rt_thread_mdelay(APP_ADC_SAMPLE_PERIOD_MS);
//    }
//
//}
//
//
//static int svc_adc_channel_enable(rt_adc_device_t adc_dev)
//{
//    if (rt_adc_enable(adc_dev, APP_ADC_CH_BAT_24V) != RT_EOK)
//    {
//        return -RT_ERROR;
//    }
//
//    if (rt_adc_enable(adc_dev, APP_ADC_CH_LI_BAT_4V2) != RT_EOK)
//    {
//        return -RT_ERROR;
//    }
//
//    if (rt_adc_enable(adc_dev, APP_ADC_CH_SUPER_C_5V) != RT_EOK)
//    {
//        return -RT_ERROR;
//    }
//
//    if (rt_adc_enable(adc_dev, APP_ADC_CH_KEY) != RT_EOK)
//    {
//        return -RT_ERROR;
//    }
//
//    return RT_EOK;
//}
//
//
//static app_adc_snapshot_t g_adc_snapshot;
//
//static void svc_adc_sample_update(rt_adc_device_t adc_dev)
//{
//    g_adc_snapshot.raw_bat24 = rt_adc_read(adc_dev, APP_ADC_CH_BAT_24V);
//    g_adc_snapshot.raw_li_bat = rt_adc_read(adc_dev, APP_ADC_CH_LI_BAT_4V2);
//    g_adc_snapshot.raw_super_c = rt_adc_read(adc_dev, APP_ADC_CH_SUPER_C_5V);
//    g_adc_snapshot.raw_key = rt_adc_read(adc_dev, APP_ADC_CH_KEY);
//
//    g_adc_snapshot.mv_bat24 = rt_adc_voltage(adc_dev, APP_ADC_CH_BAT_24V);
//    g_adc_snapshot.mv_li_bat = rt_adc_voltage(adc_dev, APP_ADC_CH_LI_BAT_4V2);
//    g_adc_snapshot.mv_super_c = rt_adc_voltage(adc_dev, APP_ADC_CH_SUPER_C_5V);
//    g_adc_snapshot.mv_key = rt_adc_voltage(adc_dev, APP_ADC_CH_KEY);
//}
//
//app_usart_cmd_init(void)
//{
//    rt_err_t result;
//
//    rt_memset(&g_uart_cmd_rx_ring,0,sizeof(g_uart_cmd_rx_ring));
//    rt_memset(&g_uart_cmd_last_frame,0,sizeof(g_uart_cmd_last_frame));
//    g_uart_cmd_tx_cb = RT_NULL;
//
//    rt_memset(&g_uart_cmd_text_msg,0,sizeof(g_uart_cmd_text_msg));
//    g_uart_cmd_text_assem_len = 0U;
//
//    result = rt_sem_init(&g_uart_cmd_rx_sem,"uart_cmd",0,RT_IPC_FLAG_FIFO);
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//
//    g_uart_cmd_dev = rt_device_find(APP_UART_CMD_DEV_NAME);
//    if(g_uart_cmd_dev == RT_NULL)
//    {
//        return -RT_ERROR;
//    }
//
//    result = rt_device_open(g_uart_cmd_dev,RT_DEVICE_FLAG_DMA_RX | RT_DEVICE_FLAG_RDWR);
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//
//    result = rt_device_set_rx_indicate(g_uart_cmd_dev,app_uart_cmd_rx_indicate);
//    if(result != RT_EOK)
//    {
//        return result;
//    }
//
//    app_usart_cmd_set_tx_callback(app_uart_cmd_uart_tx);
//    return RT_EOK;
//
//}
//
//static void app_usart_cmd_thread_entry(void *parameter)
//{
//    RT_UNUSED(parameter);
//
//    while(1)
//    {
//        rt_sem_take(&g_uart_cmd_rx_sem,RT_WAITING_FOREVER);
//        app_usart_cmd_poll();
//    }
//}
//
//int app_usart_cmd_task_start(void)
//{
//    rt_thread_t thread;
//
//    thread = rt_thread_create(APP_UART_CMD_TASK_NAME,
//                              app_usart_cmd_thread_entry,
//                              RT_NULL,
//                              APP_UART_CMD_TASK_STACK_SIZE,
//                              APP_UART_CMD_TASK_PRIORITY,
//                              APP_UART_CMD_TASK_TICK);
//
//    if(thread == RT_NULL)
//    {
//        return -RT_ERROR;
//    }
//
//    rt_thread_startup(thread);
//    return RT_EOK;
//
//
//
//
//}
//
//static rt_bool_t app_uart_cmd_ring_push(app_uart_cmd_ring_t *ring,uint8_t byte)
//{
//    if((ring == RT_NULL) || (ring->count >= APP_UART_CMD_RX_BUF_SIZE))
//    {
//        return RT_FALSE;
//    }
//
//    ring->buf[ring->head] = byte;
//    ring->head = (uint16_t)((ring->head + 1U) % APP_UART_CMD_RX_BUF_SIZE);
//    ring->count++;
//
//    return RT_TRUE;
//}
//
//void app_usart_cmd_push_byte(uint8_t byte)
//{
//    if(app_uart_cmd_ring_push(&g_uart_cmd_rx_ring,byte) != RT_TRUE)
//    {
//        rt_printf("[UART_CMD][RING oberflow] drop=0x%02X count=%u\n",byte,g_uart_cmd_rx_ring.count);
//    }
//}
//
//static void app_usart_cmd_push_bytes(const uint8_t *data,uint16_t len)
//{
//    uint16_t i;
//
//    if((data == RT_NULL)||(len == 0u))
//    {
//        return;
//    }
//
//    for(i=0u;i<len;i++)
//    {
//        app_usart_cmd_push_byte(data[i]);
//    }
//
//}
//
//
//static rt_err_t app_uart_cmd_rx_indicate(rt_device_t dev,rt_size_t size)
//{
//    uint8_t rx_buf[128];
//    rt_size_t read_len;
//    rt_bool_t has_data = RT_FALSE;
//
//    RT_UNUSED(dev);
//
//    while(size > 0u)
//    {
//        read_len = size;
//        if(read_len > sizeof(rx_buf))
//        {
//            read_len = sizeof(rx_buf);
//        }
//
//        read_len = rt_device_read(g_uart_cmd_dev , 0,rx_buf,read_len);
//        if(read_len == 0u){
//            break;
//        }
//
//        app_usart_cmd_push_bytes(rx_buf,(uint16_t)read_len);
//        has_data = RT_TRUE;
//
//        if(size >=read_len)
//        {
//            size -= read_len;
//        }else{
//            size = 0u;
//        }
//    }
//
//    if(has_data == RT_TRUE)
//    {
//        rt_sem_release(&g_uart_cmd_rx_sem);
//    }
//
//    return RT_EOK;
//
//}
//
//static rt_bool_t app_uart_cmd_ring_pop(app_uart_cmd_ring_t *ring,uint8_t *byte)
//{
//    if((ring == RT_NULL) || (byte == RT_NULL) || (ring->count == 0u))
//    {
//            return RT_FALSE;
//    }
//
//    *byte = ring->buf[ring->tail];
//    ring->tail = (uint16_t)((ring->tail + 1U)% APP_UART_CMD_RX_BUF_SIZE);
//    ring->count--;
//
//    return RT_TRUE;
//}
//
//static rt_bool_t app_uart_cmd_try_parse_frame(app_uart_cmd_frame_t *frame)
//{
//    static uint8_t temp[APP_UART_CMD_FRAME_MAX_SIZE];
//    static uint8_t index = 0;
//    static uint8_t expect_len = 0u;
//    uint8_t byte;
//    uint8_t i;
//    uint8_t next_head;
//    uint8_t remain_len;
//
//    if(frame == RT_NULL)
//    {
//        return RT_FALSE;
//
//    }
//
//    while(app_uart_cmd_ring_pop(&g_uart_cmd_rx_ring,&byte) == RT_TRUE)
//    {
//        if(index == 0U){
//            if(byte != APP_UART_CMD_FRAME_HEAD)
//            {
//                continue;
//            }
//
//            temp[0] = byte;
//            index = 1U;
//            expect_len = 0U;
//            continue;
//            }
//
//
//    }
//
//    return RT_FALSE;
//
//
//
//}

