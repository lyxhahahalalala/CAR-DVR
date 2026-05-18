#ifndef APPLICATIONS_APP_USART_CMD_H_
#define APPLICATIONS_APP_USART_CMD_H_

#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>

/*
 * ============================================================
 *  串口命令协议 (USART Command Protocol)
 * ============================================================
 *
 * 功能:
 *   通过 UART3 与外部 SOC (System on Chip, 通常是 Android 主板)
 *   进行通信, 接收来自 SOC 的命令/数据, 并上报 MCU 状态。
 *
 * 协议格式 (定界帧):
 *   Head(0xAA) + Length + Type + Payload + CRC16(L+H) + Tail(0x55)
 *
 *   CRC16 使用查表法, 覆盖从 Length 到 Payload 的所有字节
 *   (不包含 Head 和 Tail, 因为它们是帧定界符)
 *
 * 为什么使用 0xAA/0x55 作为帧定界符?
 *   0xAA = 10101010, 0x55 = 01010101
 *   这两种比特模式交替, 在串行通信中易于识别帧边界
 *
 * 命令类型:
 *   0x01: SOC 状态上报
 *   0x02: 文本消息下发 (调度信息等)
 *   0x11: MCU 版本回复
 */

#if defined(__GNUC__)
#define APP_PACKED_STRUCT __attribute__((packed))
#else
#define APP_PACKED_STRUCT
#endif

/* ---- 协议参数 ---- */
#define APP_UART_CMD_DEV_NAME            "uart3"          /* 使用的串口设备名 */
#define APP_UART_CMD_FRAME_HEAD          0xAAU            /* 帧头 */
#define APP_UART_CMD_FRAME_TAIL          0x55U            /* 帧尾 */
#define APP_UART_CMD_FRAME_MAX_SIZE      256U             /* 单帧最大长度 */
#define APP_UART_CMD_RX_BUF_SIZE         4096U            /* 接收环形缓冲区大小 (4KB) */

/* ---- 文本消息参数 ---- */
#define APP_UART_CMD_TEXT_MAX_SIZE       4096U            /* 文本消息最大长度 */
#define APP_UART_CMD_TEXT_FRAME_TYPE     0x02U            /* 文本消息帧类型 */
#define APP_UART_CMD_TEXT_BUF_SIZE       (APP_UART_CMD_TEXT_MAX_SIZE + 1U)  /* 缓冲区 (+1 给 '\0') */

/* ---- 应答返回值 ---- */
#define APP_UART_CMD_ACK_RET_OK          1U               /* 应答: 成功 */
#define APP_UART_CMD_ACK_RET_ERR         0U               /* 应答: 失败 */




/* ---- 接收任务配置 ---- */
#define APP_UART_CMD_TASK_NAME           "uart_cmd"
#define APP_UART_CMD_TASK_STACK_SIZE     1024
#define APP_UART_CMD_TASK_PRIORITY       18               /* 较低优先级 */
#define APP_UART_CMD_TASK_TICK           10

/* ---- 版本发送任务配置 ---- */
#define APP_UART_CMD_VERSION_TX_THREAD_NAME      "uart_ver"
#define APP_UART_CMD_VERSION_TX_STACK_SIZE       1024
#define APP_UART_CMD_VERSION_TX_PRIORITY         19       /* 最低优先级 */
#define APP_UART_CMD_VERSION_TX_TICK             10
#define APP_UART_CMD_VERSION_TX_PERIOD_MS        200U     /* 每 200ms 发送一次 */

/* ---- 消息类型 ---- */
#define APP_UART_CMD_TYPE_SOC_STATUS     0x01U            /* SOC 状态上报 */
#define APP_UART_CMD_TYPE_MCU_VERSION    0x11U            /* MCU 版本回复 */


/* 帧结构体 (接收到的完整一帧) */
typedef struct
{
    uint8_t data[APP_UART_CMD_FRAME_MAX_SIZE];  /* 帧数据 */
    uint8_t length;                              /* 帧长度 */
} app_uart_cmd_frame_t;

