/*
 * ============================================================
 *  lcd_graphics.c — LCD 图形层 (Framebuffer 管理)
 * ============================================================
 *
 * 功能:
 *   管理 8 页 × 132 列 = 1056 字节的帧缓冲,
 *   提供清空、画点、刷屏和跨缓冲拷贝操作。
 *
 * 帧缓冲格式 (与 ST7567 DDRAM 一致):
 *   以"页 × 列"的二维数组组织:
 *     g_lcd_fb[page][col]
 *     每字节的 bit[n] 对应一列中的第 n 行
 *
 *   例如: g_lcd_fb[0][10] = 0b10000001 表示
 *     第 0 页(行 0~7)、第 10 列:
 *       - 行 0 点亮 (bit 7)
 *       - 行 7 点亮 (bit 0)
 *
 * 这种"局部修改→整体刷新"的模式是嵌入式 GUI 的经典做法:
 *   1) 先修改帧缓冲中的像素
 *   2) 然后整体 flush 到 LCD
 *   这样可以将多次像素操作合并为一次 SPI 传输
 */
#include "lcd_graphics.h"

#include <string.h>

#include "lcd_drv.h"

#define LCD_COLS    132     /* 每页 132 列 */
#define LCD_PAGES   8       /* 8 页 (64 行) */
#define LCD_ROWS    64      /* 总行数 (8 × 8 = 64) */

/* ============================================================
 *  全局帧缓冲
 * ============================================================
 *
 * 为什么是静态全局变量 (static) 而不是动态分配?
 *   1) 大小固定 (1056 字节), 不需要动态分配
 *   2) 静态分配在 BSS 段, 上电自动初始化为 0
 *   3) 避免了堆碎片问题 (RT-Thread 的堆可能碎片化)
 *
 * 为什么不放在外部(例如 svc_lcd.c 中)管理?
 *   帧缓冲是本层的私有数据, 对外只暴露操作接口,
 *   符合"数据隐藏"的封装原则
 */
static uint8_t g_lcd_fb[LCD_PAGES][LCD_COLS];

/* ============================================================
 *  内部函数 (static, 对外隐藏实现细节)
 * ============================================================ */

/**
 * 清空帧缓冲: 所有字节写 0
 */
static void lcd_fb_clear(void)
{
    rt_memset(g_lcd_fb, 0, sizeof(g_lcd_fb));
}

/**
 * 在帧缓冲中设置一个像素
 *
 * 坐标转换:
 *   page = y / 8     → 确定在哪一页
 *   bit  = 7 - (y%8) → 确定页内的哪一位
 *                      (使用 7 - n 是因为 ST7567 的 MSB 对应最上面一行)
 *
 * 为什么 bit 是 7 - (y%8) 而不是 (y%8)?
 *   这取决于 LCD 面板的物理排列方向:
 *   ST7567 的 DDRAM 中, 一个字节的 bit 7 对应 COM0(最上面一行),
 *   bit 0 对应 COM7(最下面一行)。所以行号 y 在页内的偏移
 *   需要反转: y=48 → page=6, bit=7-(48%8)=7 → COM48
 */
static void lcd_fb_set_pixel(uint8_t x, uint8_t y, rt_bool_t on)
{
    uint8_t page;
    uint8_t bit;

    /* 边界检查: 防止数组越界 */
    if ((x >= LCD_COLS) || (y >= LCD_ROWS)) {
        return;
    }

    page = (uint8_t)(y / 8U);
    bit = (uint8_t)(7U - (y % 8U));

    if (on) {
        g_lcd_fb[page][x] |= (uint8_t)(1U << bit);   /* 点亮: 置位 */
    } else {
        g_lcd_fb[page][x] &= (uint8_t)~(uint8_t)(1U << bit);  /* 熄灭: 清零 */
    }
}

/**
 * 字节位反转函数
 *
 * 为什么需要位反转?
 *   u8g2 的帧缓冲和本地的帧缓冲可能使用不同的位序:
 *   - u8g2 输出: bit 0 对应最上面一行(LSB 优先)
 *   - ST7567 硬件: bit 7 对应最上面一行(MSB 优先)
 *   如果不反转, 显示的内容会上下颠倒
 *
 * 实现: 经典的二分法位反转算法
 *   第 1 步: 高低半字节交换
 *   第 2 步: 每 2 位一组交换
 *   第 3 步: 每 1 位交换
 *   3 步完成 8 位的完全反转
 *
 *   例如: 0b11001010 → 0b01010011
 */
