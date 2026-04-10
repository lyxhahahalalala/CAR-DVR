#ifndef APPLICATIONS_APP_CONFIG_H_
#define APPLICATIONS_APP_CONFIG_H_

/* 线程配置 */
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

/* LED 测试参数 */
#define APP_LED_TOGGLE_PERIOD_MS     500

/* ADC 设备与通道配置 */
#define APP_ADC_DEV_NAME             "adc0"
#define APP_ADC_CH_BAT_24V           2
#define APP_ADC_CH_LI_BAT_4V2        3
#define APP_ADC_CH_SUPER_C_5V        4
#define APP_ADC_CH_KEY               11
#define APP_ADC_SAMPLE_PERIOD_MS     1000
#define APP_ADC_STARTUP_DELAY_MS     1000

/* ADC 估算电压参数
 * 当前按 16bit 满量程和 3.3V 参考电压估算，后续可根据实测再校准。
 */
#define APP_ADC_FULL_SCALE           65535UL
#define APP_ADC_VREF_MV              3300UL

/* BAT24 分压：1M / 91K，输入电压 = ADC脚电压 * (1000K + 91K) / 91K */
#define APP_ADC_BAT24_DIV_UP_KOHM    1000UL
#define APP_ADC_BAT24_DIV_DOWN_KOHM  91UL

/* LI_BAT 分压：91K / 91K，输入电压 = ADC脚电压 * 2 */
#define APP_ADC_LI_BAT_DIV_UP_KOHM   91UL
#define APP_ADC_LI_BAT_DIV_DOWN_KOHM 91UL

/* 空骨架线程打印周期 */
#define APP_POWER_TASK_PERIOD_MS     1000
#define APP_CAN_TASK_PERIOD_MS       1200
#define APP_IO_TASK_PERIOD_MS        1400
#define APP_STORAGE_TASK_PERIOD_MS   1600
#define APP_LCD_TASK_PERIOD_MS       1800

#endif /* APPLICATIONS_APP_CONFIG_H_ */