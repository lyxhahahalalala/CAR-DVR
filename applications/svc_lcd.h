#ifndef APPLICATIONS_SVC_LCD_H_
#define APPLICATIONS_SVC_LCD_H_

#include <rtthread.h>

/*
 * ============================================================
 *  LCD 服务层接口 (Service Layer)
 * ============================================================
 *
 * 职责分层:
 *   svc_lcd  (服务层)  ← 本文件 —— 对外提供统一的 LCD 操作接口
 *   lcd_drv  (驱动层)  —— 直接操作 SPI/GPIO，发送命令/数据到 ST7567
 *   lcd_graphics (图形层) —— 帧缓冲(Framebuffer)管理，像素/画图
 *   lcd_ui   (UI 层)  —— 菜单系统、页面导航
 *
 * 为什么不在应用层直接调用驱动/图形/UI 层?
 *   1) 解耦 —— 如果将来换屏幕(例如换 OLED 或彩屏)，只需要改 svc_lcd 内部实现
 *   2) 一致性 —— 所有模块通过 svc_lcd_update_xxx() 更新数据，统一刷新时机
 *   3) 线程安全 —— svc_lcd 内部的互斥锁保护帧缓冲，外部调用者不需要关心
 */

/* ---------- 初始化与任务 ---------- */

/**
 * @brief 初始化 LCD 硬件和软件子系统
 *
 * 调用顺序:
 *   1) gpio_init   —— 初始化 RESET、A0、CS、Backlight 等 GPIO
 *   2) spi_init    —— 初始化 SPI 总线
 *   3) lcd_reset   —— 硬件复位 ST7567
 *   4) lcd_init    —— 发送初始化命令序列
 *   5) u8g2_init   —— 初始化 u8g2 图形库
 *   6) clear+flush —— 清屏并刷新
 */
int svc_lcd_init(void);

/**
 * @brief 创建 LCD 刷新线程
 *
 * 单独线程的原因:
 *   - LCD 刷新涉及 SPI 传输，速度较慢(软件 SPI 约几百 kHz)
 *   - 放在单独线程中不会阻塞主业务逻辑(如 CAN 处理、电源管理)
 *   - 线程优先级一般设为中等(如 18)，确保 GUI 响应及时但不抢占关键任务
 */
int svc_lcd_task_start(void);

/* ---------- 基础硬件控制 ---------- */

/**
 * @brief 硬件复位 ST7567 控制器
 *
 * 通过 RST 引脚发送低脉冲:
 *   RST=0 → 等待 ≥1μs → RST=1 → 等待 ≥10ms
 *
 * 为什么需要硬件复位?
 *   上电时 ST7567 内部状态不确定，硬件复位确保寄存器回到默认值
 */
void lcd_reset(void);

void lcd_backlight_on(void);   /* 打开背光 (BL_PWM = 1) */
void lcd_backlight_off(void);  /* 关闭背光 (BL_PWM = 0) */

/**
 * @brief 设置 A0 (数据/命令选择线)
 * @param is_data true=A0=1(发送显示数据), false=A0=0(发送命令)
 *
 * ST7567 协议:
 *   A0=0 → 写入的是命令(如设置列地址、页地址)
 *   A0=1 → 写入的是显示数据(DDRAM)
 */
void lcd_a0_set(rt_bool_t is_data);

/**
 * @brief 设置片选 CS
 * @param active true=CS=0(选中), false=CS=1(取消选中)
 *
 * 低电平有效: ST7567 的 CS 引脚是低电平选中
 */
void lcd_csn_set(rt_bool_t active);

void lcd_rst_set(rt_bool_t active);    /* 控制复位引脚电平 */
void lcd_spi_send_byte(uint8_t byte);  /* 向 LCD 发送一个字节 */

/* ---------- 显示操作 ---------- */

void lcd_clear(void);     /* 清屏: 所有 DDRAM 写 0x00 */
void lcd_fill_all(void);  /* 全亮: 所有 DDRAM 写 0xFF */

/* ---------- 帧缓冲 (Framebuffer) 操作 ---------- */

/**
 * @brief 清空公共帧缓冲
 *
 * 帧缓冲大小: 132 列 × 8 页 = 1056 字节
 * 对应到 ST7567 DDRAM 的一对一映射
 *
 * 为什么需要帧缓冲?
 *   ST7567 不支持从 DDRAM 读回数据(读操作时序复杂且慢)
 *   必须在内存中维护一份"影子"缓冲，才能做局部修改
 */
void lcd_fb_public_clear(void);

/**
 * @brief 在帧缓冲中设置/清除一个像素
 * @param x  列坐标 (0~131)
 * @param y  行坐标 (0~63)
 * @param on true=点亮, false=熄灭
 *
 * 实现原理:
 *   page = y / 8       (确定所在的页)
 *   bit  = y % 8       (确定页内的位)
 *   fb[page * 132 + x] |= (1 << bit) 或 &= ~(1 << bit)
 */
void lcd_fb_public_set_pixel(uint8_t x, uint8_t y, rt_bool_t on);

