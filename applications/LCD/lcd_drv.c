/*
 * ============================================================
 *  lcd_drv.c — ST7567 LCD 驱动层
 * ============================================================
 *
 * 功能:
 *   这个文件是 LCD 模块的最底层, 直接操作 ST7567 控制器。
 *   通过 GPIO 模拟 SPI 协议与 LCD 通信。
 *
 * 为什么用软件 SPI (GPIO 模拟) 而不是硬件 SPI?
 *   1) 引脚分配: HPM6280 的硬件 SPI 引脚可能被其他外设占用
 *   2) 灵活性: 软件 SPI 可以用任意 GPIO 引脚, PCB 布局更方便
 *   3) 速度: ST7567 的 SPI 最高只支持 ~1MHz, 软件 SPI 足够
 *      而且低速 SPI 可以省去复杂的 DMA 配置
 *
 * 模拟 SPI 参数:
 *   - CPOL=1: 空闲时时钟为高电平
 *   - CPHA=1: 在时钟下降沿采样数据
 *   即 SPI Mode 3
 *
 * 引脚映射:
 *   PA03 -> LCD_RSTB (复位, 低有效)
 *   PC16 -> LCD_LED_A (背光)
 *   PA28 -> LCD_CSN (片选, 低有效)
 *   PA29 -> LCD_A0 (数据/命令选择)
 *   PA30 -> LCD_SCK (SPI 时钟)
 *   PA31 -> LCD_SDA (SPI 数据)
 */
#include "lcd_drv.h"

#include "board.h"
#include "hpm_gpiom_drv.h"
#include "hpm_gpiom_soc_drv.h"
#include "hpm_gpio_drv.h"

/*
 * 引脚宏定义
 *
 * 为什么 GPIO 宏要分 CTRL / INDEX / OE / PIN?
 *   这是 HPM SDK 的 GPIO 驱动接口特点:
 *   gpio_write_pin(ctrl, index, pin, value)
 *   gpio_set_pin_output(ctrl, oe, pin)
 *
 *   CTRL = HPM_GPIO0 (GPIO 控制器基地址)
 *   INDEX = GPIO_DO_GPIOx (数据方向寄存器索引)
 *   OE = GPIO_OE_GPIOx (输出使能寄存器索引)
 *   PIN = 引脚号
 *
 *   IOC 配置: 将 PAD 功能选择为 GPIO
 *   GPIOM 配置: 将引脚分配给 soc_gpio0 控制器
 */

/* ---- 复位引脚 (PA03, 低有效) ---- */
#define LCD_RSTB_GPIO_CTRL      HPM_GPIO0
#define LCD_RSTB_GPIO_INDEX     GPIO_DO_GPIOA
#define LCD_RSTB_GPIO_OE        GPIO_OE_GPIOA
#define LCD_RSTB_PIN            3

/* ---- 背光引脚 (PC16, 高有效) ---- */
#define LCD_BACKLIGHT_GPIO_CTRL  HPM_GPIO0
#define LCD_BACKLIGHT_GPIO_INDEX GPIO_DO_GPIOC
#define LCD_BACKLIGHT_GPIO_OE    GPIO_OE_GPIOC
#define LCD_BACKLIGHT_PIN        16

/* ---- 片选 (PA28, 低有效) ---- */
#define LCD_CSN_GPIO_CTRL       HPM_GPIO0
#define LCD_CSN_GPIO_INDEX      GPIO_DO_GPIOA
#define LCD_CSN_GPIO_OE         GPIO_OE_GPIOA
#define LCD_CSN_PIN             28

/* ---- 数据/命令选择 (PA29, 高=数据, 低=命令) ---- */
#define LCD_A0_GPIO_CTRL        HPM_GPIO0
#define LCD_A0_GPIO_INDEX       GPIO_DO_GPIOA
#define LCD_A0_GPIO_OE          GPIO_OE_GPIOA
#define LCD_A0_PIN              29

/* ---- SPI 时钟 (PA30) ---- */
#define LCD_SCK_GPIO_CTRL       HPM_GPIO0
#define LCD_SCK_GPIO_INDEX      GPIO_DO_GPIOA
#define LCD_SCK_GPIO_OE         GPIO_OE_GPIOA
#define LCD_SCK_PIN             30

/* ---- SPI 数据 (PA31) ---- */
#define LCD_SDA_GPIO_CTRL       HPM_GPIO0
#define LCD_SDA_GPIO_INDEX      GPIO_DO_GPIOA
#define LCD_SDA_GPIO_OE         GPIO_OE_GPIOA
#define LCD_SDA_PIN             31

