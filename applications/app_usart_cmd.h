#ifndef APPLICATIONS_APP_USART_CMD_H_
#define APPLICATIONS_APP_USART_CMD_H_

#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>

#define APP_UART_CMD_DEV_NAME            "uart3"
#define APP_UART_CMD_FRAME_HEAD          0xAAU
#define APP_UART_CMD_FRAME_TAIL          0x55U
#define APP_UART_CMD_FRAME_MAX_SIZE      64U
#define APP_UART_CMD_RX_BUF_SIZE         64U
#define APP_UART_CMD_ACK_RET_OK          1U
#define APP_UART_CMD_ACK_RET_ERR         0U

#define APP_UART_CMD_TASK_NAME           "uart_cmd"
#define APP_UART_CMD_TASK_STACK_SIZE     1024
#define APP_UART_CMD_TASK_PRIORITY       18
#define APP_UART_CMD_TASK_TICK           10

typedef struct
{
    uint8_t data[APP_UART_CMD_FRAME_MAX_SIZE];
    uint8_t length;
} app_uart_cmd_frame_t;

typedef struct
{
    uint8_t camera1_status  : 1;//摄像头1状态
    uint8_t camera2_status  : 1;//摄像头2状态
    uint8_t camera3_status  : 1;//摄像头3状态
    uint8_t camera4_status  : 1;//摄像头4状态
    uint8_t record_status   : 1;// 录音状态
    uint8_t location_status : 1;// 定位状态
    uint8_t reserved        : 2;

    uint8_t signal;// SIM卡信号
    uint8_t used_satellite;// 使用的卫星数

    uint32_t total_capacity;// 安全存储器总容量(KB)，大端序
    uint32_t used_capacity;// 安全存储器使用容量(KB)，大端序
    uint32_t timestamp;// UTc时间戳(秒)
    uint32_t driver_time;// 驾驶时间(秒)
    uint16_t driver_speed;//参考速度
    uint8_t ic_card_id_raw[9];//ic卡信息
} app_soc_status_msg_t;


typedef void (*app_uart_cmd_tx_fn_t)(const uint8_t *data, uint16_t len);

int app_usart_cmd_init(void);
int app_usart_cmd_task_start(void);

void app_usart_cmd_set_tx_callback(app_uart_cmd_tx_fn_t tx_cb);
void app_usart_cmd_push_byte(uint8_t byte);
void app_usart_cmd_poll(void);

const app_uart_cmd_frame_t *app_usart_cmd_get_last_frame(void);
rt_bool_t app_usart_cmd_send_ack(uint8_t type, uint8_t ret);

#endif /* APPLICATIONS_APP_USART_CMD_H_ */
