/*
 * app_config.h — 全局配置常量
 *
 * 这个文件集中管理所有模块的配置参数，包括：
 *   - 线程名称 / 栈大小 / 优先级 / 周期
 *   - ADC 通道号和电压计算参数
 *   - 电源管理阈值和时间常数
 *   - CAN 通信参数
 *   - 调试开关
 *
 * 原则：所有"魔法数字"都应该定义在这里，而不是散布在各 .c 文件中。
 * 修改硬件或调整时序时，只需要改这个文件，不需要深入业务代码。
 */
#ifndef APPLICATIONS_APP_CONFIG_H_
#define APPLICATIONS_APP_CONFIG_H_

/* ==================== 线程配置 ==================== */

/*
 * LED 指示灯线程
 * 优先级 10（较高），栈 1024 字节，每 10 tick 运行一次
 * 用来控制运行指示灯闪烁
 */
#define APP_LED_TASK_NAME            "led_th"
#define APP_LED_TASK_STACK_SIZE      1024
#define APP_LED_TASK_PRIORITY        10
#define APP_LED_TASK_TICK            10

/*
 * ADC 采样线程
 * 优先级 11，栈 2048 字节，每 10 tick 运行一次
 * 负责 4 通道 ADC 采样，为电源管理/按键解码/电压监测提供数据
 */
#define APP_ADC_TASK_NAME            "adc_th"
#define APP_ADC_TASK_STACK_SIZE      2048
#define APP_ADC_TASK_PRIORITY        11
#define APP_ADC_TASK_TICK            10

/*
 * 电源管理线程
 * 优先级 8（最高应用级优先级），栈 2048 字节
 * 核心状态机，响应 ACC/ON 事件，控制系统上下电
 * 优先级最高是因为电源状态变化必须及时响应，否则可能导致数据丢失
 */
#define APP_POWER_TASK_NAME          "power_th"
#define APP_POWER_TASK_STACK_SIZE    2048
#define APP_POWER_TASK_PRIORITY      8
#define APP_POWER_TASK_TICK          10

/*
 * CAN 通信线程
 * 优先级 9，栈 1024 字节
 * 周期性发送/接收车辆 CAN 数据
 */
#define APP_CAN_TASK_NAME            "can_th"
#define APP_CAN_TASK_STACK_SIZE      1024
#define APP_CAN_TASK_PRIORITY        9
#define APP_CAN_TASK_TICK            10

/* CAN 设备名称和波特率 */
#define APP_CAN_DEV_NAME             BOARD_CAN_NAME
#define APP_CAN_BAUDRATE             CAN250kBaud

/* CAN 接收线程（优先级 18 较低，数据量大但不紧急） */
#define APP_CAN_RX_THREAD_NAME       "can_rx"
#define APP_CAN_RX_THREAD_STACK      3072
#define APP_CAN_RX_THREAD_PRIORITY   18
#define APP_CAN_RX_THREAD_TICK       20

/* CAN 发送线程（优先级 19 更低） */
#define APP_CAN_TX_THREAD_NAME       "can_tx"
#define APP_CAN_TX_THREAD_STACK      2048
#define APP_CAN_TX_THREAD_PRIORITY   19
#define APP_CAN_TX_THREAD_TICK       20

/* CAN 错误处理线程（优先级 20 最低，不需要实时响应） */
#define APP_CAN_ERR_THREAD_NAME      "can_err"
#define APP_CAN_ERR_THREAD_STACK     2048
#define APP_CAN_ERR_THREAD_PRIORITY  20
#define APP_CAN_ERR_THREAD_TICK      20

/* CAN 发送消息队列（深度 16，缓冲待发送的 CAN 帧） */
#define APP_CAN_TX_MQ_NAME           "can_txq"
#define APP_CAN_TX_MQ_DEPTH          16

