#include "app_usart_cmd.h"

#include <string.h>

#include "app_config.h"
#include "svc_lcd.h"

typedef struct
{
    uint8_t buf[APP_UART_CMD_RX_BUF_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} app_uart_cmd_ring_t;

static app_uart_cmd_ring_t g_uart_cmd_rx_ring;
static app_uart_cmd_frame_t g_uart_cmd_last_frame;
static app_uart_cmd_tx_fn_t g_uart_cmd_tx_cb = RT_NULL;
static rt_device_t g_uart_cmd_dev = RT_NULL;
static struct rt_semaphore g_uart_cmd_rx_sem;

static rt_bool_t app_uart_cmd_ring_push(app_uart_cmd_ring_t *ring, uint8_t byte)
{
    if ((ring == RT_NULL) || (ring->count >= APP_UART_CMD_RX_BUF_SIZE)) {
        return RT_FALSE;
    }

    ring->buf[ring->head] = byte;
    ring->head = (uint16_t)((ring->head + 1U) % APP_UART_CMD_RX_BUF_SIZE);
    ring->count++;
    return RT_TRUE;
}

static rt_bool_t app_uart_cmd_ring_pop(app_uart_cmd_ring_t *ring, uint8_t *byte)
{
    if ((ring == RT_NULL) || (byte == RT_NULL) || (ring->count == 0U)) {
        return RT_FALSE;
    }

    *byte = ring->buf[ring->tail];
    ring->tail = (uint16_t)((ring->tail + 1U) % APP_UART_CMD_RX_BUF_SIZE);
    ring->count--;
    return RT_TRUE;
}



static rt_bool_t app_uart_cmd_try_parse_frame(app_uart_cmd_frame_t *frame)
{
    static uint8_t collecting = 0U;
    static uint8_t temp[APP_UART_CMD_FRAME_MAX_SIZE];
    static uint8_t index = 0U;
    uint8_t byte;

    while (app_uart_cmd_ring_pop(&g_uart_cmd_rx_ring, &byte) == RT_TRUE) {
        if (collecting == 0U) {
            if (byte == APP_UART_CMD_FRAME_HEAD) {
                collecting = 1U;
                index = 0U;
                temp[index++] = byte;
            }
            continue;
        }

        if (index < APP_UART_CMD_FRAME_MAX_SIZE) {
            temp[index++] = byte;
        } else {
            collecting = 0U;
            index = 0U;
            continue;
        }

        if (byte == APP_UART_CMD_FRAME_TAIL) {
            frame->length = index;
            rt_memcpy(frame->data, temp, index);

            collecting = 0U;
            index = 0U;
            return RT_TRUE;
        }
    }

    return RT_FALSE;
}


static void app_uart_cmd_dump_frame(const uint8_t *buf, uint8_t len)
{
    uint8_t i;

    rt_kprintf("[uart_cmd][frame] len=%u data:", len);
    for (i = 0; i < len; i++) {
        rt_kprintf(" %02X", buf[i]);
    }
    rt_kprintf("\n");
}



static uint16_t app_uart_cmd_build_ack(uint8_t type, uint8_t ret, uint8_t *buf, uint16_t buf_size)
{
    if ((buf == RT_NULL) || (buf_size < 8U)) {
        return 0U;
    }

    buf[0] = APP_UART_CMD_FRAME_HEAD;
    buf[1] = 8U;
    buf[2] = type;
    buf[3] = ret;
    buf[4] = 0U;
    buf[5] = 0U;
    buf[6] = 0U;
    buf[7] = APP_UART_CMD_FRAME_TAIL;

    return 8U;
}

static void app_uart_cmd_uart_tx(const uint8_t *data, uint16_t len)
{
    if ((g_uart_cmd_dev == RT_NULL) || (data == RT_NULL) || (len == 0U)) {
        return;
    }

    rt_device_write(g_uart_cmd_dev, 0, data, len);
}

static rt_err_t app_uart_cmd_rx_indicate(rt_device_t dev, rt_size_t size)
{
    uint8_t ch;
    rt_bool_t has_data = RT_FALSE;

    RT_UNUSED(dev);

    while (size-- > 0U) {
        if (rt_device_read(g_uart_cmd_dev, 0, &ch, 1) == 1) {
            app_usart_cmd_push_byte(ch);
            has_data = RT_TRUE;

            if ((ch >= 0x20U) && (ch <= 0x7EU)) {
                rt_kprintf("[uart_cmd][raw] '%c' (0x%02X)\n", ch, ch);
            } else {
                rt_kprintf("[uart_cmd][raw] 0x%02X\n", ch);
            }
        } else {
            break;
        }
    }

    if (has_data == RT_TRUE) {
        rt_sem_release(&g_uart_cmd_rx_sem);
    }

    return RT_EOK;
}

static void app_uart_cmd_timestamp_to_hms(uint32_t timestamp,
                                          uint8_t *hour,
                                          uint8_t *minute,
                                          uint8_t *second)
{
    uint32_t local_seconds;
    uint32_t seconds_of_day;

    /* Unix timestamp -> UTC+8 */
    local_seconds = timestamp + 8U * 3600U;
    seconds_of_day = local_seconds % 86400U;

    if (hour != RT_NULL) {
        *hour = (uint8_t)(seconds_of_day / 3600U);
    }
    if (minute != RT_NULL) {
        *minute = (uint8_t)((seconds_of_day % 3600U) / 60U);
    }
    if (second != RT_NULL) {
        *second = (uint8_t)(seconds_of_day % 60U);
    }
}

static void app_usart_cmd_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1) {
        rt_sem_take(&g_uart_cmd_rx_sem, RT_WAITING_FOREVER);
        app_usart_cmd_poll();
    }
}

