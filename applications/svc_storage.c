/*
 * ============================================================
 *  svc_storage.c — I2C EEPROM 存储服务
 * ============================================================
 *
 * 功能:
 *   通过 I2C 总线读写外部 EEPROM 芯片, 实现数据的掉电保存。
 *
 * 存储的内容:
 *   1) 里程数据 (magic=MILE, 地址 0x0000)
 *      每次里程变化时保存, 确保断电后里程不丢失
 *   2) 设备配置 (magic=CONF, 地址 0x0040)
 *      车牌号、车辆类型、颜色、电话号码等, 通过 LCD 菜单设置
 *
 * 为什么选择 EEPROM?
 *   - 按字节写入, 不像 Flash 需要先擦除整页
 *   - 典型擦写寿命 100 万次 (Flash 通常只有 1 万次)
 *   - 里程频繁写入的场景非常适合
 *
 * 数据完整性设计:
 *   每条记录包含 magic + version + length + checksum
 *   读回后校验通过才认为数据有效, 否则使用默认值
 *   这种模式在嵌入式领域非常经典
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>

#include "app_config.h"
#include "svc_storage.h"

/* ============================================================
 *  EEPROM 硬件参数
 * ============================================================ */

#define SVC_EEPROM_I2C_BUS_NAME      "i2c1"   /* I2C 总线名称 (设备树/驱动注册的名称) */
#define SVC_EEPROM_I2C_ADDR_7BIT     0x50U     /* EEPROM 的 7 位 I2C 地址 */
#define SVC_EEPROM_PAGE_SIZE         32U       /* EEPROM 页大小 (按页写入) */
#define SVC_EEPROM_WRITE_DELAY_MS    5U        /* 页写入后的等待时间 (tWR) */

/*
 * 为什么需要页写入延时?
 *   EEPROM 写入一页后内部需要进行电荷注入, 这个过程需要时间 (tWR)。
 *   在 tWR 期间不能对 EEPROM 进行任何操作, 否则写入可能失败。
 *   不同型号的 EEPROM 的 tWR 不同, 5ms 是一个安全值。
 */

/* ============================================================
 *  里程记录: 地址和格式定义
 * ============================================================ */

#define SVC_STORAGE_MILEAGE_ADDR     0x0000U   /* EEPROM 地址: 里程存储位置 */
#define SVC_STORAGE_MAGIC            0x4D494C45UL /* 魔数 "MILE" (ASCII 编码) */
#define SVC_STORAGE_VERSION          1U          /* 里程记录格式版本 */

/* ============================================================
 *  配置记录: 地址和格式定义
 * ============================================================ */

#define SVC_STORAGE_CONFIG_ADDR          0x0040U   /* EEPROM 地址: 配置存储位置 */
#define SVC_STORAGE_CONFIG_MAGIC         0x434F4E46UL /* 魔数 "CONF" (ASCII 编码) */
#define SVC_STORAGE_CONFIG_VERSION       2U           /* 配置记录格式版本 */

#define SVC_STORAGE_VIN_ADDR             0x0100U
#define SVC_STORAGE_VIN_MAGIC            0x56494E31UL /* "VIN1" */
#define SVC_STORAGE_VIN_VERSION          1U

/*
 * ============================================================
 *  EEPROM 内部记录结构体 (带完整性保护头)
 * ============================================================
 *
 * 为什么要有 svc_storage_mileage_record_t 和
 * svc_storage_mileage_t 两个结构体?
 *
 *   内部记录结构体 (record_t):
 *     包含 magic/version/length/checksum 头部 + 有效数据
 *     这是实际写入 EEPROM 的格式
 *
 *   外部接口结构体 (无 _record 后缀):
 *     只包含有效数据, 不包含头部字段
 *     这是对外 API 使用的结构体, 调用者不需要关心存储格式
 *
 *   这种分离的好处:
 *     如果未来需要修改存储格式(例如增加 version 字段扩展),
 *     只需要修改 record_t, 外部接口不变
 */

/*
 * 里程记录 (内部格式)
 *
 * 内存布局 (写入 EEPROM 的顺序):
 *   [magic:4B][version:2B][length:2B][odo_km:4B][odo_rem_m:2B][reserved:2B][checksum:4B]
 *   总计 = 20 字节
 *
 * checksum 的计算范围: 从 magic 开始到 reserved 结束(共 16 字节)
 * 不包含 checksum 自身, 因为 checksum 不能用自己的值算自己
 */
typedef struct
{
    uint32_t magic;      /* 魔数: SVC_STORAGE_MAGIC (0x4D494C45 = "MILE") */
    uint16_t version;    /* 版本: SVC_STORAGE_VERSION */
    uint16_t length;     /* 记录总长度: sizeof(svc_storage_mileage_record_t) */
    uint32_t odo_km;     /* 里程整数部分, 单位 km */
    uint16_t odo_rem_m;  /* 里程小数部分, 单位 m */
    uint16_t reserved;   /* 保留 */
    uint32_t checksum;   /* 校验和: 从 magic 到 reserved 的累加和 */
} svc_storage_mileage_record_t;