/* CAN 测试发包开关（0=关闭，1=启用，调试用） */
#define APP_CAN_TEST_TX_ENABLE       0
#define APP_CAN_TEST_TX_THREAD_NAME   "can_ttx"
#define APP_CAN_TEST_TX_THREAD_STACK  2048
#define APP_CAN_TEST_TX_THREAD_PRIORITY 21
#define APP_CAN_TEST_TX_THREAD_TICK   20
#define APP_CAN_TEST_TX_PERIOD_MS     1000
#define APP_CAN_TEST_TX_ID            0x123UL

/* ==================== 产品信息 ==================== */

/* 软件版本号 */
#define APP_SOFTWARE_VERSION "1.0.0"

/* 车辆管理辖区代码（429006 = 湖南省某地区，行驶记录仪国标要求） */
#define APP_ADMIN_REGION_CODE 429006UL

/* ==================== 日志开关 ==================== */

#define APP_DEBUG_CAN_ONLY           0  /* 为 1 时只输出 CAN 日志 */
#define APP_SHUTDOWN_TRACE_ONLY      0  /* 为 1 时只输出关机过程日志 */
#define APP_CAN_LOG_ENABLE           0  /* 为 1 时启用 CAN 详细日志 */

/* 非 CAN 日志的编译开关：根据上文配置决定是否输出 */
#if APP_DEBUG_CAN_ONLY
#define APP_NON_CAN_LOG(...)         ((void)0)
#elif APP_SHUTDOWN_TRACE_ONLY
#define APP_NON_CAN_LOG(...)         ((void)0)
#else
#define APP_NON_CAN_LOG(...)         rt_kprintf(__VA_ARGS__)
#endif

/* ==================== 车辆 IO 线程配置 ==================== */
#define APP_IO_TASK_NAME             "vehio_th"
#define APP_IO_TASK_STACK_SIZE       1024
#define APP_IO_TASK_PRIORITY         12
#define APP_IO_TASK_TICK             10

/* ==================== 存储线程配置 ==================== */
#define APP_STORAGE_TASK_NAME        "store_th"
#define APP_STORAGE_TASK_STACK_SIZE  1024
#define APP_STORAGE_TASK_PRIORITY    13
#define APP_STORAGE_TASK_TICK        10

/* ==================== LCD 线程配置 ==================== */
#define APP_LCD_TASK_NAME            "lcd_th"
#define APP_LCD_TASK_STACK_SIZE      2048
#define APP_LCD_TASK_PRIORITY        14
#define APP_LCD_TASK_TICK            10

/* LED 闪烁周期（毫秒），500ms = 1Hz 闪烁 */
#define APP_LED_TOGGLE_PERIOD_MS     500

/* ==================== ADC 采样配置 ==================== */

#define APP_ADC_DEV_NAME             "adc0"
#define APP_ADC_CH_BAT_24V           2    /* ADC 通道 2：24V 主电池电压 */
#define APP_ADC_CH_LI_BAT_4V2        3    /* ADC 通道 3：锂电池电压（4.2V） */
#define APP_ADC_CH_SUPER_C_5V        4    /* ADC 通道 4：超级电容电压 */
#define APP_ADC_CH_KEY               11   /* ADC 通道 11：按键分压 */

#define APP_ADC_SAMPLE_PERIOD_MS     10   /* 采样周期 10ms */
#define APP_ADC_STARTUP_DELAY_MS     500  /* 启动后延时 500ms 开始采样（等电源稳定） */

/*
 * ADC 量程和参考电压
 * HPM6280 的 ADC 是 16 位，满量程 = 65535
 * VREF = 3.3V（板载参考电压）
 */
#define APP_ADC_FULL_SCALE           65535UL
#define APP_ADC_VREF_MV              3300UL

/*
 * 分压电阻值（用于反算实际电压）
 * 24V 电池：上拉 1000KΩ，下拉 91KΩ，分压比 = 91/(1000+91)
 * 锂电池：上拉 91KΩ，下拉 91KΩ，分压比 = 91/(91+91)
 * 公式：实际电压(mV) = ADC原始值 × VREF(mV) / 满量程 × 分压比倒数
 */
#define APP_ADC_BAT24_DIV_UP_KOHM    1000UL
#define APP_ADC_BAT24_DIV_DOWN_KOHM  91UL