/*
 * SPI 线序交换开关:
 *   如果 PCB 布线把 SCK 和 SDA 接反了, 设置此宏为 1 可以软件修正
 *   不需要改 PCB。这是非常实用的调试功能。
 */
#define LCD_SW_SPI_SWAP_LINES   0

/*
 * SPI 空闲电平选择:
 *   1 = 空闲时 SCK=高 (CPOL=1, Mode 3)
 *   0 = 空闲时 SCK=低 (CPOL=0, Mode 0)
 *   这里使用 Mode 3, 因为 ST7567 数据手册推荐这种模式
 */
#define LCD_SW_SPI_IDLE_HIGH    1

#if LCD_SW_SPI_SWAP_LINES
/* 交换 SCK 和 SDA 定义 */
#define LCD_CLK_GPIO_CTRL       LCD_SDA_GPIO_CTRL
#define LCD_CLK_GPIO_INDEX      LCD_SDA_GPIO_INDEX
#define LCD_CLK_PIN             LCD_SDA_PIN
#define LCD_DAT_GPIO_CTRL       LCD_SCK_GPIO_CTRL
#define LCD_DAT_GPIO_INDEX      LCD_SCK_GPIO_INDEX
#define LCD_DAT_PIN             LCD_SCK_PIN
#else
/* 正常连接 */
#define LCD_CLK_GPIO_CTRL       LCD_SCK_GPIO_CTRL
#define LCD_CLK_GPIO_INDEX      LCD_SCK_GPIO_INDEX
#define LCD_CLK_PIN             LCD_SCK_PIN
#define LCD_DAT_GPIO_CTRL       LCD_SDA_GPIO_CTRL
#define LCD_DAT_GPIO_INDEX      LCD_SDA_GPIO_INDEX
#define LCD_DAT_PIN             LCD_SDA_PIN
#endif

/*
 * ============================================================
 *  ST7567 命令宏
 * ============================================================
 *
 * 这些命令在 ST7567 数据手册中有详细说明。
 * 初始化序列参考了数据手册的"上电初始化流程"。
 *
 * 命令格式:
 *   单字节命令: 0xXX (通过 A0=0 发送)
 *   带参数命令: 先发命令, 再发参数(也通过 A0=0)
 *   例如 SET_EV(0x81) + EV_value(0x28) 是两个命令字节
 */
#define ST7567_CMD_DISPLAY_ON       0xAF    /* 打开显示 */
#define ST7567_CMD_SET_START_LINE   0x40    /* 设置显示起始行 */
#define ST7567_CMD_SET_PAGE         0xB0    /* 设置页地址 (0xB0~0xB7) */
#define ST7567_CMD_SET_COL_HI       0x10    /* 设置列地址高 4 位 */
#define ST7567_CMD_SET_COL_LO       0x00    /* 设置列地址低 4 位 */
#define ST7567_CMD_SEG_NORMAL       0xA0    /* SEG 输出方向: 正常(正序) */
#define ST7567_CMD_INVERSE_OFF      0xA6    /* 非反转显示 */
#define ST7567_CMD_ALL_PIXEL_OFF    0xA4    /* 正常显示(非全亮) */
#define ST7567_CMD_BIAS_1_9         0xA2    /* 偏压比 1/9 */
#define ST7567_CMD_POWER_ALL_ON     0x2F    /* 开启所有电源(升压+调节器+跟随器) */
#define ST7567_CMD_REG_RATIO        0x20    /* 内部电阻比例设置 */
#define ST7567_CMD_SET_EV           0x81    /* 设置对比度 (后面跟一个参数: EV 值) */
#define ST7567_CMD_SET_BOOSTER      0xF8    /* 倍压模式设置 (后面跟参数) */
#define ST7567_CMD_BOOSTER_X5       0x01    /* 5 倍升压 */
#define ST7567_CMD_COM_NORMAL       0xC0    /* COM 扫描方向: 正常(从上到下) */

/* 初始化参数值 */
#define ST7567_INIT_EV              0x28    /* 对比度值 (根据实际屏调, 越大越黑) */
#define ST7567_INIT_REG_RATIO       0x07    /* 电阻比例 (影响电压/对比度) */

