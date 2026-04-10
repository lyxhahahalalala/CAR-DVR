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

/* 电源管理参数。
 * 这一组参数全部服务于 svc_power 状态机，
 * 目的是把“主电是否存在、超级电容是否可用、什么时候进入关机准备、什么时候执行最终断电”
 * 这些策略统一收口到配置文件中，避免后续到业务代码里到处找魔法数字。
 *
 * 这里有一个现实前提：
 * 当前 SUPER 相关打印值还是按现有 ADC 显示口径在用，
 * 也就是它还没有完全换算成最终真实母线电压。
 * 所以后面如果你修正了 SUPER 的分压换算，
 * 下面这些 SUPER 阈值要跟着一起调整。
 */

/* 判断“主电已经稳定存在”的门限。
 * 低于这个值时，不认为 BAT24 是有效主电。
 * 这样做是为了解决刚上电、掉电边缘、电压抖动时误判主电存在的问题。
 */
#define APP_PWR_MAIN_PRESENT_THRESHOLD_MV          18000UL

/* 判断“超级电容已经充到可参与掉电保持”的门限。
 * 只有充到这个范围，才把 supercap_ready 置为 1。
 * 这样做是为了解决“刚上电时超容还没充满，却被误当成可支撑掉电收尾”的问题。
 */
#define APP_PWR_SUPERCAP_READY_THRESHOLD_MV        2100UL

/* 判断“主电掉电时，超级电容还够进入保持阶段”的门限。
 * 当前和 READY 阈值保持一致，先保证逻辑简单可验证。
 * 后面如果需要，也可以把 READY 和 HOLD 阈值拆成不同等级。
 */
#define APP_PWR_SUPERCAP_HOLD_THRESHOLD_MV         2100UL

/* 判断“超级电容已经低到不适合继续拖延，必须推进关机”的门限。
 * 低于这个值后，不再继续长时间保持，而是进入 SHUTDOWN_PENDING。
 * 这样做是为了解决超容电量继续被拖空、导致最终断电过晚的问题。
 */
#define APP_PWR_SUPERCAP_SHUTDOWN_THRESHOLD_MV     1800UL

/* 从进入超级电容保持开始，到进入关机准备的最长等待时间。
 * 即使超容还没明显掉到低压阈值，超过这个时间也要推进关机。
 * 这样做是为了解决系统一直挂在保持阶段不收尾的问题。
 */
#define APP_PWR_HOLD_PREPARE_TIMEOUT_MS            10000UL

/* 从 SHUTDOWN_PENDING 推进到 SHUTDOWN_IN_PROGRESS 的时间。
 * 这段时间相当于留给上层做“最后准备动作”的窗口。
 * 目前 SoC 关机请求还没接，这里先作为时序骨架保留。
 */
#define APP_PWR_SHUTDOWN_IN_PROGRESS_MS            3000UL

/* 掉电过程中如果主电突然恢复，先短延时再软件复位。
 * 这样做是为了解决“超容没放空又重新上电，系统卡在半掉电状态”的问题。
 */
#define APP_PWR_RECOVERY_RESET_DELAY_MS            50UL

/* 主电恢复确认时间。
 * 要连续满足这么久，才认定主电真的恢复稳定。
 * 这样做是为了解决车上电源抖动、插拔瞬态造成的误恢复问题。
 */
#define APP_PWR_MAIN_PRESENT_CONFIRM_MS            1000UL

/* 主电丢失确认时间。
 * 要连续掉到阈值以下这么久，才认定主电真的没了。
 * 这样做是为了解决瞬时跌落、采样抖动导致误触发掉电流程的问题。
 */
#define APP_PWR_MAIN_LOSS_CONFIRM_MS               1000UL

/* 超级电容 ready 确认时间。
 * 要连续满足这么久，才把 supercap_ready 置为 1。
 * 这样做是为了解决超容充电过程中电压刚碰到阈值就被过早判为 ready 的问题。
 */
#define APP_PWR_SUPERCAP_READY_CONFIRM_MS          3000UL

/* 超级电容低压确认时间。
 * 要连续低于低压阈值这么久，才认定它真的低了。
 * 这样做是为了解决掉电末期采样波动导致关机状态来回抖动的问题。
 */
#define APP_PWR_SUPERCAP_LOW_CONFIRM_MS            2000UL

/* 进入 SHUTDOWN_IN_PROGRESS 后，先等待这么久再切 SoC/24V/超容充电输出。
 * 这样做是为了让关机后半程有清晰的两段动作，而不是一进最终阶段就全部同时拉掉。
 */
#define APP_PWR_FINAL_SOC_CUT_DELAY_MS             1000UL

/* 切完 SoC/24V 相关输出后，再等这么久释放 MCU 自身保持。
 * 这样做是为了保证 MCU 有最后一点时间完成尾部状态推进，再真正把自己也断掉。
 */
#define APP_PWR_FINAL_HOLD_CUT_DELAY_MS            1500UL

/* 空骨架线程打印周期 */
#define APP_POWER_TASK_PERIOD_MS     1000
#define APP_CAN_TASK_PERIOD_MS       1200
#define APP_IO_TASK_PERIOD_MS        1400
#define APP_STORAGE_TASK_PERIOD_MS   1600
#define APP_LCD_TASK_PERIOD_MS       1800

#endif /* APPLICATIONS_APP_CONFIG_H_ */
