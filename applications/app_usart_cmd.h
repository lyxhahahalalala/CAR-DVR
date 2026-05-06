#ifndef APPLICATIONS_APP_USART_CMD_H_
#define APPLICATIONS_APP_USART_CMD_H_

#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>

#if defined(__GNUC__)
#define APP_PACKED_STRUCT __attribute__((packed))
#else
#define APP_PACKED_STRUCT
#endif

#define APP_UART_CMD_DEV_NAME            "uart3"
#define APP_UART_CMD_FRAME_HEAD          0xAAU
#define APP_UART_CMD_FRAME_TAIL          0x55U
#define APP_UART_CMD_FRAME_MAX_SIZE      128U
#define APP_UART_CMD_RX_BUF_SIZE         256U
#define APP_UART_CMD_ACK_RET_OK          1U
#define APP_UART_CMD_ACK_RET_ERR         0U
#define APP_UART_CMD_TYPE_MCU_VERSION    0x11U



#define APP_UART_CMD_TASK_NAME           "uart_cmd"
#define APP_UART_CMD_TASK_STACK_SIZE     1024
#define APP_UART_CMD_TASK_PRIORITY       18
#define APP_UART_CMD_TASK_TICK           10

#define APP_UART_CMD_VERSION_TX_THREAD_NAME      "uart_ver"
#define APP_UART_CMD_VERSION_TX_STACK_SIZE       1024
#define APP_UART_CMD_VERSION_TX_PRIORITY         19
#define APP_UART_CMD_VERSION_TX_TICK             10
#define APP_UART_CMD_VERSION_TX_PERIOD_MS        1000U

#define APP_UART_CMD_TYPE_SOC_STATUS     0x00U
#define APP_UART_CMD_TYPE_MCU_VERSION    0x11U


typedef struct
{
    uint8_t data[APP_UART_CMD_FRAME_MAX_SIZE];
    uint8_t length;
} app_uart_cmd_frame_t;

typedef struct
{
    uint8_t camera1_status  : 1;// 摄像头1状态
    uint8_t camera2_status  : 1;// 摄像头2状态
    uint8_t camera3_status  : 1;// 摄像头3状态
    uint8_t camera4_status  : 1;// 摄像头4状态
    uint8_t record_status   : 1;// 录音状态
    uint8_t location_status : 1;// 定位状态
    uint8_t ic_card_status  : 1;// IC卡状态
    uint8_t udisk_status    : 1;// U盘状态

    uint8_t ip1_connected   : 1;// IP1连接状态
    uint8_t ip2_connected   : 1;// IP2连接状态
    uint8_t gsm_connected   : 1;// GSM连接状态
    uint8_t protect_storage : 1;// 防护存储器状态
    uint8_t sdcard_status   : 1;
    uint8_t sim_status      : 1;
    uint8_t reserved        : 2;

    uint8_t driver_number[9];// 驾驶员证号，BCD码
    uint32_t total_capacity;// 安全存储器总容量(KB)
    uint32_t free_capacity;
    uint32_t driver_time;// 驾驶时间(秒)
    uint8_t sim_signal; // SIM卡信号
    uint8_t phone_number[10];// 手机号，BCD码
    uint8_t used_satellite;// 使用的卫星数
    uint32_t timestamp;// UTC时间戳(秒)
    uint16_t driver_speed;// 速度，0.1km/h
    uint8_t terminal_id[30];// 终端ID
    uint32_t ip1;// 第1服务器地址
    uint32_t ip2;// 第2服务器地址
} app_soc_status_msg_t;



typedef void (*app_uart_cmd_tx_fn_t)(const uint8_t *data, uint16_t len);

int app_usart_cmd_init(void);
int app_usart_cmd_task_start(void);

void app_usart_cmd_set_tx_callback(app_uart_cmd_tx_fn_t tx_cb);
void app_usart_cmd_push_byte(uint8_t byte);
void app_usart_cmd_poll(void);

const app_uart_cmd_frame_t *app_usart_cmd_get_last_frame(void);
rt_bool_t app_usart_cmd_send_ack(uint8_t type, uint8_t ret);
rt_bool_t app_usart_cmd_send_mcu_version(void);


#endif /* APPLICATIONS_APP_USART_CMD_H_ */
