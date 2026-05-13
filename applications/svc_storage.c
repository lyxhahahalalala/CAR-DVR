#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>

#include "app_config.h"
#include "svc_storage.h"

#define SVC_EEPROM_I2C_BUS_NAME      "i2c1"
#define SVC_EEPROM_I2C_ADDR_7BIT     0x50U
#define SVC_EEPROM_PAGE_SIZE         32U
#define SVC_EEPROM_WRITE_DELAY_MS    5U

#define SVC_STORAGE_MILEAGE_ADDR     0x0000U
#define SVC_STORAGE_MAGIC            0x4D494C45UL /* MILE */
#define SVC_STORAGE_VERSION          1U

#define SVC_STORAGE_CONFIG_ADDR          0x0040U
#define SVC_STORAGE_CONFIG_MAGIC         0x434F4E46UL /* CONF */
#define SVC_STORAGE_CONFIG_VERSION       1U


typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t odo_km;
    uint16_t odo_rem_m;
    uint16_t reserved;
    uint32_t checksum;
} svc_storage_mileage_record_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    char local_phone[12];
    uint8_t plate_class;
    uint8_t plate_color;
    uint8_t reserved[2];
    uint32_t checksum;
} svc_storage_config_record_t;



static struct rt_i2c_bus_device *g_eeprom_i2c_bus = RT_NULL;

static svc_storage_mileage_t g_storage_mileage_shadow = {
    .odo_km = 0U,
    .odo_rem_m = 0U,
    .reserved = 0U
};

static const svc_storage_config_t g_default_config = {
    .local_phone = "00000000000",
    .plate_class = 1U,
    .plate_color = 1U
};

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

static void svc_storage_config_set_default(svc_storage_config_t *config)
{
    if (config == RT_NULL) {
        return;
    }

    rt_memcpy(config, &g_default_config, sizeof(svc_storage_config_t));
}


static rt_bool_t svc_eeprom_write_page(uint16_t mem_addr, const uint8_t *data, uint16_t len)
{
    uint8_t buf[2U + SVC_EEPROM_PAGE_SIZE];
    struct rt_i2c_msg msg;

    if ((g_eeprom_i2c_bus == RT_NULL) || (data == RT_NULL) ||
        (len == 0U) || (len > SVC_EEPROM_PAGE_SIZE)) {
        return RT_FALSE;
    }

    buf[0] = (uint8_t)(mem_addr >> 8);
    buf[1] = (uint8_t)(mem_addr & 0xFFU);
    rt_memcpy(&buf[2], data, len);

    msg.addr = SVC_EEPROM_I2C_ADDR_7BIT;
    msg.flags = RT_I2C_WR;
    msg.buf = buf;
    msg.len = (rt_uint16_t)(len + 2U);

    if (rt_i2c_transfer(g_eeprom_i2c_bus, &msg, 1) != 1) {
        return RT_FALSE;
    }

    rt_thread_mdelay(SVC_EEPROM_WRITE_DELAY_MS);
    return RT_TRUE;
}

static rt_bool_t svc_eeprom_write(uint16_t mem_addr, const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0U;

    if ((data == RT_NULL) || (len == 0U)) {
        return RT_FALSE;
    }

    while (offset < len) {
        uint16_t page_remain;
        uint16_t write_len;

        page_remain = (uint16_t)(SVC_EEPROM_PAGE_SIZE -
                      ((mem_addr + offset) % SVC_EEPROM_PAGE_SIZE));
        write_len = (uint16_t)(len - offset);

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

static rt_bool_t svc_eeprom_read(uint16_t mem_addr, uint8_t *data, uint16_t len)
{
    uint8_t addr_buf[2];
    struct rt_i2c_msg msg;

    if ((g_eeprom_i2c_bus == RT_NULL) || (data == RT_NULL) || (len == 0U)) {
        return RT_FALSE;
    }

    addr_buf[0] = (uint8_t)(mem_addr >> 8);
    addr_buf[1] = (uint8_t)(mem_addr & 0xFFU);

    msg.addr = SVC_EEPROM_I2C_ADDR_7BIT;
    msg.flags = RT_I2C_WR;
    msg.buf = addr_buf;
    msg.len = sizeof(addr_buf);

    if (rt_i2c_transfer(g_eeprom_i2c_bus, &msg, 1) != 1) {
        return RT_FALSE;
    }

    msg.addr = SVC_EEPROM_I2C_ADDR_7BIT;
    msg.flags = RT_I2C_RD;
    msg.buf = data;
    msg.len = len;

    return (rt_i2c_transfer(g_eeprom_i2c_bus, &msg, 1) == 1) ? RT_TRUE : RT_FALSE;
}

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

    checksum = svc_storage_checksum((const uint8_t *)record,
                                    (uint16_t)(sizeof(svc_storage_mileage_record_t) -
                                               sizeof(record->checksum)));

    return (checksum == record->checksum) ? RT_TRUE : RT_FALSE;
}

static rt_bool_t svc_storage_phone_digits_valid(const char *digits)
{
    uint8_t i;

    if (digits == RT_NULL) {
        return RT_FALSE;
    }

    for (i = 0U; i < 11U; i++) {
        if ((digits[i] < '0') || (digits[i] > '9')) {
            return RT_FALSE;
        }
    }

    return (digits[11] == '\0') ? RT_TRUE : RT_FALSE;
}



static rt_bool_t svc_storage_plate_class_valid(uint8_t plate_class)
{
    return ((plate_class >= 1U) && (plate_class <= 5U)) ? RT_TRUE : RT_FALSE;
}



static rt_bool_t svc_storage_plate_color_valid(uint8_t plate_color)
{
    return ((plate_color >= 1U) && (plate_color <= 5U)) ? RT_TRUE : RT_FALSE;
}

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

    if (svc_storage_phone_digits_valid(record->local_phone) != RT_TRUE) {
        return RT_FALSE;
    }

    if (svc_storage_plate_class_valid(record->plate_class) != RT_TRUE) {
        return RT_FALSE;
    }

    if (svc_storage_plate_color_valid(record->plate_color) != RT_TRUE) {
        return RT_FALSE;
    }

    checksum = svc_storage_checksum((const uint8_t *)record,
                                    (uint16_t)(sizeof(svc_storage_config_record_t) - sizeof(uint32_t)));

    return (checksum == record->checksum) ? RT_TRUE : RT_FALSE;
}