/*
 * SOC 状态消息结构体
 *
 * 这个结构体包含了 SOC (Android 主板) 上报给 MCU 的所有状态信息:
 *   摄像头状态、记录状态、定位信息、驾驶员信息、存储容量等。
 *
 * 注意: 这是一个 packed (紧凑排列) 结构体,
 *       确保与 SOC 端的二进制协议完全一致, 没有字节对齐填充。
 */
typedef struct
{
    /* 第 1 字节: 外设状态位 */
    uint8_t camera1_status   : 1;   // 摄像头1状态
    uint8_t camera2_status   : 1;   // 摄像头2状态
    uint8_t camera3_status   : 1;   // 摄像头3状态
    uint8_t camera4_status   : 1;   // 摄像头4状态
    uint8_t record_status    : 1;   // 录音状态
    uint8_t location_status  : 1;   // 定位状态
    uint8_t ic_card_status   : 1;   // IC卡状态
    uint8_t udisk_status     : 1;   // U盘状态

    /* 第 2 字节: 连接状态位 */
    uint8_t ip1_connected       : 1;// IP1连接状态
    uint8_t ip2_connected       : 1;// IP2连接状态
    uint8_t gsm_connected       : 1;// GSM连接状态
    uint8_t protect_storage     : 1;// 防护存储器状态
    uint8_t sdcard_status       : 1;// SD卡状态
    uint8_t sim_status          : 1;// SIM卡状态
    uint8_t latitude_direction  : 1;// 0: 北纬; 1: 南纬
    uint8_t longitude_direction : 1;// 0: 东经; 1: 西经

    uint8_t driver_number[9];           // 驾驶员证号, BCD码 (18 位数字)
    uint32_t total_capacity;             // 安全存储器总容量(KB)
    uint32_t free_capacity;              // 安全存储器剩余容量(KB)
    uint32_t driver_time;                // 驾驶时间(秒)
    uint8_t sim_signal;                  // SIM卡信号强度
    uint8_t phone_number[10];            // 手机号, BCD码
    uint8_t used_satellite;              // 使用的卫星数
    uint32_t latitude;                   // 纬度 (微度)
    uint32_t longitude;                  // 经度 (微度)
    uint32_t timestamp;                  // UTC时间戳(秒)
    uint16_t driver_speed;               // 速度, 0.1km/h
    uint8_t terminal_id[30];             // 终端ID
    uint32_t ip1;                        // 第1服务器地址
    uint32_t ip2;                        // 第2服务器地址
} app_soc_status_msg_t;


/* 文本消息结构体 (从 SOC 下发的调度信息/通知) */
typedef struct
{
    uint8_t valid;                              /* 有效标志 */
    uint8_t flag;                               /* 消息标志 */
    uint8_t text_type;                          /* 消息类型 */
    uint16_t text_len;                          /* 消息长度 */
    char text[APP_UART_CMD_TEXT_BUF_SIZE];      /* UTF-8 文本内容 */
} app_uart_cmd_text_msg_t;




/* 发送回调函数类型 (由底层串口驱动注册) */
typedef void (*app_uart_cmd_tx_fn_t)(const uint8_t *data, uint16_t len);

/* ========== 初始化与任务 ========== */
int app_usart_cmd_init(void);
int app_usart_cmd_task_start(void);

/* ========== 发送/接收接口 ========== */
void app_usart_cmd_set_tx_callback(app_uart_cmd_tx_fn_t tx_cb); /* 注册发送回调 */
void app_usart_cmd_push_byte(uint8_t byte);                       /* 推送一个接收字节 */
void app_usart_cmd_poll(void);                                    /* 轮询接收缓冲区 */

/* ========== 数据获取接口 ========== */
const app_uart_cmd_frame_t *app_usart_cmd_get_last_frame(void);   /* 获取最后一帧 */
rt_bool_t app_usart_cmd_send_ack(uint8_t type, uint8_t ret);     /* 发送应答 */
rt_bool_t app_usart_cmd_send_mcu_version(void);                   /* 发送 MCU 版本 */
rt_bool_t app_usart_cmd_get_text_msg(app_uart_cmd_text_msg_t *msg); /* 获取文本消息 */

#endif /* APPLICATIONS_APP_USART_CMD_H_ */
