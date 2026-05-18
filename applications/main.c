/*
 * main.c — 系统主入口
 *
 * 启动流程分为两步：
 *   1. app_framework_init() — 初始化所有硬件和服务（按依赖顺序）
 *   2. app_task_start()     — 启动所有服务线程（按优先级/时机顺序）
 *
 * 两步分离的好处：可以只做初始化不启线程，方便硬件调试。
 */
#include "app_init.h"
#include "app_task.h"

#include "u8g2_port.h"

int main(void)
{
    /*
     * 第一阶段：初始化所有硬件和服务模块。
     * 此时 CPU 时钟、内存堆、串口控制台等已由 rtt_board_init() 就绪。
     * 这里只做"硬件配置 + 数据结构就绪"，不创建线程。
     */
    app_framework_init();

    /* u8g2 测试代码（调试时启用，正式发布时注释掉） */
    //u8g2_port_init();
    //u8g2_port_test_draw();

    /*
     * 第二阶段：启动所有服务线程。
     * 各服务独立线程运行，通过 IPC 通信，任务调度器开始工作。
     */
    app_task_start();

    /* main() 不会走到这里，RT-Thread 调度器接管后会在线程中循环 */
    return 0;
}