/* 硬件参数 */
#define LCD_COLS                    132     /* 列数 (ST7567 最大 132) */
#define LCD_PAGES                   8       /* 页数 (64 行 / 8 = 8 页) */
#define LCD_HW_COL_OFFSET           0       /* 列偏移 (有些屏需要偏移来居中) */

/*
 * ============================================================
 *  底层硬件初始化
 * ============================================================
 */

/**
 * 初始化 SPI 引脚 (SCK 和 SDA):
 *   1) 通过 IOC 将 PAD 功能选择为 GPIO
 *   2) 通过 GPIOM 将引脚分配给 soc_gpio0
 *   3) 设置引脚为输出模式
 *   4) 设置空闲电平
 *
 * 为什么需要 IOC 和 GPIOM 两步?
 *   HPM 系列芯片的引脚分配非常灵活:
 *   IOC (IO Controller) 决定每个 PAD 的功能(GPIO/外设)
 *   GPIOM (GPIO Manager) 决定哪个 GPIO 控制器接管这个引脚
 *   这是 HPM 芯片特有的两段式引脚配置
 */
static void lcd_drv_spi_hw_init(void)
{
    /* SCK = PA30 */
    HPM_IOC->PAD[IOC_PAD_PA30].FUNC_CTL = IOC_PA30_FUNC_CTL_GPIO_A_30;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_SCK_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_SCK_GPIO_CTRL, LCD_SCK_GPIO_OE, LCD_SCK_PIN);

    /* SDA = PA31 */
    HPM_IOC->PAD[IOC_PAD_PA31].FUNC_CTL = IOC_PA31_FUNC_CTL_GPIO_A_31;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_SDA_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_SDA_GPIO_CTRL, LCD_SDA_GPIO_OE, LCD_SDA_PIN);

    /* 设置空闲电平 */
#if LCD_SW_SPI_IDLE_HIGH
    gpio_write_pin(LCD_CLK_GPIO_CTRL, LCD_CLK_GPIO_INDEX, LCD_CLK_PIN, 1);  /* SCK = 高 */
#else
    gpio_write_pin(LCD_CLK_GPIO_CTRL, LCD_CLK_GPIO_INDEX, LCD_CLK_PIN, 0);  /* SCK = 低 */
#endif
    gpio_write_pin(LCD_DAT_GPIO_CTRL, LCD_DAT_GPIO_INDEX, LCD_DAT_PIN, 0);  /* SDA = 低 */
}

/**
 * 初始化控制引脚 (RST, BL, CS, A0):
 *   这些引脚只作为 GPIO 输出, 不涉及 SPI 通信
 */
static void lcd_drv_ctrl_pins_init(void)
{
    /* RST = PA03 */
    HPM_IOC->PAD[IOC_PAD_PA03].FUNC_CTL = IOC_PA03_FUNC_CTL_GPIO_A_03;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_RSTB_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_OE, LCD_RSTB_PIN);

    /* BL = PC16 */
    HPM_IOC->PAD[IOC_PAD_PC16].FUNC_CTL = IOC_PC16_FUNC_CTL_GPIO_C_16;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOC, LCD_BACKLIGHT_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_BACKLIGHT_GPIO_CTRL, LCD_BACKLIGHT_GPIO_OE, LCD_BACKLIGHT_PIN);

    /* CS = PA28 */
    HPM_IOC->PAD[IOC_PAD_PA28].FUNC_CTL = IOC_PA28_FUNC_CTL_GPIO_A_28;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_CSN_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_CSN_GPIO_CTRL, LCD_CSN_GPIO_OE, LCD_CSN_PIN);

    /* A0 = PA29 */
    HPM_IOC->PAD[IOC_PAD_PA29].FUNC_CTL = IOC_PA29_FUNC_CTL_GPIO_A_29;
    gpiom_set_pin_controller(HPM_GPIOM, GPIOM_ASSIGN_GPIOA, LCD_A0_PIN, gpiom_soc_gpio0);
    gpio_set_pin_output(LCD_A0_GPIO_CTRL, LCD_A0_GPIO_OE, LCD_A0_PIN);
}

/*
 * ============================================================
 *  SPI 通信函数
 * ============================================================
 */

/**
 * 微秒级延时 (用于 SPI 时序控制)
 * 使用 board 层提供的延时函数, 确保精确的 SPI 时钟频率
 */
static void lcd_drv_spi_delay_us(void)
{
    board_delay_us(1);
}

static void lcd_drv_sck_set(rt_bool_t high)
{
    gpio_write_pin(LCD_CLK_GPIO_CTRL, LCD_CLK_GPIO_INDEX, LCD_CLK_PIN, high ? 1 : 0);
}

