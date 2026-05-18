/*
 * app_task.c — 线程统一启动入口
 *
 * 所有服务线程在这里集中创建启动，顺序反映了依赖关系：
 *   LED → 电源 → CAN → LCD → 车辆IO → ADC → 存储 → 串口协议
 *
 * 每个线程的优先级和栈大小由 app_config.h 中的宏定义决定。
 * 每个 svc_*_task_start() 内部调用 rt_thread_create() 创建线程。
 */
#include <rtthread.h>

#include "app_task.h"
#include "svc_adc.h"
#include "svc_can.h"
#include "svc_led.h"
#include "svc_lcd.h"
#include "svc_power.h"
#include "svc_storage.h"
#include "svc_vehicle_io.h"
#include "app_usart_cmd.h"

int app_task_start(void)
{
    int result;

    /* 任务调度层只关心服务启动顺序，不再包含具体业务实现。 */

    /*
     * 线程创建顺序：
     * 1. LED 最先：运行指示灯要最早亮起来，标识系统已启动
     * 2. 电源状态机：需要尽快开始监控 ACC/ON 信号
     * 3. CAN 通信：车辆数据采集，与电源状态相关
     * 4. LCD 显示：UI 线程，优先级较低（app_config.h 中配置为 18）
     * 5. 车辆 IO 采集：独立硬件，不影响其他模块启动
     * 6. ADC 采样：电压/按键采集，频次较高
     * 7. EEPROM 存储：数据持久化，不需要实时响应
     * 8. 串口命令协议：与 SOC 通信，优先级最低（19）
     */
    result = svc_led_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = svc_power_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = svc_can_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = svc_lcd_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = svc_vehicle_io_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    /* ADC 任务交给独立服务模块启动，app_task 只负责统一调度。 */
    result = svc_adc_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = svc_storage_task_start();
    if (result != RT_EOK)
    {
        return result;
    }

    result = app_usart_cmd_task_start();
    if (result != RT_EOK)
    {
        return result;
    }
    return RT_EOK;
}
