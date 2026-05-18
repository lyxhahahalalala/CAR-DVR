#ifndef APPLICATIONS_LCD_LCD_GRAPHICS_H_
#define APPLICATIONS_LCD_LCD_GRAPHICS_H_

#include <rtthread.h>
#include <stdint.h>

/*
 * ============================================================
 *  LCD 图形层 (Graphics Layer)
 * ============================================================
 *
 * 职责:
 *   管理帧缓冲 (Framebuffer), 提供像素级别的图形操作。
 *
 * 为什么需要帧缓冲?
 *   ST7567 不支持从 DDRAM 读回数据。如果要局部修改屏幕,
 *   必须先在内存中修改, 再整帧刷新到硬件。
 *
 * 帧缓冲大小: 8 页 × 132 列 = 1056 字节
 *   页 0 → 行 0~7, 页 1 → 行 8~15, ..., 页 7 → 行 56~63
 *   每字节的 8 位对应一列中的 8 个像素行
 */

/**
 * @brief 清空公共帧缓冲
 */
void lcd_fb_public_clear(void);

/**
 * @brief 在帧缓冲中设置/清除一个像素
 * @param x  列坐标 (0~131)
 * @param y  行坐标 (0~63)
 * @param on true=点亮, false=熄灭
 */
void lcd_fb_public_set_pixel(uint8_t x, uint8_t y, rt_bool_t on);

/**
 * @brief 将帧缓冲全部刷到 ST7567 DDRAM
 *
 * 遍历所有 8 页, 调用 lcd_drv_write_page() 逐页写入
 */
void lcd_fb_public_flush(void);

/**
 * @brief 将外部图像数据拷贝到帧缓冲
 * @param src        源数据
 * @param src_stride 源数据的行步长(字节数)
 *
 * 用途: 将 u8g2 渲染的位图拷贝到本地帧缓冲中
 *       注意处理字节位序差异 (u8g2 可能使用不同的位序)
 */
void lcd_fb_public_copy_pages(const uint8_t *src, uint16_t src_stride);

#endif /* APPLICATIONS_LCD_LCD_GRAPHICS_H_ */