/*
 * 配置记录 (内部格式)
 *
 * 内存布局:
 *   [magic:4B][version:2B][length:2B][phone:12B][class:1B][color:1B]
 *   [plate:13B][reserved:2B][checksum:4B]
 *   总计 = 41 字节
 *
 * 注意版本号是 2 (CONFIG_VERSION=2):
 *   说明这个格式已经有过一次升级。如果未来再次升级,
 *   可以通过版本号做向前/向后兼容处理
 */
typedef struct
{
    uint32_t magic;      /* 魔数: SVC_STORAGE_CONFIG_MAGIC (0x434F4E46 = "CONF") */
    uint16_t version;    /* 版本: SVC_STORAGE_CONFIG_VERSION */
    uint16_t length;     /* 记录总长度 */
    char local_phone[12];                    /* 本地电话号码 */
    uint8_t plate_class;                     /* 车辆类型 */
    uint8_t plate_color;                     /* 车牌颜色 */
    svc_storage_plate_number_t plate_number; /* 车牌信息 (13 字节) */
    uint8_t reserved[2];                     /* 保留 (对齐用) */
    uint32_t checksum;                       /* 校验和 */
} svc_storage_config_record_t;


typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    svc_storage_vin_t vin;
    uint8_t reserved[1];
    uint32_t checksum;
} svc_storage_vin_record_t;


/* I2C 总线设备句柄, 通过 rt_device_find() 获取 */
static struct rt_i2c_bus_device *g_eeprom_i2c_bus = RT_NULL;

/*
 * 里程"影子"副本 (shadow copy):
 *
 * 为什么需要影子副本?
 *   每次里程变化都读写 EEPROM 会显著影响寿命。
 *   影子副本让其他模块可以随时读取当前里程值,
 *   而不需要频繁访问 EEPROM。
 *   只有在里程真正变化时才触发一次 EEPROM 写入。
 *
 * 初始值: 0.000 km (出厂状态)
 */
static svc_storage_mileage_t g_storage_mileage_shadow = {
    .odo_km = 0U,
    .odo_rem_m = 0U,
    .reserved = 0U
};

/*
 * 默认配置 (出厂设置):
 *   本地电话号码: 00000000000 (未设置)
 *   车辆类型: 1 (乘用车)
 *   车牌颜色: 1 (蓝色)
 *   车牌: 未设置 (valid=0)
 *
 * 当 EEPROM 中没有有效配置时使用这个默认值
 */
static const svc_storage_config_t g_default_config = {
    .local_phone = "00000000000",
    .plate_class = 1U,
    .plate_color = 1U,
    .plate_number = {
        .valid = 0U,
        .province_index = 0U,
        .letter = 'A',
        .digits = "00000"
    }
};


/*
 * ============================================================
 *  svc_storage_checksum — 计算校验和
 * ============================================================
 *
 * 算法: 累加和 + 移位混合
 *   sum = (sum << 5) - sum + data[i]
 *   等价于 sum = sum * 31 + data[i]
 *
 * 为什么用这种算法而不是 CRC?
 *   1) 简单, 计算速度快 (适合 MCU)
 *   2) 检测能力足够 (对于小于几十字节的数据, 冲突概率极低)
 *   3) EEPROM 数据损坏通常是整字节坏, 这种校验能检测出来
 *
 * 为什么不用 CRC16/CRC32 硬件模块?
 *   HPM6280 有 CRC 硬件, 但这里处理的数据很小(最多 41 字节),
 *   软件累加和的性能已经足够
 */
static uint32_t svc_storage_checksum(const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0U;
    uint16_t i;

    if (data == RT_NULL) {
        return 0U;
    }

    for (i = 0U; i < len; i++) {
        sum = (sum << 5) - sum + data[i];
    }

    return sum;
}

/*
 * 设置默认配置 (从只读常量复制到可写变量)
 */
static void svc_storage_config_set_default(svc_storage_config_t *config)
{
    if (config == RT_NULL) {
        return;
    }

    rt_memcpy(config, &g_default_config, sizeof(svc_storage_config_t));
}


/*
 * ============================================================
 *  EEPROM 底层读写函数
 * ============================================================
 */

/**
 * @brief 向 EEPROM 写入一页数据
 * @param mem_addr EEPROM 内部地址 (16 位, 支持最大 64KB EEPROM)
 * @param data     待写入的数据
 * @param len      数据长度 (不能超过 SVC_EEPROM_PAGE_SIZE)
 * @return RT_TRUE=成功, RT_FALSE=失败
 *
 * I2C 时序:
 *   起始条件 → 发送设备地址(0x50, 写) → 等待 ACK
 *   → 发送内存地址高字节 → 等待 ACK
 *   → 发送内存地址低字节 → 等待 ACK
 *   → 发送数据字节(1~N) → 等待 ACK
 *   → 停止条件
 *
 * 为什么一次写入不能超过页大小?
 *   EEPROM 内部有页缓冲区, 跨页写入会导致地址自动回卷,
 *   可能会覆盖之前写入的数据
 */
