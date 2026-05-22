#ifndef APPLICATIONS_SVC_STORAGE_H_
#define APPLICATIONS_SVC_STORAGE_H_

#include <rtthread.h>
#include <stdint.h>

/*
 * ============================================================
 *  存储服务 (Storage Service)
 * ============================================================
 *
 * 功能: 通过 I2C EEPROM 实现数据的非易失性存储
 *
 * 存储内容:
 *   1) 里程数据 (总里程, 断电不丢失)
 *   2) 设备配置 (本地电话号码、车辆类型、车牌信息等)
 *
 * 为什么用 EEPROM 而不是 Flash?
 *   - EEPROM 按字节读写, 不需要擦除整页
 *   - 里程数据频繁写入(每次里程变化都要保存), Flash 的擦写寿命 (10K 次)
 *     可能不够, EEPROM 通常支持 100 万次
 *   - I2C 接口占用引脚少, 适合引脚紧张的 MCU 设计
 *
 * 数据格式:
 *   每条记录包含:
 *   - magic: 魔数, 用于识别数据类型 (MILE / CONF)
 *   - version: 版本号, 用于数据格式升级
 *   - length: 记录长度
 *   - data: 有效数据
 *   - checksum: 校验和, 用于完整性检查
 *   (这种"魔数+版本+长度+校验和"的存储方式是嵌入式数据持久化的经典模式)
 */

/* 中国省份简称数量 (用于车牌省份选择) */
#define SVC_STORAGE_PLATE_PROVINCE_COUNT 31U


/*
 * 里程数据结构体
 *
 * 为什么里程用 odo_km + odo_rem_m 而不是浮点数?
 *   嵌入式 MCU 没有 FPU (或避免使用), 浮点数运算慢且占用栈。
 *   用两个整数表示 "km + m" 可以精确到米, 避免了浮点数精度问题。
 *   例如: 12345.678 km = odo_km=12345, odo_rem_m=678
 *
 * reserved 字段的作用:
 *   用于结构体对齐。如果去掉 reserved, sizeof 可能在不同编译器
 *   下不一致, 影响 EEPROM 读写的兼容性。
 */
typedef struct
{
    uint32_t odo_km;       /* 里程整数部分, 单位: km */
    uint16_t odo_rem_m;    /* 里程小数部分, 单位: m (0~999) */
    uint16_t reserved;     /* 保留, 用于对齐 */
} svc_storage_mileage_t;

/* 电话号码: 11 位数字 + 结尾 '\0' */
typedef struct
{
    char digits[12];
} svc_storage_phone_t;

/*
 * 车辆类型 (依据 GB/T 3730.1):
 *   1 = 乘用车
 *   2 = 货车
 *   3 = 专用汽车
 *   4 = 挂车
 *   5 = 汽车列车 (牵引车+挂车)
 */
typedef struct
{
    uint8_t plate_class;
} svc_storage_plate_class_t;

/*
 * 车牌颜色 (依据 GA 36):
 *   1 = 蓝色 (小型汽车)
 *   2 = 黄色 (大型汽车、挂车)
 *   3 = 白色 (军车、警车)
 *   4 = 黑色 (外籍汽车)
 *   5 = 绿色 (新能源)
 */
typedef struct
{
    uint8_t plate_color;
} svc_storage_plate_color_t;

/*
 * 车牌号结构体
 *
 * 中国车牌格式: [省份简称][字母][5位数字/字母]
 * 例如: 湘A·88888
 *   - province_index = 2 (湘)    ← 查表 lcd_plate_province_codes[]
 *   - letter = 'A'
 *   - digits = "88888"
 *
 * valid: 0=未设置(出厂状态), 1=已设置
 */
typedef struct
{
    uint8_t valid;          /* 0=未设置 1=已设置 */
    uint8_t province_index; /* 省份简称索引 (0=京, 1=沪, 2=湘, 3=粤 ...) */
    char letter;            /* 字母 A-Z */
    char digits[6];         /* 5位数字/字母 + '\0' */
} svc_storage_plate_number_t;


#define SVC_STORAGE_VIN_LEN 17U

typedef struct
{
    uint8_t valid;                         /* 0=未设置, 1=已设置 */
    char vin[SVC_STORAGE_VIN_LEN + 1U];    /* 17位VIN + '\0' */
} svc_storage_vin_t;


/*
 * 完整配置结构体
 * 在 EEPROM 中以 svc_storage_config_record_t 格式存储
 * (带 magic/version/length/checksum 头部)
 */
typedef struct
{
    char local_phone[12];                        /* 本地电话号码 */
    uint8_t plate_class;                         /* 车辆类型 */
    uint8_t plate_color;                         /* 车牌颜色 */
    svc_storage_plate_number_t plate_number;     /* 车牌号 */
} svc_storage_config_t;


/* ---------- 模块初始化 ---------- */
int svc_storage_init(void);
int svc_storage_task_start(void);

/* ---------- 里程读写 ---------- */
rt_bool_t svc_storage_load_mileage(svc_storage_mileage_t *mileage);
rt_bool_t svc_storage_save_mileage(const svc_storage_mileage_t *mileage);

/*
 * ---------- 配置读写 ----------
 *
 * 为什么每个字段都有独立的 load/save 函数?
 *   应用层(LCD UI 或 app_usart_cmd)经常只需要修改配置中的
 *   某一个字段(例如只修改车牌颜色), 不需要读写整个结构体。
 *   独立函数让调用者更清晰, 但内部实现仍然是通过
 *   load_config → modify → save_config 完成的。
 *
 *   这种"读-改-写"的模式有一个隐含问题:
 *   如果两个线程同时 save 不同字段, 后保存的会覆盖先保存的。
 *   在这个项目中, 配置只在 LCD 菜单界面中修改, 不存在并发,
 *   所以是安全的。
 */
rt_bool_t svc_storage_load_config(svc_storage_config_t *config);
rt_bool_t svc_storage_save_config(const svc_storage_config_t *config);

rt_bool_t svc_storage_load_local_phone(svc_storage_phone_t *phone);
rt_bool_t svc_storage_save_local_phone(const svc_storage_phone_t *phone);

rt_bool_t svc_storage_load_plate_class(svc_storage_plate_class_t *plate_class);
rt_bool_t svc_storage_save_plate_class(const svc_storage_plate_class_t *plate_class);

rt_bool_t svc_storage_load_plate_color(svc_storage_plate_color_t *plate_color);
rt_bool_t svc_storage_save_plate_color(const svc_storage_plate_color_t *plate_color);

rt_bool_t svc_storage_load_plate_number(svc_storage_plate_number_t *plate_number);
rt_bool_t svc_storage_save_plate_number(const svc_storage_plate_number_t *plate_number);
rt_bool_t svc_storage_load_vin(svc_storage_vin_t *vin);
rt_bool_t svc_storage_save_vin(const svc_storage_vin_t *vin);


#endif /* APPLICATIONS_SVC_STORAGE_H_ */
