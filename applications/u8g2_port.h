#ifndef U8G2_PORT_H
#define U8G2_PORT_H

/*
 * ============================================================
 *  u8g2 移植层 (u8g2 Port)
 * ============================================================
 *
 * 功能:
 *   将 u8g2 图形库移植到本工程的硬件平台。
 *   u8g2 是一个纯 C 的嵌入式 GUI 库, 支持多种显示控制器。
 *   本移植层实现了 u8g2 与 ST7567 驱动之间的桥接。
 *
 * 移植接口:
 *   u8g2 需要底层提供"发送字节"和"设置 DC"等回调函数。
 *   这些回调在 u8g2_port.c 中通过 u8x8_byte_st7567_bb() 实现,
 *   最终调用 lcd_drv 层的函数。
 */

#include "u8g2.h"

void u8g2_port_init(void);          /* 初始化 u8g2 库 */
void u8g2_port_test_draw(void);     /* 测试绘制 */
u8g2_t *u8g2_port_get(void);        /* 获取 u8g2 对象指针 */

void u8g2_port_clear_buffer(void);  /* 清空 u8g2 内部缓冲区 */
void u8g2_port_flush_buffer(void);  /* 刷新 u8g2 缓冲区到 LCD */

#endif