static void lcd_drv_sda_set(rt_bool_t high)
{
    gpio_write_pin(LCD_DAT_GPIO_CTRL, LCD_DAT_GPIO_INDEX, LCD_DAT_PIN, high ? 1 : 0);
}

/**
 * 通过模拟 SPI 写入一个字节
 *
 * 时序 (Mode 3, CPOL=1, CPHA=1):
 *   空闲: SCK=高
 *   1) SCK=高 → 设置 SDA (数据在 SCK 下降沿前准备好)
 *   2) 延时
 *   3) SCK=低 (下降沿, ST7567 在此边沿采样数据)
 *   4) 延时
 *   5) 重复 8 次, 从 MSB 开始
 *
 * 为什么要从 MSB 开始?
 *   SPI 标准有两种传输顺序: MSB-first 和 LSB-first。
 *   ST7567 和大多数 LCD 控制器使用 MSB-first。
 *   所以先用 byte & 0x80 取最高位。
 *
 * 移位方式:
 *   byte <<= 1 每发送一位就把下一个位移动到最高位,
 *   这样就不需要每次都重新取 byte 的指定位。
 */
static void lcd_drv_spi_write_byte(uint8_t byte)
{
    uint8_t bit;

    for (bit = 0; bit < 8U; bit++) {
#if LCD_SW_SPI_IDLE_HIGH
        /* Mode 3: SCK 高有效, 下降沿采样 */
        lcd_drv_sck_set(RT_TRUE);                       /* SCK = 高 */
        lcd_drv_sda_set((byte & 0x80U) != 0U ? RT_TRUE : RT_FALSE); /* 设置数据 */
        lcd_drv_spi_delay_us();                          /* 等待数据稳定 */
        lcd_drv_sck_set(RT_FALSE);                       /* SCK = 低 (下降沿, 采样) */
        lcd_drv_spi_delay_us();                          /* 保持 */
#else
        /* Mode 0: SCK 低有效, 上升沿采样 */
        lcd_drv_sck_set(RT_FALSE);
        lcd_drv_sda_set((byte & 0x80U) != 0U ? RT_TRUE : RT_FALSE);
        lcd_drv_spi_delay_us();
        lcd_drv_sck_set(RT_TRUE);
        lcd_drv_spi_delay_us();
#endif
        byte <<= 1;  /* 将下一位移到最高位 */
    }

    /* 恢复空闲状态 */
#if LCD_SW_SPI_IDLE_HIGH
    lcd_drv_sck_set(RT_TRUE);   /* 空闲 = 高 */
#else
    lcd_drv_sck_set(RT_FALSE);  /* 空闲 = 低 */
#endif
}

/*
 * ============================================================
 *  ST7567 命令/数据写入函数
 * ============================================================
 */

/**
 * 写入命令:
 *   1) CS = 低 (选中)
 *   2) A0 = 低 (命令模式)
 *   3) SPI 发送命令字节
 *   4) CS = 高 (取消选中)
 */
static void lcd_drv_write_cmd(uint8_t cmd)
{
    lcd_csn_set(RT_TRUE);           /* CS = 低 (选中) */
    lcd_a0_set(RT_FALSE);           /* A0 = 低 (命令) */
    lcd_drv_spi_write_byte(cmd);    /* 发送命令 */
    lcd_csn_set(RT_FALSE);          /* CS = 高 (取消选中) */
}

/**
 * 写入数据缓冲区:
 *   1) CS = 低 (选中)
 *   2) A0 = 高 (数据模式)
 *   3) SPI 连续发送所有数据字节
 *   4) CS = 高 (取消选中)
 *
 * 为什么不每发一个字节就切换一次 CS?
 *   在同一个 CS 有效期间内连续发送多个字节,
 *   ST7567 会自动递增内部的列地址指针。
 *   这样可以高效地填充一行的数据。
 */
static void lcd_drv_write_data_buf(const uint8_t *buf, uint16_t len)
{
    lcd_csn_set(RT_TRUE);           /* CS = 低 (选中) */
    lcd_a0_set(RT_TRUE);            /* A0 = 高 (数据) */

    while (len-- > 0U) {
        lcd_drv_spi_write_byte(*buf++);
    }

    lcd_csn_set(RT_FALSE);          /* CS = 高 (取消选中) */
}

