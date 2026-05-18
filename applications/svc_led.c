/*
 * ============================================================
 *  svc_led.c — LED 指示服务
 * ============================================================
 *
 * 功能:
 *   以固定的周期闪烁设备的状态指示灯。
 *   这是设备"心跳"信号, 告诉用户设备正在运行。
 *
 * 为什么 LED 闪烁要单独一个线程?
 *   1) 实时性: LED 闪烁的定时不能被其他任务阻塞
 *   2) 解耦: 闪烁逻辑独立, 其他模块只需要调用 app_led_write()
 *      在需要时改变状态, 不需要关心定时
 *   3) 简单: 一个 while(1) + delay 循环就是最可靠的定时方式,
 *      不需要使用硬件定时器(节省定时器资源给其他模块)
 *
 * 当前只控制 APP_LED0 (状态灯), 未来可以扩展到更多 LED
 */
#include <rtthread.h>
#include "rtt_board.h"

#include "app_config.h"
#include "svc_led.h"

/*
 * ============================================================
 *  svc_led_thread_entry — LED 闪烁线程入口
 * ============================================================
 *
 * 执行周期: APP_LED_TOGGLE_PERIOD_MS × 2
 *   (亮 → 等待 → 灭 → 等待, 一个完整周期是 2×delay)
 *
 * 为什么是亮/灭交替而不是 PWM 调光?
 *   - 对于状态指示灯, 亮灭交替已经足够让用户感知
 *   - PWM 需要硬件定时器, 这个项目中定时器资源有限
 *   - 简单的 GPIO 翻转 + delay 即可实现, 不需要额外硬件资源
 *
 * 被注释掉的行:
 *   //app_led_write(APP_LED0, APP_LED_ON);
 *   如果取消注释, 第二个 delay 也会亮灯, 那就变成了常亮。
 *   这行被注释掉说明开发者可能调试过不同的闪烁模式。
 */
static void svc_led_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        app_led_write(APP_LED0, APP_LED_ON);           /* 点亮 LED */
        rt_thread_mdelay(APP_LED_TOGGLE_PERIOD_MS);    /* 保持亮 */
        app_led_write(APP_LED0, APP_LED_OFF);          /* 熄灭 LED */
        //app_led_write(APP_LED0, APP_LED_ON);          /* (注释掉: 如果取消就变成常亮) */
        rt_thread_mdelay(APP_LED_TOGGLE_PERIOD_MS);    /* 保持灭 */
    }
}

/*
 * ============================================================
 *  模块初始化
 * ============================================================
 */
int svc_led_init(void)
{
    /*
     * LED 依赖板级引脚初始化，这里保留统一服务初始化入口。
     *
     * 为什么不在这里初始化 GPIO?
     *   LED 的 GPIO 初始化在 board 层 (rtt_board.c) 的
     *   app_init_led_pins() 中完成, 在 app_init.c 中调用。
     *   这里只是保留了一个统一的初始化接口,
     *   未来如果 LED 需要额外初始化可以加在这里。
     */
    return RT_EOK;
}

int svc_led_task_start(void)
{
    rt_thread_t led_thread;

    led_thread = rt_thread_create(APP_LED_TASK_NAME,
                                  svc_led_thread_entry,
                                  RT_NULL,
                                  APP_LED_TASK_STACK_SIZE,
                                  APP_LED_TASK_PRIORITY,
                                  APP_LED_TASK_TICK);
    if (led_thread == RT_NULL)
    {
        rt_kprintf("led thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(led_thread);
    return RT_EOK;
}