static void svc_storage_thread_entry(void *arg)
{
    RT_UNUSED(arg);

    while (1)
    {
        rt_thread_mdelay(APP_STORAGE_TASK_PERIOD_MS);
    }
}

int svc_storage_init(void)
{
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

rt_bool_t svc_storage_load_mileage(svc_storage_mileage_t *mileage)
{
    svc_storage_mileage_record_t record;

    if (mileage == RT_NULL) {
        return RT_FALSE;
    }

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

    mileage->odo_km = record.odo_km;
    mileage->odo_rem_m = record.odo_rem_m;
    mileage->reserved = record.reserved;

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

    g_storage_mileage_shadow = *mileage;

    if (g_eeprom_i2c_bus == RT_NULL) {
        APP_NON_CAN_LOG("EEPROM: save failed, bus null\r\n");
        return RT_FALSE;
    }

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

    if (svc_eeprom_write(SVC_STORAGE_MILEAGE_ADDR,
                         (const uint8_t *)&record,
                         sizeof(record)) != RT_TRUE) {
        APP_NON_CAN_LOG("EEPROM: save mileage write failed\r\n");
        return RT_FALSE;
    }

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

rt_bool_t svc_storage_load_config(svc_storage_config_t *config)
{
    svc_storage_config_record_t record;

    if (config == RT_NULL) {
        return RT_FALSE;
    }

    if ((g_eeprom_i2c_bus == RT_NULL) ||
        (svc_eeprom_read(SVC_STORAGE_CONFIG_ADDR,
                         (uint8_t *)&record,
                         sizeof(record)) != RT_TRUE) ||
        (svc_storage_config_record_is_valid(&record) != RT_TRUE)) {
        svc_storage_config_set_default(config);
        APP_NON_CAN_LOG("EEPROM: no valid config, use defaults\r\n");
        return RT_FALSE;
    }

    rt_memcpy(config->local_phone, record.local_phone, sizeof(config->local_phone));
    config->plate_class = record.plate_class;
    config->plate_color = record.plate_color;

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

    if ((config == RT_NULL) ||
        (svc_storage_phone_digits_valid(config->local_phone) != RT_TRUE) ||
        (svc_storage_plate_class_valid(config->plate_class) != RT_TRUE) ||
        (svc_storage_plate_color_valid(config->plate_color) != RT_TRUE)) {
        return RT_FALSE;
    }

    if (g_eeprom_i2c_bus == RT_NULL) {
        APP_NON_CAN_LOG("EEPROM: save config failed, bus null\r\n");
        return RT_FALSE;
    }

    rt_memset(&record, 0, sizeof(record));
    record.magic = SVC_STORAGE_CONFIG_MAGIC;
    record.version = SVC_STORAGE_CONFIG_VERSION;
    record.length = sizeof(record);
    rt_memcpy(record.local_phone, config->local_phone, sizeof(record.local_phone));
    record.plate_class = config->plate_class;
    record.plate_color = config->plate_color;
    record.checksum = svc_storage_checksum((const uint8_t *)&record,
                                           (uint16_t)(sizeof(record) - sizeof(uint32_t)));

    if (svc_eeprom_write(SVC_STORAGE_CONFIG_ADDR,
                         (const uint8_t *)&record,
                         sizeof(record)) != RT_TRUE) {
        APP_NON_CAN_LOG("EEPROM: save config write failed\r\n");
        return RT_FALSE;
    }

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


