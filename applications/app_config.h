#ifndef APPLICATIONS_APP_CONFIG_H_
#define APPLICATIONS_APP_CONFIG_H_

#define APP_LED_TASK_NAME            "led_th"
#define APP_LED_TASK_STACK_SIZE      2048
#define APP_LED_TASK_PRIORITY        10
#define APP_LED_TASK_TICK            10

#define APP_ADC_TASK_NAME            "adc_th"
#define APP_ADC_TASK_STACK_SIZE      4096
#define APP_ADC_TASK_PRIORITY        11
#define APP_ADC_TASK_TICK            10

#define APP_POWER_TASK_NAME          "power_th"
#define APP_POWER_TASK_STACK_SIZE    2048
#define APP_POWER_TASK_PRIORITY      8
#define APP_POWER_TASK_TICK          10

#define APP_CAN_TASK_NAME            "can_th"
#define APP_CAN_TASK_STACK_SIZE      2048
#define APP_CAN_TASK_PRIORITY        9
#define APP_CAN_TASK_TICK            10

/* CAN 配置 */
#define APP_CAN_DEV_NAME             BOARD_CAN_NAME
#define APP_CAN_BAUDRATE             CAN250kBaud
#define APP_CAN_RX_THREAD_NAME       "can_rx"
#define APP_CAN_RX_THREAD_STACK      3072
#define APP_CAN_RX_THREAD_PRIORITY   18
#define APP_CAN_RX_THREAD_TICK       20
#define APP_CAN_TX_THREAD_NAME       "can_tx"
#define APP_CAN_TX_THREAD_STACK      2048
#define APP_CAN_TX_THREAD_PRIORITY   19
#define APP_CAN_TX_THREAD_TICK       20
#define APP_CAN_ERR_THREAD_NAME      "can_err"
#define APP_CAN_ERR_THREAD_STACK     2048
#define APP_CAN_ERR_THREAD_PRIORITY  20
#define APP_CAN_ERR_THREAD_TICK      20
#define APP_CAN_TX_MQ_NAME           "can_txq"
#define APP_CAN_TX_MQ_DEPTH          16
#define APP_CAN_TEST_TX_ENABLE        0
#define APP_CAN_TEST_TX_THREAD_NAME   "can_ttx"
#define APP_CAN_TEST_TX_THREAD_STACK  2048
#define APP_CAN_TEST_TX_THREAD_PRIORITY 21
#define APP_CAN_TEST_TX_THREAD_TICK   20
#define APP_CAN_TEST_TX_PERIOD_MS     1000
#define APP_CAN_TEST_TX_ID            0x123UL

/* 日志开关 */
#define APP_DEBUG_CAN_ONLY           0
#define APP_SHUTDOWN_TRACE_ONLY      0
#define APP_CAN_LOG_ENABLE           0

#if APP_DEBUG_CAN_ONLY
#define APP_NON_CAN_LOG(...)         ((void)0)
#elif APP_SHUTDOWN_TRACE_ONLY
#define APP_NON_CAN_LOG(...)         ((void)0)
#else
#define APP_NON_CAN_LOG(...)         rt_kprintf(__VA_ARGS__)
#endif

#define APP_IO_TASK_NAME             "vehio_th"
#define APP_IO_TASK_STACK_SIZE       2048
#define APP_IO_TASK_PRIORITY         12
#define APP_IO_TASK_TICK             10

#define APP_STORAGE_TASK_NAME        "store_th"
#define APP_STORAGE_TASK_STACK_SIZE  2048
#define APP_STORAGE_TASK_PRIORITY    13
#define APP_STORAGE_TASK_TICK        10

#define APP_LCD_TASK_NAME            "lcd_th"

#define APP_LCD_TASK_STACK_SIZE      2048
#define APP_LCD_TASK_PRIORITY        14
#define APP_LCD_TASK_TICK            10

#define APP_LED_TOGGLE_PERIOD_MS     500

/* ADC 配置 */
#define APP_ADC_DEV_NAME             "adc0"
#define APP_ADC_CH_BAT_24V           2
#define APP_ADC_CH_LI_BAT_4V2        3
#define APP_ADC_CH_SUPER_C_5V        4
#define APP_ADC_CH_KEY               11
#define APP_ADC_SAMPLE_PERIOD_MS     1000
#define APP_ADC_STARTUP_DELAY_MS     1000

#define APP_ADC_FULL_SCALE           65535UL
#define APP_ADC_VREF_MV              3300UL

#define APP_ADC_BAT24_DIV_UP_KOHM    1000UL
#define APP_ADC_BAT24_DIV_DOWN_KOHM  91UL

#define APP_ADC_LI_BAT_DIV_UP_KOHM   91UL
#define APP_ADC_LI_BAT_DIV_DOWN_KOHM 91UL

/* 电源管理阈值与时序配置 */
#define APP_PWR_MAIN_PRESENT_THRESHOLD_MV          18000UL

#define APP_PWR_SUPERCAP_READY_THRESHOLD_MV        2100UL

#define APP_PWR_SUPERCAP_HOLD_THRESHOLD_MV         2100UL

#define APP_PWR_SUPERCAP_SHUTDOWN_THRESHOLD_MV     1800UL

#define APP_PWR_HOLD_PREPARE_TIMEOUT_MS            10000UL

#define APP_PWR_SHUTDOWN_IN_PROGRESS_MS            3000UL

#define APP_PWR_RECOVERY_RESET_DELAY_MS            50UL

#define APP_PWR_MAIN_PRESENT_CONFIRM_MS            1000UL

#define APP_PWR_MAIN_LOSS_CONFIRM_MS               1000UL

#define APP_PWR_SUPERCAP_READY_CONFIRM_MS          3000UL

#define APP_PWR_SUPERCAP_LOW_CONFIRM_MS            2000UL

#define APP_PWR_SUPERCAP_MGMT_ENABLE               0

#define APP_PWR_FINAL_SOC_CUT_DELAY_MS             1000UL

#define APP_PWR_FINAL_HOLD_CUT_DELAY_MS            1500UL

#define APP_PWR_SOC_ON_DELAY_MS                      5000UL

/* 周期任务节拍 */
#define APP_POWER_TASK_PERIOD_MS     100
#define APP_CAN_TASK_PERIOD_MS       1200
#define APP_IO_TASK_PERIOD_MS        1400
#define APP_STORAGE_TASK_PERIOD_MS   1600
#define APP_LCD_TASK_PERIOD_MS       1800

#if APP_CAN_LOG_ENABLE
#define APP_CAN_LOG(...)             rt_kprintf(__VA_ARGS__)
#else
#define APP_CAN_LOG(...)             ((void)0)
#endif

#endif 
