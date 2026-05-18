#ifndef APPLICATIONS_LCD_LCD_DRV_H_
#define APPLICATIONS_LCD_LCD_DRV_H_

#include <rtthread.h>
#include <stdint.h>

/*
 * ============================================================
 *  LCD 驱动层 (LCD Driver Layer)
 * ============================================================
 *
 * 职责: 直接操作 ST7567 控制器的硬件底层
 *
 * 硬件接口:
 *   - GPIO 模拟 SPI (SCK=PA30, SDA=PA31)
 *   - 控制引脚: RST(PA3), A0(PA29), CS(PA28), BL(PC16)
 *
 * 本层只做"发命令、发数据"的原子操作, 不做任何图形逻辑
 */

/**
 * @brief 初始化 LCD 驱动
 *
 * 初始化步骤:
 *   1) 配置所有 GPIO 引脚为输出模式
 *   2) 初始化模拟 SPI 时序
 *   3) 发送 ST7567 初始化命令序列
 *   4) 清屏 → 打开显示
 */
int lcd_drv_init(void);

/**
 * @brief 硬件复位 ST7567
 *
 * RST 低脉冲时序:
 *   RST=0 → 等待 20ms → RST=1 → 等待 5ms
 *   20ms 确保内部振荡器稳定
 */
void lcd_reset(void);
void lcd_rst_set(rt_bool_t active);

/* 背光控制 */
void lcd_backlight_on(void);
void lcd_backlight_off(void);

/* A0: 0=命令, 1=数据 */
void lcd_a0_set(rt_bool_t is_data);

/* CS: 低电平有效 */
void lcd_csn_set(rt_bool_t active);

/**
 * @brief 通过模拟 SPI 发送一个字节
 *
 * 时序: CPOL=1, CPHA=1 (Mode 3)
 *   空闲时 SCK=高, 在 SCK 下降沿采样
 */
void lcd_spi_send_byte(uint8_t byte);

/* 全屏操作 */
void lcd_clear(void);     /* DDRAM 全写 0x00 */
void lcd_fill_all(void);  /* DDRAM 全写 0xFF */

/**
 * @brief 向 ST7567 写入一页数据
 * @param page 页号 (0~7)
 * @param col  起始列 (0~131)
 * @param buf  数据缓冲区
 * @param len  数据长度 (字节)
 */
void lcd_drv_write_page(uint8_t page, uint8_t col, const uint8_t *buf, uint16_t len);

#endif /* APPLICATIONS_LCD_LCD_DRV_H_ */
