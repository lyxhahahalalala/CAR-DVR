#include "app_usart_cmd.h"

#include <string.h>

#include "app_config.h"

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
    static uint8_t state = 0U;
    static uint8_t temp[APP_UART_CMD_FRAME_SIZE];
    static uint8_t index = 0U;
    static uint8_t expect_len = 0U;

    uint8_t byte;

    while (app_uart_cmd_ring_pop(&g_uart_cmd_rx_ring, &byte) == RT_TRUE) {
        switch (state) {
        case 0:
            if (byte == APP_UART_CMD_FRAME_HEAD) {
                temp[0] = byte;
                index = 1U;
                state = 1U;
            }
            break;

        case 1:
            temp[index++] = byte;
            expect_len = byte;

            if (expect_len != APP_UART_CMD_FRAME_SIZE) {
                state = 0U;
                index = 0U;
            } else {
                state = 2U;
            }
            break;

        case 2:
            temp[index++] = byte;
            if (index >= expect_len) {
                if (temp[expect_len - 1U] == APP_UART_CMD_FRAME_TAIL) {
                    frame->head   = temp[0];
                    frame->length = temp[1];
                    frame->type   = temp[2];
                    frame->id     = temp[3];
                    frame->data   = temp[4];
                    frame->r1     = temp[5];
                    frame->r2     = temp[6];
                    frame->tail   = temp[7];

                    rt_kprintf("[uart_cmd][rx] %02X %02X %02X %02X %02X %02X %02X %02X\n",
                                                   temp[0], temp[1], temp[2], temp[3],
                                                   temp[4], temp[5], temp[6], temp[7]);

                    state = 0U;
                    index = 0U;
                    return RT_TRUE;
                }

                state = 0U;
                index = 0U;
            }
            break;

        default:
            state = 0U;
            index = 0U;
            break;
        }
    }

    return RT_FALSE;
}

static uint16_t app_uart_cmd_build_ack(uint8_t type, uint8_t ret, uint8_t *buf, uint16_t buf_size)
{
    if ((buf == RT_NULL) || (buf_size < APP_UART_CMD_FRAME_SIZE)) {
        return 0U;
    }

    buf[0] = APP_UART_CMD_FRAME_HEAD;
    buf[1] = APP_UART_CMD_FRAME_SIZE;
    buf[2] = type;
    buf[3] = ret;
    buf[4] = 0U;
    buf[5] = 0U;
    buf[6] = 0U;
    buf[7] = APP_UART_CMD_FRAME_TAIL;

    return APP_UART_CMD_FRAME_SIZE;
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

    RT_UNUSED(dev);

    while (size-- > 0U) {
        if (rt_device_read(g_uart_cmd_dev, 0, &ch, 1) == 1) {
            app_usart_cmd_push_byte(ch);
        } else {
            break;
        }
    }

    return RT_EOK;
}

static void app_usart_cmd_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1) {
        app_usart_cmd_poll();
        rt_thread_mdelay(10);
    }
}

int app_usart_cmd_init(void)
{
    rt_err_t result;

    rt_memset(&g_uart_cmd_rx_ring, 0, sizeof(g_uart_cmd_rx_ring));
    rt_memset(&g_uart_cmd_last_frame, 0, sizeof(g_uart_cmd_last_frame));
    g_uart_cmd_tx_cb = RT_NULL;

    g_uart_cmd_dev = rt_device_find(APP_UART_CMD_DEV_NAME);
    if (g_uart_cmd_dev == RT_NULL) {
        return -RT_ERROR;
    }

    result = rt_device_open(g_uart_cmd_dev, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_RDWR);
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
    uint8_t ack_buf[APP_UART_CMD_FRAME_SIZE];
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
    if (frame == RT_NULL) {
        return;
    }

    switch (frame->type) {
    case 209:
        /* 预留：限速参数命令 */
        app_usart_cmd_send_ack(frame->type, APP_UART_CMD_ACK_RET_OK);
        break;

    case 212:
        /* 预留：背光参数命令 */
        app_usart_cmd_send_ack(frame->type, APP_UART_CMD_ACK_RET_OK);
        break;

    case 219:
        /* 预留：语言参数命令 */
        app_usart_cmd_send_ack(frame->type, APP_UART_CMD_ACK_RET_OK);
        break;

    case 220:
        /* 预留：蜂鸣器参数命令 */
        app_usart_cmd_send_ack(frame->type, APP_UART_CMD_ACK_RET_OK);
        break;

    default:
        app_usart_cmd_send_ack(frame->type, APP_UART_CMD_ACK_RET_ERR);
        break;
    }
}

void app_usart_cmd_poll(void)
{
    app_uart_cmd_frame_t frame;

    while (app_uart_cmd_try_parse_frame(&frame) == RT_TRUE) {
        g_uart_cmd_last_frame = frame;

        rt_kprintf("[uart_cmd][frame] type=%u id=%u data=%u r1=%u r2=%u\n",
                   frame.type, frame.id, frame.data, frame.r1, frame.r2);

        app_usart_cmd_handle_frame(&frame);
    }
}