/**
 * @brief 将帧缓冲全部刷到 ST7567 DDRAM
 *
 * 刷屏过程:
 *   1) 设置列地址 = 0
 *   2) 对每页(0~7)设置页地址
 *   3) 连续发送 132 字节数据
 *
 * 为什么不每次修改都立即刷?
 *   频繁 SPI 传输会占用 CPU。应用层先修改 fb，然后一次 flush
 *   这样可以批量合并更新，减少 SPI 通信次数
 */
void lcd_fb_public_flush(void);

/**
 * @brief 将外部图像数据拷贝到帧缓冲的指定区域
 * @param src        源数据指针
 * @param src_stride 源数据每行字节数(步长)
 *
 * 用途: 将 u8g2 渲染好的位图拷贝到本地的帧缓冲中
 *       实现 u8g2 图形库与本地帧缓冲的桥接
 *
 * 注意:
 *   src_stride 可能不等于 132(例如 u8g2 使用 128 列缓冲)
 *   函数内部需要处理 stride 不同时的逐行拷贝逻辑
 */
void lcd_fb_public_copy_pages(const uint8_t *src, uint16_t src_stride);

/* ---------- 主页 UI 数据更新接口 ---------- */

/**
 * @brief 更新主页时间显示
 * @param hour   小时 (0~23)
 * @param minute 分钟 (0~59)
 * @param second 秒   (0~59)
 *
 * 时间来源: GPS/北斗 定位模块 或 RTC 芯片
 */
void svc_lcd_update_home_time(uint8_t hour, uint8_t minute, uint8_t second);

/**
 * @brief 更新顶部状态栏
 * @param value 位域组合, 各 bit 代表不同状态
 *
 * 状态包括: GPS 定位状态、卫星颗数、记录状态、卡状态等
 * 具体 bit 定义见 app_config.h 或 svc_lcd.c 内部宏
 */
void svc_lcd_update_top_status(uint8_t value);

/**
 * @brief 更新主页速度显示
 * @param speed_kmh_x10 速度值, 单位 0.1 km/h
 *                      例如 655 表示 65.5 km/h
 */
void svc_lcd_update_home_speed(uint16_t speed_kmh_x10);

/**
 * @brief 更新行驶时间显示
 * @param hour   小时
 * @param minute 分钟
 * @param second 秒
 *
 * 行驶时间: 从车辆行驶开始累计, 停车时暂停计时
 */
void svc_lcd_update_drive_time(uint8_t hour, uint8_t minute, uint8_t second);

/**
 * @brief 更新驾驶员卡号显示
 * @param card_id 卡号字符串(通常是 20 位数字)
 */
void svc_lcd_update_card_id(const char *card_id);

/**
 * @brief 更新超时驾驶次数
 * @param count 连续驾驶超过 4 小时的累计次数
 *
 * 法规要求: 连续驾驶超过 4 小时必须休息 ≥20 分钟
 * 行驶记录仪(GB/T 19056)要求记录超时驾驶事件
 */
void svc_lcd_update_overtime_drive_count(uint16_t count);

/**
 * @brief 更新总里程显示
 * @param odo_km     里程整数部分, 单位 km
 * @param odo_rem_m  里程小数部分, 单位 m (0~999 米)
 *
 * 例如 odo_km=12345, odo_rem_m=678 → 显示 "12345.678 km"
 * 将米(小数部分)和千米(整数部分)分开存储可避免浮点数精度问题
 */
void svc_lcd_update_total_mileage(uint32_t odo_km, uint16_t odo_rem_m);

/**
 * @brief 更新主页 GPS 定位信息
 * @param latitude             纬度, 单位: 1/1000000 度
 * @param longitude            经度, 单位: 1/1000000 度
 * @param latitude_direction   'N'=北纬, 'S'=南纬
 * @param longitude_direction  'E'=东经, 'W'=西经
 * @param timestamp            UTC 时间戳(秒)
 *
 * 纬度和经度使用整数微度表示:
 *   39°54'26.352"N = 39907352 (北纬)
 *   116°23'50.604"E = 116238504 (东经)
 *
 * 这样存储可以避免浮点数运算, 加快 MCU 处理速度
 */
void svc_lcd_update_home_location(uint32_t latitude,
                                  uint32_t longitude,
                                  uint8_t latitude_direction,
                                  uint8_t longitude_direction,
                                  uint32_t timestamp);

/**
 * @brief 更新文本消息显示(信息中心页面)
 * @param flag      消息标志(新消息提醒等)
 * @param text_type 文本类型
 * @param utf8_text UTF-8 编码的文本内容
 * @param text_len  文本长度(字节)
 *
 * 消息来源: 平台下发的调度信息、系统通知等
 * 使用 UTF-8 编码以支持中文显示
 */
void svc_lcd_update_text_message(uint8_t flag,
                                 uint8_t text_type,
                                 const char *utf8_text,
                                 uint16_t text_len);


#endif /* APPLICATIONS_SVC_LCD_H_ */