int app_usart_cmd_init(void)
{
    rt_err_t result;

    rt_memset(&g_uart_cmd_rx_ring, 0, sizeof(g_uart_cmd_rx_ring));
    rt_memset(&g_uart_cmd_last_frame, 0, sizeof(g_uart_cmd_last_frame));
    g_uart_cmd_tx_cb = RT_NULL;

    result = rt_sem_init(&g_uart_cmd_rx_sem, "uart_cmd", 0, RT_IPC_FLAG_FIFO);
    if (result != RT_EOK) {
        return result;
    }

    g_uart_cmd_dev = rt_device_find(APP_UART_CMD_DEV_NAME);
    if (g_uart_cmd_dev == RT_NULL) {
        return -RT_ERROR;
    }


    result = rt_device_open(g_uart_cmd_dev, RT_DEVICE_FLAG_DMA_RX | RT_DEVICE_FLAG_RDWR);
    //result = rt_device_open(g_uart_cmd_dev, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_RDWR);
    if (result != RT_EOK) {
        return result;
    }

    result = rt_device_set_rx_indicate(g_uart_cmd_dev, app_uart_cmd_rx_indicate);
    if (result != RT_EOK) {
        return result;
    }

    app_usart_cmd_set_tx_callback(app_uart_cmd_uart_tx);
    return RT_EOK;
}

int app_usart_cmd_task_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(APP_UART_CMD_TASK_NAME,
                              app_usart_cmd_thread_entry,
                              RT_NULL,
                              APP_UART_CMD_TASK_STACK_SIZE,
                              APP_UART_CMD_TASK_PRIORITY,
                              APP_UART_CMD_TASK_TICK);
    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}

void app_usart_cmd_set_tx_callback(app_uart_cmd_tx_fn_t tx_cb)
{
    g_uart_cmd_tx_cb = tx_cb;
}

void app_usart_cmd_push_byte(uint8_t byte)
{
    (void)app_uart_cmd_ring_push(&g_uart_cmd_rx_ring, byte);
}

const app_uart_cmd_frame_t *app_usart_cmd_get_last_frame(void)
{
    return &g_uart_cmd_last_frame;
}

rt_bool_t app_usart_cmd_send_ack(uint8_t type, uint8_t ret)
{
    uint8_t ack_buf[8];
    uint16_t ack_len;

    if (g_uart_cmd_tx_cb == RT_NULL) {
        return RT_FALSE;
    }

    ack_len = app_uart_cmd_build_ack(type, ret, ack_buf, sizeof(ack_buf));
    if (ack_len == 0U) {
        return RT_FALSE;
    }

    rt_kprintf("[uart_cmd][ack] %02X %02X %02X %02X %02X %02X %02X %02X\n",
               ack_buf[0], ack_buf[1], ack_buf[2], ack_buf[3],
               ack_buf[4], ack_buf[5], ack_buf[6], ack_buf[7]);

    g_uart_cmd_tx_cb(ack_buf, ack_len);
    return RT_TRUE;
}

void app_usart_cmd_handle_frame(const app_uart_cmd_frame_t *frame)
{
    uint8_t type;
    uint32_t timestamp;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    if ((frame == RT_NULL) || (frame->length < 8U)) {
        return;
    }

    type = frame->data[2];

    if (type == 0x01U) {
        /* Timestamp is located in the 4 bytes before CRC(2B) and tail(1B). */
        timestamp = ((uint32_t)frame->data[frame->length - 7U])
                  | ((uint32_t)frame->data[frame->length - 6U] << 8)
                  | ((uint32_t)frame->data[frame->length - 5U] << 16)
                  | ((uint32_t)frame->data[frame->length - 4U] << 24);

        app_uart_cmd_timestamp_to_hms(timestamp, &hour, &minute, &second);

        rt_kprintf("[uart_cmd][time] ts=0x%08X -> %02u:%02u:%02u\n",
                   timestamp, hour, minute, second);

        svc_lcd_update_home_time(hour, minute, second);
    }
}


void app_usart_cmd_poll(void)
{
    app_uart_cmd_frame_t frame;

    while (app_uart_cmd_try_parse_frame(&frame) == RT_TRUE) {
        uint8_t type;
        uint32_t timestamp;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;

        g_uart_cmd_last_frame = frame;

        app_uart_cmd_dump_frame(frame.data, frame.length);

        if (frame.length < 3U) {
            continue;
        }

        type = frame.data[2];

        if ((type == 0x01U) && (frame.length >= 8U)) {
            timestamp = ((uint32_t)frame.data[frame.length - 7U])
                      | ((uint32_t)frame.data[frame.length - 6U] << 8)
                      | ((uint32_t)frame.data[frame.length - 5U] << 16)
                      | ((uint32_t)frame.data[frame.length - 4U] << 24);

            app_uart_cmd_timestamp_to_hms(timestamp, &hour, &minute, &second);

            rt_kprintf("[uart_cmd][time] ts=0x%08X -> %02u:%02u:%02u\n",
                       timestamp, hour, minute, second);

            svc_lcd_update_home_time(hour, minute, second);
        }
    }
}