/**
 * 设置 ST7567 的页和列地址 (DDRAM 指针)
 *
 * ST7567 的 DDRAM 组织方式:
 *   以"页 × 列"的方式访问, 每页 8 行(一个字节的 8 位对应 8 行)
 *   Page 0: 行 0~7, Page 1: 行 8~15, ..., Page 7: 行 56~63
 *
 *   设置命令:
 *   1) 页地址: 0xB0 | page (page 0~7)
 *   2) 列高 4 位: 0x10 | (col >> 4)
 *   3) 列低 4 位: 0x00 | (col & 0x0F)
 *
 *   LCD_HW_COL_OFFSET: 有些 LCD 面板的列起始地址不从 0 开始,
 *   可以通过这个偏移来调整显示位置。
 *
 *   注意: hw_col 如果超过 LCD_COLS 会自动减去 LCD_COLS (地址回卷)
 */
static void lcd_drv_set_page_col(uint8_t page, uint8_t col)
{
    uint8_t hw_col = (uint8_t)(col + LCD_HW_COL_OFFSET);

    if (hw_col >= LCD_COLS) {
        hw_col = (uint8_t)(hw_col - LCD_COLS);
    }

    lcd_drv_write_cmd(ST7567_CMD_SET_PAGE | (page & 0x0FU));
    lcd_drv_write_cmd(ST7567_CMD_SET_COL_HI | ((hw_col >> 4) & 0x0FU));
    lcd_drv_write_cmd(ST7567_CMD_SET_COL_LO | (hw_col & 0x0FU));
}

/*
 * ============================================================
 *  ST7567 初始化序列
 * ============================================================
 *
 * 这个序列参考了 ST7567 数据手册的"Power ON Sequence",
 * 严格按照时序要求:
 *
 * 1) 等待 VDD 稳定 (10ms)
 * 2) 硬件复位 (RST 低脉冲)
 * 3) 设置偏压比 (1/9) — 影响 LCD 驱动电压
 * 4) 设置 SEG/COM 扫描方向 — 取决于面板组装方向
 * 5) 设置起始行 = 0
 * 6) 设置电压调节参数 — 影响对比度
 * 7) 开启升压电路 — 等待升压稳定 (120ms)
 * 8) 关闭反转、关闭全亮测试模式
 * 9) 清屏
 * 10) 打开显示
 *
 * 为什么需要 120ms 等待?
 *   ST7567 内部有 DC-DC 升压电路, 从开始升压到电压稳定
 *   需要一定时间。如果在电压稳定前打开显示, 可能出现
 *   显示不均匀或闪烁。
 */
static void lcd_drv_init_seq(void)
{
    rt_thread_mdelay(10);           /* 等待 VDD 稳定 */
    lcd_reset();                    /* 硬件复位 */

    /* 基本设置 */
    lcd_drv_write_cmd(ST7567_CMD_BIAS_1_9);     /* 偏压 1/9 */
    lcd_drv_write_cmd(ST7567_CMD_SEG_NORMAL);   /* SEG 正常方向 */
    lcd_drv_write_cmd(ST7567_CMD_COM_NORMAL);   /* COM 正常方向 */
    lcd_drv_write_cmd(ST7567_CMD_SET_START_LINE | 0x00U);  /* 起始行 = 0 */

    /* 电压/对比度设置 */
    lcd_drv_write_cmd(ST7567_CMD_REG_RATIO | ST7567_INIT_REG_RATIO);  /* 电阻比例 */
    lcd_drv_write_cmd(ST7567_CMD_SET_EV);       /* 对比度命令 */
    lcd_drv_write_cmd(ST7567_INIT_EV);          /* 对比度值 */
    lcd_drv_write_cmd(ST7567_CMD_SET_BOOSTER);  /* 升压命令 */
    lcd_drv_write_cmd(ST7567_CMD_BOOSTER_X5);   /* 5 倍升压 */

    /* 打开电源 */
    lcd_drv_write_cmd(ST7567_CMD_POWER_ALL_ON); /* 升压+调节器+跟随器全部开启 */
    rt_thread_mdelay(120);                      /* 等待电压稳定 (关键!) */

    /* 最终设置 */
    lcd_drv_write_cmd(ST7567_CMD_INVERSE_OFF);  /* 非反转 */
    lcd_drv_write_cmd(ST7567_CMD_ALL_PIXEL_OFF);/* 正常显示模式 */
    lcd_clear();                                 /* 清屏 */
    lcd_drv_write_cmd(ST7567_CMD_DISPLAY_ON);   /* 打开显示 */
}