static rt_bool_t svc_eeprom_write_page(uint16_t mem_addr, const uint8_t *data, uint16_t len)
{
    uint8_t buf[2U + SVC_EEPROM_PAGE_SIZE];
    struct rt_i2c_msg msg;

    if ((g_eeprom_i2c_bus == RT_NULL) || (data == RT_NULL) ||
        (len == 0U) || (len > SVC_EEPROM_PAGE_SIZE)) {
        return RT_FALSE;
    }

    /* 构造 I2C 发送缓冲区: [地址高字节][地址低字节][数据...] */
    buf[0] = (uint8_t)(mem_addr >> 8);      /* EEPROM 地址高 8 位 */
    buf[1] = (uint8_t)(mem_addr & 0xFFU);   /* EEPROM 地址低 8 位 */
    rt_memcpy(&buf[2], data, len);

    msg.addr = SVC_EEPROM_I2C_ADDR_7BIT;    /* 从机地址 (7 位) */
    msg.flags = RT_I2C_WR;                  /* 写操作 */
    msg.buf = buf;
    msg.len = (rt_uint16_t)(len + 2U);      /* 地址 2 字节 + 数据 N 字节 */

    if (rt_i2c_transfer(g_eeprom_i2c_bus, &msg, 1) != 1) {
        return RT_FALSE;
    }

    /*
     * 等待页写入完成 (tWR):
     *   EEPROM 在收到停止条件后开始内部写入,
     *   在此期间不响应 ACK。延时确保下次操作时写入已完成。
     */
    rt_thread_mdelay(SVC_EEPROM_WRITE_DELAY_MS);
    return RT_TRUE;
}

/**
 * @brief 向 EEPROM 写入任意长度的数据 (自动处理页边界)
 * @param mem_addr EEPROM 起始地址
 * @param data     待写入的数据
 * @param len      数据长度
 * @return RT_TRUE=成功, RT_FALSE=失败
 *
 * 为什么需要这个"分页"函数?
 *   EEPROM 按页写入, 但调用者可能想写入跨越页边界的连续数据。
 *   这个函数自动处理:
 *   1) 计算当前地址到页尾的剩余字节数
 *   2) 分多次 svc_eeprom_write_page() 写入
 *   3) 确保每次写入不超过页大小, 避免地址回卷
 *
 * 例如: 写入 20 字节, 从地址 0x001E 开始
 *   - 第一次: 写入地址 0x001E~0x001F (2 字节, 到页边界)
 *   - 第二次: 写入地址 0x0020~0x0031 (18 字节, 下一页)
 */
static rt_bool_t svc_eeprom_write(uint16_t mem_addr, const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0U;

    if ((data == RT_NULL) || (len == 0U)) {
        return RT_FALSE;
    }

    while (offset < len) {
        uint16_t page_remain;
        uint16_t write_len;

        /*
         * 计算当前地址到页末尾的剩余空间:
         *   page_remain = PAGE_SIZE - (address % PAGE_SIZE)
         *   例如: addr=0x001E, PAGE_SIZE=32
         *         page_remain = 32 - (30 % 32) = 32 - 30 = 2
         */
        page_remain = (uint16_t)(SVC_EEPROM_PAGE_SIZE -
                      ((mem_addr + offset) % SVC_EEPROM_PAGE_SIZE));
        write_len = (uint16_t)(len - offset);

        /* 不能超过页剩余空间 */
        if (write_len > page_remain) {
            write_len = page_remain;
        }

        if (svc_eeprom_write_page((uint16_t)(mem_addr + offset),
                                  &data[offset],
                                  write_len) != RT_TRUE) {
            return RT_FALSE;
        }

        offset = (uint16_t)(offset + write_len);
    }

    return RT_TRUE;
}

/**
 * @brief 从 EEPROM 读取数据
 * @param mem_addr EEPROM 起始地址
 * @param data     读取缓冲区
 * @param len      读取长度
 * @return RT_TRUE=成功, RT_FALSE=失败
 *
 * I2C 读取时序 (复合操作):
 *   起始 → 发设备地址(写) → 发内存地址(2 字节)
 *   → 重复起始 → 发设备地址(读) → 接收数据(1~N)
 *   → 停止条件
 *
 * 为什么需要"写内存地址→重新起始→读数据"?
 *   标准的 I2C EEPROM 读取协议要求先通过写操作设置内部地址指针,
 *   然后通过读操作读取数据。重新起始条件(而不是停止+起始)
 *   保证总线不会被其他主机抢占。
 */
