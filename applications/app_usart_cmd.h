#ifndef APPLICATIONS_APP_USART_CMD_H_
#define APPLICATIONS_APP_USART_CMD_H_

#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>

#define APP_UART_CMD_DEV_NAME            "uart0"
#define APP_UART_CMD_FRAME_HEAD          0xAAU
#define APP_UART_CMD_FRAME_TAIL          0x55U
#define APP_UART_CMD_FRAME_SIZE          8U
#define APP_UART_CMD_RX_BUF_SIZE         64U
#define APP_UART_CMD_ACK_RET_OK          1U
#define APP_UART_CMD_ACK_RET_ERR         0U

#define APP_UART_CMD_TASK_NAME           "uart_cmd"
#define APP_UART_CMD_TASK_STACK_SIZE     1024
#define APP_UART_CMD_TASK_PRIORITY       18
#define APP_UART_CMD_TASK_TICK           10

typedef struct
{
    uint8_t head;
    uint8_t length;
    uint8_t type;
    uint8_t id;
    uint8_t data;
    uint8_t r1;
    uint8_t r2;
    uint8_t tail;
} app_uart_cmd_frame_t;

typedef void (*app_uart_cmd_tx_fn_t)(const uint8_t *data, uint16_t len);

int app_usart_cmd_init(void);
int app_usart_cmd_task_start(void);

void app_usart_cmd_set_tx_callback(app_uart_cmd_tx_fn_t tx_cb);
void app_usart_cmd_push_byte(uint8_t byte);
void app_usart_cmd_poll(void);

const app_uart_cmd_frame_t *app_usart_cmd_get_last_frame(void);
rt_bool_t app_usart_cmd_send_ack(uint8_t type, uint8_t ret);

/* 业务层命令分发入口 */
void app_usart_cmd_handle_frame(const app_uart_cmd_frame_t *frame);

#endif /* APPLICATIONS_APP_USART_CMD_H_ */