static uint8_t lcd_reverse_byte(uint8_t v)
{
    v = (uint8_t)(((v & 0xF0U) >> 4) | ((v & 0x0FU) << 4));   /* 高低半字节互换 */
    v = (uint8_t)(((v & 0xCCU) >> 2) | ((v & 0x33U) << 2));   /* 每 2 位互换 */
    v = (uint8_t)(((v & 0xAAU) >> 1) | ((v & 0x55U) << 1));   /* 每 1 位互换 */
    return v;
}

/**
 * 将帧缓冲刷新到 ST7567
 *
 * 遍历所有 8 页, 调用驱动层的 lcd_drv_write_page() 写入
 */
static void lcd_fb_flush(void)
{
    uint8_t page;

    for (page = 0; page < LCD_PAGES; page++) {
        lcd_drv_write_page(page, 0, g_lcd_fb[page], LCD_COLS);
    }
}

/* ============================================================
 *  公共接口 (以 lcd_fb_public_ 为前缀)
 * ============================================================
 *
 * 为什么要有"内部函数"和"公共函数"两层?
 *   内部函数(无 _public_ 前缀)只在当前文件中使用,
 *   公共函数(有 _public_ 前缀)供外部模块(如 svc_lcd)调用。
 *
 *   这是一种"命名空间"的技巧:
 *   内部函数名简洁, 公共函数名带_public_表示这是对外接口。
 *   但实际上这里都直接调用了内部函数, 没有额外逻辑。
 *   这是一种"预留扩展点"的设计——未来如果在公共接口中
 *   需要加锁或其他处理, 可以直接在公共函数中添加。
 */
void lcd_fb_public_clear(void)
{
    lcd_fb_clear();
}

void lcd_fb_public_set_pixel(uint8_t x, uint8_t y, rt_bool_t on)
{
    lcd_fb_set_pixel(x, y, on);
}

void lcd_fb_public_flush(void)
{
    lcd_fb_flush();
}

/**
 * 从外部源(如 u8g2 渲染结果)拷贝数据到帧缓冲
 *
 * 处理流程:
 *   1) 对源数据的每一页
 *   2) 使用 src_page ^ 3 计算目标页 (页重映射)
 *   3) 对每个字节做位反转
 *
 * 为什么需要 page ^ 3 (页重映射)?
 *   u8g2 的帧缓冲组织和本地可能不同:
 *   u8g2 输出通常从上到下排列: page0, page1, page2, ...
 *   但本地帧缓冲配合 ST7567 的 COM 扫描方向可能需要重新排列:
 *   page 0 → page 3 (0 ^ 3 = 3)
 *   page 1 → page 2 (1 ^ 3 = 2)
 *   page 2 → page 1 (2 ^ 3 = 1)
 *   page 3 → page 0 (3 ^ 3 = 0)
 *   page 4 → page 7 (4 ^ 3 = 7)
 *   ...
 *   这是一种常见的页顺序变换, 与硬件 COM 扫描方向和面板
 *   的物理连接方式有关。
 *
 * 为什么需要 lcd_reverse_byte()?
 *   源数据(来自 u8g2)和本地帧缓冲的字节内位序不同,
 *   必须逐字节反转以保证显示正确。
 */
void lcd_fb_public_copy_pages(const uint8_t *src, uint16_t src_stride)
{
    uint8_t src_page;
    uint16_t x;

    if (src == RT_NULL) {
        return;
    }

    for (src_page = 0; src_page < LCD_PAGES; src_page++) {
        uint8_t dst_page = (uint8_t)(src_page ^ 3U);           /* 页重映射 */
        const uint8_t *page_ptr = src + src_page * src_stride; /* 源数据指针偏移 */

        /* 逐字节拷贝并位反转 */
        for (x = 0; x < LCD_COLS; x++) {
            g_lcd_fb[dst_page][x] = lcd_reverse_byte(page_ptr[x]);
        }
    }
}
