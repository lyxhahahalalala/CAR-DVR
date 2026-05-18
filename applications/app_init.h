/*
 * app_init.h — 系统初始化接口
 *
 * 对外暴露 app_framework_init() 供 main() 调用。
 * 所有硬件和服务模块的初始化集中在这里编排。
 */
#ifndef APPLICATIONS_APP_INIT_H_
#define APPLICATIONS_APP_INIT_H_

/**
 * @brief 初始化所有硬件和服务模块（按依赖顺序）
 *
 * 在 main() 的"第一阶段"调用，此时 CPU 时钟/内存堆/串口已就绪。
 * 初始化顺序有严格要求：被依赖的模块必须优先初始化。
 *
 * @return RT_EOK 成功，-RT_ERROR 失败（任一模块初始化失败都返回错误）
 */
int app_framework_init(void);

#endif /* APPLICATIONS_APP_INIT_H_ */