static rt_bool_t svc_eeprom_read(uint16_t mem_addr, uint8_t *data, uint16_t len)
{
    uint8_t addr_buf[2];
    struct rt_i2c_msg msg;

    if ((g_eeprom_i2c_bus == RT_NULL) || (data == RT_NULL) || (len == 0U)) {
        return RT_FALSE;
    }

    /* 准备内存地址 */
    addr_buf[0] = (uint8_t)(mem_addr >> 8);     /* 地址高字节 */
    addr_buf[1] = (uint8_t)(mem_addr & 0xFFU);  /* 地址低字节 */

    /* 第一段: 写入内存地址 (伪写操作) */
    msg.addr = SVC_EEPROM_I2C_ADDR_7BIT;
    msg.flags = RT_I2C_WR;
    msg.buf = addr_buf;
    msg.len = sizeof(addr_buf);

    if (rt_i2c_transfer(g_eeprom_i2c_bus, &msg, 1) != 1) {
        return RT_FALSE;
    }

    /* 第二段: 读取数据 (重新起始 + 读) */
    msg.addr = SVC_EEPROM_I2C_ADDR_7BIT;
    msg.flags = RT_I2C_RD;
    msg.buf = data;
    msg.len = len;

    return (rt_i2c_transfer(g_eeprom_i2c_bus, &msg, 1) == 1) ? RT_TRUE : RT_FALSE;
}


/*
 * ============================================================
 *  记录有效性校验函数
 * ============================================================
 */

/**
 * @brief 校验里程记录是否有效
 *
 * 校验步骤:
 *   1) magic 必须等于 "MILE"
 *   2) version 必须匹配 (版本兼容检查)
 *   3) length 必须等于结构体大小 (防止不同编译器对齐不一致)
 *   4) checksum 必须等于数据的累加和
 *
 * 只有所有检查都通过才认为数据有效。
 *
 * 为什么需要 length 检查?
 *   如果固件升级后结构体大小变了, 旧的 EEPROM 数据可能不兼容。
 *   length 检查可以检测到这种不匹配。
 */
static rt_bool_t svc_storage_record_is_valid(const svc_storage_mileage_record_t *record)
{
    uint32_t checksum;

    if (record == RT_NULL) {
        return RT_FALSE;
    }

    if ((record->magic != SVC_STORAGE_MAGIC) ||
        (record->version != SVC_STORAGE_VERSION) ||
        (record->length != sizeof(svc_storage_mileage_record_t))) {
        return RT_FALSE;
    }

    /*
     * 计算校验和时排除 checksum 字段本身:
     * sizeof(record) - sizeof(record->checksum)
     * 因为 checksum 是结果, 不能参与自己的计算
     */
    checksum = svc_storage_checksum((const uint8_t *)record,
                                    (uint16_t)(sizeof(svc_storage_mileage_record_t) -
                                               sizeof(record->checksum)));

    return (checksum == record->checksum) ? RT_TRUE : RT_FALSE;
}

/* ----- 电话号码有效性 ----- */
static rt_bool_t svc_storage_phone_digits_valid(const char *digits)
{
    uint8_t i;

    if (digits == RT_NULL) {
        return RT_FALSE;
    }

    /* 11 位数字必须都是 '0'~'9' */
    for (i = 0U; i < 11U; i++) {
        if ((digits[i] < '0') || (digits[i] > '9')) {
            return RT_FALSE;
        }
    }

    /* 第 12 个字符必须是字符串结束符 */
    return (digits[11] == '\0') ? RT_TRUE : RT_FALSE;
}



/* ----- 车辆类型有效性 (1~5) ----- */
static rt_bool_t svc_storage_plate_class_valid(uint8_t plate_class)
{
    return ((plate_class >= 1U) && (plate_class <= 5U)) ? RT_TRUE : RT_FALSE;
}



/* ----- 车牌颜色有效性 (1~5) ----- */
static rt_bool_t svc_storage_plate_color_valid(uint8_t plate_color)
{
    return ((plate_color >= 1U) && (plate_color <= 5U)) ? RT_TRUE : RT_FALSE;
}

/* ----- 车牌号有效性 ----- */
static rt_bool_t svc_storage_plate_number_valid(const svc_storage_plate_number_t *plate_number)
{
    uint8_t i;

    if (plate_number == RT_NULL) {
        return RT_FALSE;
    }

    /* valid 字段只能是 0 或 1 */
    if (plate_number->valid > 1U) {
        return RT_FALSE;
    }

    /* 省份索引必须在有效范围内 (0~30) */
    if (plate_number->province_index >= SVC_STORAGE_PLATE_PROVINCE_COUNT) {
        return RT_FALSE;
    }

    /* 字母必须是大写 A~Z */
    if ((plate_number->letter < 'A') || (plate_number->letter > 'Z')) {
        return RT_FALSE;
    }

    /* 5 位字符可以是数字或大写字母 (中国车牌允许字母和数字混排) */
    for (i = 0U; i < 5U; i++) {
        char ch = plate_number->digits[i];

        if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z'))) {
            return RT_FALSE;
        }
    }


    return (plate_number->digits[5] == '\0') ? RT_TRUE : RT_FALSE;
}

