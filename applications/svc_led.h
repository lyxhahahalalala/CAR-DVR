#ifndef APPLICATIONS_SVC_LED_H_
#define APPLICATIONS_SVC_LED_H_

/*
 * ============================================================
 *  LED 指示服务 (LED Service)
 * ============================================================
 *
 * 功能:
 *   控制设备运行状态指示灯 (通常是绿色 LED, 标记为 APP_LED0)
 *
 * 闪烁模式:
 *   当前实现: 固定周期亮灭交替, 表示设备正常运行
 *   (周期由 APP_LED_TOGGLE_PERIOD_MS 配置)
 *
 * 未来扩展:
 *   可以根据设备状态改变闪烁模式:
 *   - 正常: 均匀闪烁
 *   - 告警: 快闪
 *   - 故障: 常亮或常灭
 */

int svc_led_init(void);
int svc_led_task_start(void);

#endif /* APPLICATIONS_SVC_LED_H_ */
