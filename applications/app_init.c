/*
 * app_init.c — 系统初始化编排
 *
 * 所有硬件和服务模块的初始化都集中在这里，按严格的依赖顺序排列：
 *
 *   初始化顺序            | 为什么这个顺序
 *   ---------------------|----------------------------------------------
 *   app_init_led_pins()  | 最优先：即使后面初始化失败，LED 还能闪灯提示
 *   svc_led_init()       | LED 服务最基础，其他模块初始化失败后用它报错
 *   svc_adc_init()       | ADC 先于电源：电源状态机需要读取电压值做判断
 *   svc_lcd_init()       | LCD 显示，开机 logo 需要尽早显示
 *   svc_power_init()     | 电源状态机：依赖 ADC 已就绪
 *   svc_can_init()       | CAN 通信：依赖系统时钟已稳定
 *   svc_vehicle_io_init()| 车辆 IO 采集：独立硬件，无特殊依赖
 *   svc_storage_init()   | EEPROM 存储：依赖 I2C 总线已就绪
 *   app_usart_cmd_init() | 串口协议：最后初始化，因为通讯不着急
 *
 *  注意：svc_*_init() 目前没有逐个检查返回值，后续改进点。
 */
#include "app_init.h"

#include <rtthread.h>
#include "rtt_board.h"
#include "svc_adc.h"
#include "svc_can.h"
#include "svc_led.h"
#include "svc_lcd.h"
#include "svc_power.h"
#include "svc_storage.h"
#include "svc_vehicle_io.h"
#include "app_usart_cmd.h"

int app_framework_init(void)
{
    /* ========== 第一阶段：板级引脚初始化 ========== */
    app_init_led_pins();

    /* ========== 第二阶段：服务模块初始化（按依赖顺序） ========== */

    /* LED 最先初始化，后续模块出问题时可闪烁提示 */
    svc_led_init();
    /* ADC 采样模块：电源管理需要借用 ADC 读取电压 */
    svc_adc_init();
    /* LCD 显示：开机 logo / 自检信息 */
    svc_lcd_init();
    /* 电源管理状态机：依赖 ADC 已就绪读取电压判断 */
    svc_power_init();
    /* CAN 通信：3 路 CAN FD，用于车辆数据采集 */
    svc_can_init();
    /* 车辆 IO 采集：ACC/ON/10 路开关量 */
    svc_vehicle_io_init();
    /* EEPROM 存储：里程 / 配置 / 电话号码持久化 */
    svc_storage_init();
    /* 串口命令协议：与 SOC 通信，最后初始化通讯不急 */
    //app_usart_cmd_init();  /* 旧版本：无错误检查 */
    if (app_usart_cmd_init() != RT_EOK)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}