#define APP_ADC_LI_BAT_DIV_UP_KOHM   91UL
#define APP_ADC_LI_BAT_DIV_DOWN_KOHM 91UL

/* ==================== 电源管理阈值与时序 ==================== */

/*
 * 电源状态判断的电压阈值（单位：mV）
 * 这些值需要根据实际硬件调试确定，与分压电阻精度有关
 */
#define APP_PWR_MAIN_PRESENT_THRESHOLD_MV    18000UL  /* 主电源存在阈值：≥18V 认为主电接入 */
#define APP_PWR_SUPERCAP_READY_THRESHOLD_MV  2100UL   /* 超级电容就绪阈值：≥2.1V */
#define APP_PWR_SUPERCAP_HOLD_THRESHOLD_MV   2100UL   /* 超级电容维持阈值 */
#define APP_PWR_SUPERCAP_SHUTDOWN_THRESHOLD_MV 1800UL /* 超级电容关机阈值：<1.8V 强制断电 */

/*
 * 电源状态切换的防抖时间（单位：ms）
 * 防止电压波动导致状态误切换
 */
#define APP_PWR_MAIN_PRESENT_CONFIRM_MS    1000UL  /* 主电接入确认：连续 1s 检测通过 */
#define APP_PWR_MAIN_LOSS_CONFIRM_MS       1000UL  /* 主电断开确认：连续 1s 检测低于阈值 */
#define APP_PWR_SUPERCAP_READY_CONFIRM_MS  3000UL  /* 超级电容就绪确认：连续 3s */
#define APP_PWR_SUPERCAP_LOW_CONFIRM_MS    2000UL  /* 超级电容电压低确认：连续 2s */

/*
 * 关机时序控制
 * 为了防止突然断电导致里程数据丢失，系统需要按顺序执行断电：
 *   1. 通知 SOC 准备关机（PWR_HOLD_PREPARE）
 *   2. 等待 SOC 处理完数据
 *   3. 切断 SOC 电源
 *   4. 释放自锁信号
 */
#define APP_PWR_HOLD_PREPARE_TIMEOUT_MS        10000UL  /* SOC 准备超时：最多等 10s */
#define APP_PWR_SHUTDOWN_IN_PROGRESS_MS        3000UL   /* 正在关机中：保持 3s */
#define APP_PWR_RECOVERY_RESET_DELAY_MS        50UL     /* 恢复复位延迟：50ms */

/* 超级电容管理开关（0=关闭超级电容管理，1=启用） */
#define APP_PWR_SUPERCAP_MGMT_ENABLE               0

/* 最终断电时序 */
#define APP_PWR_FINAL_SOC_CUT_DELAY_MS     1000UL  /* 先切断 SOC 电源：等待 1s */
#define APP_PWR_FINAL_HOLD_CUT_DELAY_MS    1500UL  /* 再释放 PWR_HOLD：等待 1.5s */
#define APP_PWR_SOC_ON_DELAY_MS            5000UL  /* SOC 上电等待：5s 后认为 SOC 启动完成 */
#define APP_CAM_12V_ENABLE_DELAY_MS        5000UL  /* 摄像头 12V 上电延迟：5s */

/* ==================== 线程运行周期 ==================== */

/* 各服务线程的运行周期（毫秒），线程在 while(1) 中 sleep 这个时长 */
#define APP_POWER_TASK_PERIOD_MS     100
#define APP_CAN_TASK_PERIOD_MS       1200
#define APP_IO_TASK_PERIOD_MS        100
#define APP_STORAGE_TASK_PERIOD_MS   1600
#define APP_LCD_TASK_PERIOD_MS       900

/* CAN 日志编译开关 */
#if APP_CAN_LOG_ENABLE
#define APP_CAN_LOG(...)             rt_kprintf(__VA_ARGS__)
#else
#define APP_CAN_LOG(...)             ((void)0)
#endif

#endif  /* APPLICATIONS_APP_CONFIG_H_ */
