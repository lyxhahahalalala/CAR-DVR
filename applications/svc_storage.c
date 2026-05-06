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

static struct rt_i2c_bus_device *g_eeprom_i2c_bus = RT_NULL;

static svc_storage_mileage_t g_storage_mileage_shadow = {
    .odo_km = 0U,
    .odo_rem_m = 0U,
    .reserved = 0U
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