static rt_bool_t svc_storage_vin_valid(const svc_storage_vin_t *vin)
{
    uint8_t i;

    if (vin == RT_NULL) {
        return RT_FALSE;
    }

    if (vin->valid > 1U) {
        return RT_FALSE;
    }

    if (vin->vin[SVC_STORAGE_VIN_LEN] != '\0') {
        return RT_FALSE;
    }

    if (vin->valid == 0U) {
        return RT_TRUE;
    }

    for (i = 0U; i < SVC_STORAGE_VIN_LEN; i++) {
        char ch = vin->vin[i];

        if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z'))) {
            return RT_FALSE;
        }
    }

    return RT_TRUE;
}

static rt_bool_t svc_storage_vin_record_is_valid(const svc_storage_vin_record_t *record)
{
    uint32_t checksum;

    if (record == RT_NULL) {
        return RT_FALSE;
    }

    if ((record->magic != SVC_STORAGE_VIN_MAGIC) ||
        (record->version != SVC_STORAGE_VIN_VERSION) ||
        (record->length != sizeof(svc_storage_vin_record_t))) {
        return RT_FALSE;
    }

    if (svc_storage_vin_valid(&record->vin) != RT_TRUE) {
        return RT_FALSE;
    }

    checksum = svc_storage_checksum((const uint8_t *)record,
                                    (uint16_t)(sizeof(svc_storage_vin_record_t) -
                                               sizeof(record->checksum)));

    return (checksum == record->checksum) ? RT_TRUE : RT_FALSE;
}



/**
 * @brief 校验配置记录是否有效
 *
 * 除了检查 magic/version/length/checksum 外,
 * 还验证所有字段的内容合理性 (电话号格式、车辆类型范围等)
 *
 * 为什么需要字段级验证?
 *   即使 checksum 通过了, 也可能存在逻辑上的无效数据。
 *   例如: 车牌颜色被写入了 6 (超出 1~5 范围),
 *   checksum 可能仍然正确, 但数据是无意义的。
 */
static rt_bool_t svc_storage_config_record_is_valid(const svc_storage_config_record_t *record)
{
    uint32_t checksum;

    if (record == RT_NULL) {
        return RT_FALSE;
    }

    if ((record->magic != SVC_STORAGE_CONFIG_MAGIC) ||
        (record->version != SVC_STORAGE_CONFIG_VERSION) ||
        (record->length != sizeof(svc_storage_config_record_t))) {
        return RT_FALSE;
    }

    /* 内容合理性验证 */
    if (svc_storage_phone_digits_valid(record->local_phone) != RT_TRUE) {
        return RT_FALSE;
    }

    if (svc_storage_plate_class_valid(record->plate_class) != RT_TRUE) {
        return RT_FALSE;
    }

    if (svc_storage_plate_color_valid(record->plate_color) != RT_TRUE) {
        return RT_FALSE;
    }
    if (svc_storage_plate_number_valid(&record->plate_number) != RT_TRUE) {
        return RT_FALSE;
    }

    checksum = svc_storage_checksum((const uint8_t *)record,
                                    (uint16_t)(sizeof(svc_storage_config_record_t) - sizeof(uint32_t)));

    return (checksum == record->checksum) ? RT_TRUE : RT_FALSE;
}



/*
 * ============================================================
 *  线程入口
 * ============================================================
 *
 * 注意: 当前线程函数是空的 (只循环延时)
 * 因为本模块使用"按需读写"模式, 不主动做周期性操作。
 *
 * 为什么还要创建线程?
 *   1) 预留: 未来可能添加周期性任务(如定时保存、健康检查)
 *   2) 一致性: 所有 svc_xxx 模块都有 init + task_start 接口,
 *      即使线程暂时为空, 也保持接口完整
 */
static void svc_storage_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        rt_thread_mdelay(APP_STORAGE_TASK_PERIOD_MS);
    }
}

/*
 * ============================================================
 *  模块初始化
 * ============================================================
 */
int svc_storage_init(void)
{
    /*
     * 查找 I2C 总线设备:
     *   rt_device_find("i2c1") 会在 RT-Thread 的设备管理器中
     *   查找名称为 "i2c1" 的注册设备。
     *
     *   "i2c1" 这个名称需要在 board 层配置(或设备树中定义),
     *   与 SVC_EEPROM_I2C_BUS_NAME 宏保持一致。
     *
     *   如果找不到, 可能是 board 层 I2C 驱动未初始化或名称不匹配。
     *   这里只打印日志, 后续的读写操作会返回失败。
     */
    g_eeprom_i2c_bus = (struct rt_i2c_bus_device *)rt_device_find(SVC_EEPROM_I2C_BUS_NAME);
    if (g_eeprom_i2c_bus == RT_NULL) {
        APP_NON_CAN_LOG("EEPROM: i2c1 not found\r\n");
        return -RT_ERROR;
    }

    APP_NON_CAN_LOG("EEPROM: i2c1 ready\r\n");
    return RT_EOK;
}