/*
 * ============================================================
 *  对外接口实现
 * ============================================================
 */
int lcd_drv_init(void)
{
    lcd_drv_ctrl_pins_init();   /* 初始化控制引脚 */
    lcd_drv_spi_hw_init();      /* 初始化 SPI 引脚 */

    lcd_csn_set(RT_FALSE);      /* CS = 高 (取消选中) */
    lcd_a0_set(RT_TRUE);        /* A0 = 高 (默认数据模式) */
    lcd_backlight_off();        /* 先关闭背光 */

    lcd_drv_init_seq();         /* 执行初始化序列 */
    return RT_EOK;
}

void lcd_reset(void)
{
    /* RST = 低 → 等待 20ms → RST = 高 → 等待 5ms */
    gpio_write_pin(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_INDEX, LCD_RSTB_PIN, 0);
    rt_thread_mdelay(20);
    gpio_write_pin(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_INDEX, LCD_RSTB_PIN, 1);
    rt_thread_mdelay(5);
}

void lcd_rst_set(rt_bool_t active)
{
    /* RST 低有效: active=true 表示复位 (引脚输出 0) */
    gpio_write_pin(LCD_RSTB_GPIO_CTRL, LCD_RSTB_GPIO_INDEX, LCD_RSTB_PIN, active ? 0 : 1);
}

void lcd_backlight_on(void)
{
    gpio_write_pin(LCD_BACKLIGHT_GPIO_CTRL, LCD_BACKLIGHT_GPIO_INDEX, LCD_BACKLIGHT_PIN, 1);
}

void lcd_backlight_off(void)
{
    gpio_write_pin(LCD_BACKLIGHT_GPIO_CTRL, LCD_BACKLIGHT_GPIO_INDEX, LCD_BACKLIGHT_PIN, 0);
}

void lcd_a0_set(rt_bool_t is_data)
{
    /* A0=1 → 数据, A0=0 → 命令 */
    gpio_write_pin(LCD_A0_GPIO_CTRL, LCD_A0_GPIO_INDEX, LCD_A0_PIN, is_data ? 1 : 0);
}

void lcd_csn_set(rt_bool_t active)
{
    /* CS 低有效: active=true 表示选中 (引脚输出 0) */
    gpio_write_pin(LCD_CSN_GPIO_CTRL, LCD_CSN_GPIO_INDEX, LCD_CSN_PIN, active ? 0 : 1);
}

void lcd_spi_send_byte(uint8_t byte)
{
    lcd_drv_spi_write_byte(byte);
}

/**
 * 清屏: 将所有页和列的数据写为 0x00
 *
 * 使用 static const 数组作为全零的数据源:
 *   编译器会将 zeros 放在只读区(ROM), 不占栈空间
 *   每次循环发送 LCD_COLS(132) 个 0 字节
 */
void lcd_clear(void)
{
    static const uint8_t zeros[LCD_COLS] = {0};
    uint8_t page;

    for (page = 0; page < LCD_PAGES; page++) {
        lcd_drv_set_page_col(page, 0);
        lcd_drv_write_data_buf(zeros, LCD_COLS);
    }
}

/**
 * 全亮: 将所有页和列的数据写为 0xFF
 *
 * 注意 ones 数组是局部变量(在栈上初始化),
 * 因为写入 0xFF 只需要一次初始化, 之后可以重复使用
 */
void lcd_fill_all(void)
{
    static uint8_t ones[LCD_COLS];
    uint8_t page;
    uint8_t i;

    for (i = 0; i < LCD_COLS; i++) {
        ones[i] = 0xFFU;
    }

    for (page = 0; page < LCD_PAGES; page++) {
        lcd_drv_set_page_col(page, 0);
        lcd_drv_write_data_buf(ones, LCD_COLS);
    }
}

/**
 * 向指定页和列写入数据
 *
 * 这个函数是图形层刷新帧缓冲的核心接口:
 *   lcd_graphics 的 flush 就是通过循环调用此函数
 *   将帧缓冲的各页数据写入 ST7567
 */
void lcd_drv_write_page(uint8_t page, uint8_t col, const uint8_t *buf, uint16_t len)
{
    if ((buf == RT_NULL) || (page >= LCD_PAGES) || (col >= LCD_COLS) || (len == 0U)) {
        return;
    }

    lcd_drv_set_page_col(page, col);
    lcd_drv_write_data_buf(buf, len);
}
