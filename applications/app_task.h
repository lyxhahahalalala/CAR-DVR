/*
 * app_task.h — 线程启动接口
 *
 * 对外暴露 app_task_start() 供 main() 第二阶段调用。
 * 所有服务线程都在这里统一创建启动。
 */
#ifndef APPLICATIONS_APP_TASK_H_
#define APPLICATIONS_APP_TASK_H_

/**
 * @brief 启动所有服务线程（按优先级/时机顺序）
 *
 * 在 main() 的"第二阶段"调用，此时所有硬件和服务已初始化就绪。
 * 线程创建顺序会影响首次调度的时机。
 *
 * @return RT_EOK 成功，非 RT_EOK 表示失败（任一线程创建失败即返回）
 */
int app_task_start(void);

#endif /* APPLICATIONS_APP_TASK_H_ */