int svc_storage_task_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(APP_STORAGE_TASK_NAME,
                              svc_storage_thread_entry,
                              RT_NULL,
                              APP_STORAGE_TASK_STACK_SIZE,
                              APP_STORAGE_TASK_PRIORITY,
                              APP_STORAGE_TASK_TICK);
    if (thread == RT_NULL)
    {
        APP_NON_CAN_LOG("storage thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}

/*
 * ============================================================
 *  里程读写接口
 * ============================================================
 *
 * 这两组函数实现了"带完整性保护"的数据持久化:
 *   load: 从 EEPROM 读取 → 校验 → 返回
 *   save: 封装记录 → 写入 → 读回验证
 *
 * 为什么 save 要读回验证 (Read-Verify)?
 *   虽然 EEPROM 写入出错概率很低, 但行驶记录仪的里程数据
 *   (GB/T 19056 要求) 不能丢失或错误。读回验证确保数据
 *   确实被正确写入了。如果验证失败还会返回 RT_FALSE,
 *   让上层知道保存失败。
 */
rt_bool_t svc_storage_load_mileage(svc_storage_mileage_t *mileage)
{
    svc_storage_mileage_record_t record;

    if (mileage == RT_NULL) {
        return RT_FALSE;
    }

    /*
     * 如果 I2C 总线无效、读取失败或校验失败:
     *   使用 0.000 km 作为默认值
     *
     * 注意: 即使读取失败也返回 RT_TRUE:
     *   因为默认值(0)也是合理的初始值, 调用者不需要
     *   因为 EEPROM 故障而无法启动
     */
    if ((g_eeprom_i2c_bus == RT_NULL) ||
        (svc_eeprom_read(SVC_STORAGE_MILEAGE_ADDR,
                         (uint8_t *)&record,
                         sizeof(record)) != RT_TRUE) ||
        (svc_storage_record_is_valid(&record) != RT_TRUE)) {
        mileage->odo_km = 0U;
        mileage->odo_rem_m = 0U;
        mileage->reserved = 0U;
        g_storage_mileage_shadow = *mileage;

        APP_NON_CAN_LOG("EEPROM: no valid mileage, use 0.000 km\r\n");
        return RT_TRUE;
    }

    /* 读取成功, 返回 EEPROM 中的值 */
    mileage->odo_km = record.odo_km;
    mileage->odo_rem_m = record.odo_rem_m;
    mileage->reserved = record.reserved;

    /* 更新影子副本 */
    g_storage_mileage_shadow = *mileage;

    APP_NON_CAN_LOG("EEPROM: load mileage %lu.%03u km\r\n",
                    (unsigned long)mileage->odo_km,
                    (unsigned int)mileage->odo_rem_m);

    return RT_TRUE;
}

rt_bool_t svc_storage_save_mileage(const svc_storage_mileage_t *mileage)
{
    svc_storage_mileage_record_t record;
    svc_storage_mileage_record_t verify;

    if (mileage == RT_NULL) {
        return RT_FALSE;
    }

    /* 先更新影子副本, 即使后续写入失败也有缓存值 */
    g_storage_mileage_shadow = *mileage;

    if (g_eeprom_i2c_bus == RT_NULL) {
        APP_NON_CAN_LOG("EEPROM: save failed, bus null\r\n");
        return RT_FALSE;
    }

    /* 构造记录: 填充 magic/version/length/checksum 头部 */
    rt_memset(&record, 0, sizeof(record));
    record.magic = SVC_STORAGE_MAGIC;
    record.version = SVC_STORAGE_VERSION;
    record.length = sizeof(record);
    record.odo_km = mileage->odo_km;
    record.odo_rem_m = mileage->odo_rem_m;
    record.reserved = mileage->reserved;
    record.checksum = svc_storage_checksum((const uint8_t *)&record,
                                           (uint16_t)(sizeof(record) -
                                                      sizeof(record.checksum)));

    /* 写入 EEPROM */
    if (svc_eeprom_write(SVC_STORAGE_MILEAGE_ADDR,
                         (const uint8_t *)&record,
                         sizeof(record)) != RT_TRUE) {
        APP_NON_CAN_LOG("EEPROM: save mileage write failed\r\n");
        return RT_FALSE;
    }

    /* 读回验证 */
    if (svc_eeprom_read(SVC_STORAGE_MILEAGE_ADDR,
                        (uint8_t *)&verify,
                        sizeof(verify)) != RT_TRUE) {
        APP_NON_CAN_LOG("EEPROM: save mileage verify read failed\r\n");
        return RT_FALSE;
    }

    if (svc_storage_record_is_valid(&verify) != RT_TRUE) {
        APP_NON_CAN_LOG("EEPROM: save mileage verify invalid\r\n");
        return RT_FALSE;
    }

    APP_NON_CAN_LOG("EEPROM: save mileage %lu.%03u km\r\n",
                    (unsigned long)mileage->odo_km,
                    (unsigned int)mileage->odo_rem_m);

    return RT_TRUE;
}

/*
 * ============================================================
 *  配置读写接口
 * ============================================================
 */

rt_bool_t svc_storage_load_config(svc_storage_config_t *config)
{
    svc_storage_config_record_t record;

    if (config == RT_NULL) {
        return RT_FALSE;
    }

    /*
     * 读取失败或校验不通过:
     *   使用出厂默认配置, 但返回 RT_FALSE 让调用者知道
     *   这是默认值而不是从 EEPROM 读取的
     */
    if ((g_eeprom_i2c_bus == RT_NULL) ||
        (svc_eeprom_read(SVC_STORAGE_CONFIG_ADDR,
                         (uint8_t *)&record,
                         sizeof(record)) != RT_TRUE) ||
        (svc_storage_config_record_is_valid(&record) != RT_TRUE)) {
        svc_storage_config_set_default(config);
        APP_NON_CAN_LOG("EEPROM: no valid config, use defaults\r\n");
        return RT_FALSE;
    }

    /* 解析记录到接口结构体 */
    rt_memcpy(config->local_phone, record.local_phone, sizeof(config->local_phone));
    config->plate_class = record.plate_class;
    config->plate_color = record.plate_color;
    config->plate_number = record.plate_number;

    APP_NON_CAN_LOG("EEPROM: load config phone=%s class=%u color=%u\r\n",
                    config->local_phone,
                    (unsigned int)config->plate_class,
                    (unsigned int)config->plate_color);
    return RT_TRUE;
}

rt_bool_t svc_storage_save_config(const svc_storage_config_t *config)
{
    svc_storage_config_record_t record;
    svc_storage_config_record_t verify;

    /*
     * 参数校验:
     *   保存前检查所有字段的合法性, 防止误写入无效数据
     *   导致配置损坏
     */
    if ((config == RT_NULL) ||
        (svc_storage_phone_digits_valid(config->local_phone) != RT_TRUE) ||
        (svc_storage_plate_class_valid(config->plate_class) != RT_TRUE) ||
        (svc_storage_plate_color_valid(config->plate_color) != RT_TRUE) ||
        (svc_storage_plate_number_valid(&config->plate_number) != RT_TRUE)) {
        return RT_FALSE;
    }


    if (g_eeprom_i2c_bus == RT_NULL) {
        APP_NON_CAN_LOG("EEPROM: save config failed, bus null\r\n");
        return RT_FALSE;
    }

    /* 构造记录 */
    rt_memset(&record, 0, sizeof(record));
    record.magic = SVC_STORAGE_CONFIG_MAGIC;
    record.version = SVC_STORAGE_CONFIG_VERSION;
    record.length = sizeof(record);
    rt_memcpy(record.local_phone, config->local_phone, sizeof(record.local_phone));
    record.plate_class = config->plate_class;
    record.plate_color = config->plate_color;
    record.plate_number = config->plate_number;

    record.checksum = svc_storage_checksum((const uint8_t *)&record,
                                           (uint16_t)(sizeof(record) - sizeof(uint32_t)));

    /* 写入 EEPROM */
    if (svc_eeprom_write(SVC_STORAGE_CONFIG_ADDR,
                         (const uint8_t *)&record,
                         sizeof(record)) != RT_TRUE) {
        APP_NON_CAN_LOG("EEPROM: save config write failed\r\n");
        return RT_FALSE;
    }

    /* 读回验证 */
    if (svc_eeprom_read(SVC_STORAGE_CONFIG_ADDR,
                        (uint8_t *)&verify,
                        sizeof(verify)) != RT_TRUE) {
        APP_NON_CAN_LOG("EEPROM: save config verify read failed\r\n");
        return RT_FALSE;
    }

    if (svc_storage_config_record_is_valid(&verify) != RT_TRUE) {
        APP_NON_CAN_LOG("EEPROM: save config verify invalid\r\n");
        return RT_FALSE;
    }

    APP_NON_CAN_LOG("EEPROM: save config phone=%s class=%u color=%u\r\n",
                    config->local_phone,
                    (unsigned int)config->plate_class,
                    (unsigned int)config->plate_color);
    return RT_TRUE;
}


/*
 * ============================================================
 *  单字段读写接口
 * ============================================================
 *
 * 这些函数都是"读完整配置 → 修改单个字段 → 写回完整配置"
 * 的模式, 即 Read-Modify-Write (RMW)。
 *
 * 为什么不用每个字段独立存一条记录?
 *   1) 节省 EEPROM 空间 — 每条记录要有 magic/version/length/checksum
 *      开销约 12 字节, 如果每个字段独立存储, 开销太大
 *   2) 一致性 — 多个字段一起保存保证它们是同一时刻的配置
 *   3) 简化 — 所有配置放在同一个记录中, 管理简单
 *
 * RMW 的风险:
 *   读取和写入之间如果发生其他写入, 可能会覆盖。
 *   但在本项目中, 配置只通过 LCD 菜单修改, 是串行的, 没问题。
 */

/* ----- 电话号 ----- */
rt_bool_t svc_storage_load_local_phone(svc_storage_phone_t *phone)
{
    svc_storage_config_t config;
    rt_bool_t result;

    if (phone == RT_NULL) {
        return RT_FALSE;
    }

    result = svc_storage_load_config(&config);
    rt_memcpy(phone->digits, config.local_phone, sizeof(phone->digits));
    return result;
}

rt_bool_t svc_storage_save_local_phone(const svc_storage_phone_t *phone)
{
    svc_storage_config_t config;

    if ((phone == RT_NULL) ||
        (svc_storage_phone_digits_valid(phone->digits) != RT_TRUE)) {
        return RT_FALSE;
    }

    (void)svc_storage_load_config(&config);
    rt_memcpy(config.local_phone, phone->digits, sizeof(config.local_phone));

    return svc_storage_save_config(&config);
}


/* ----- 车辆类型 ----- */
rt_bool_t svc_storage_load_plate_class(svc_storage_plate_class_t *plate_class)
{
    svc_storage_config_t config;
    rt_bool_t result;

    if (plate_class == RT_NULL) {
        return RT_FALSE;
    }

    result = svc_storage_load_config(&config);
    plate_class->plate_class = config.plate_class;
    return result;
}

rt_bool_t svc_storage_save_plate_class(const svc_storage_plate_class_t *plate_class)
{
    svc_storage_config_t config;

    if ((plate_class == RT_NULL) ||
        (svc_storage_plate_class_valid(plate_class->plate_class) != RT_TRUE)) {
        return RT_FALSE;
    }

    (void)svc_storage_load_config(&config);
    config.plate_class = plate_class->plate_class;

    return svc_storage_save_config(&config);
}


/* ----- 车牌颜色 ----- */
rt_bool_t svc_storage_load_plate_color(svc_storage_plate_color_t *plate_color)
{
    svc_storage_config_t config;
    rt_bool_t result;

    if (plate_color == RT_NULL) {
        return RT_FALSE;
    }

    result = svc_storage_load_config(&config);
    plate_color->plate_color = config.plate_color;
    return result;
}


rt_bool_t svc_storage_save_plate_color(const svc_storage_plate_color_t *plate_color)
{
    svc_storage_config_t config;

    if ((plate_color == RT_NULL) ||
        (svc_storage_plate_color_valid(plate_color->plate_color) != RT_TRUE)) {
        return RT_FALSE;
    }

    (void)svc_storage_load_config(&config);
    config.plate_color = plate_color->plate_color;




    return svc_storage_save_config(&config);
}

/* ----- 车牌号 ----- */
rt_bool_t svc_storage_load_plate_number(svc_storage_plate_number_t *plate_number)
{
    svc_storage_config_t config;
    rt_bool_t result;

    if (plate_number == RT_NULL) {
        return RT_FALSE;
    }

    result = svc_storage_load_config(&config);
    *plate_number = config.plate_number;
    return result;
}

rt_bool_t svc_storage_save_plate_number(const svc_storage_plate_number_t *plate_number)
{
    svc_storage_config_t config;

    if (svc_storage_plate_number_valid(plate_number) != RT_TRUE) {
        return RT_FALSE;
    }

    (void)svc_storage_load_config(&config);
    config.plate_number = *plate_number;

    return svc_storage_save_config(&config);
}

rt_bool_t svc_storage_load_vin(svc_storage_vin_t *vin)
{
    svc_storage_vin_record_t record;

    if (vin == RT_NULL) {
        return RT_FALSE;
    }

    if ((g_eeprom_i2c_bus == RT_NULL) ||
        (svc_eeprom_read(SVC_STORAGE_VIN_ADDR,
                         (uint8_t *)&record,
                         sizeof(record)) != RT_TRUE) ||
        (svc_storage_vin_record_is_valid(&record) != RT_TRUE)) {
        vin->valid = 0U;
        rt_memset(vin->vin, 0, sizeof(vin->vin));
        return RT_FALSE;
    }

    *vin = record.vin;
    return RT_TRUE;
}

rt_bool_t svc_storage_save_vin(const svc_storage_vin_t *vin)
{
    svc_storage_vin_record_t record;
    svc_storage_vin_record_t verify;

    if (svc_storage_vin_valid(vin) != RT_TRUE) {
        return RT_FALSE;
    }

    if (g_eeprom_i2c_bus == RT_NULL) {
        return RT_FALSE;
    }

    rt_memset(&record, 0, sizeof(record));
    record.magic = SVC_STORAGE_VIN_MAGIC;
    record.version = SVC_STORAGE_VIN_VERSION;
    record.length = sizeof(record);
    record.vin = *vin;

    record.checksum = svc_storage_checksum((const uint8_t *)&record,
                                           (uint16_t)(sizeof(record) -
                                                      sizeof(record.checksum)));

    if (svc_eeprom_write(SVC_STORAGE_VIN_ADDR,
                         (const uint8_t *)&record,
                         sizeof(record)) != RT_TRUE) {
        return RT_FALSE;
    }

    if (svc_eeprom_read(SVC_STORAGE_VIN_ADDR,
                        (uint8_t *)&verify,
                        sizeof(verify)) != RT_TRUE) {
        return RT_FALSE;
    }

    return svc_storage_vin_record_is_valid(&verify);
}

