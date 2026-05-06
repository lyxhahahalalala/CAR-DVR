#include "app_usart_cmd.h"

#include <string.h>
#include "svc_storage.h"
#include "app_config.h"
#include "svc_lcd.h"




typedef struct
{
    uint8_t buf[APP_UART_CMD_RX_BUF_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} app_uart_cmd_ring_t;

static app_uart_cmd_ring_t g_uart_cmd_rx_ring;
static app_uart_cmd_frame_t g_uart_cmd_last_frame;
static app_uart_cmd_tx_fn_t g_uart_cmd_tx_cb = RT_NULL;
static rt_device_t g_uart_cmd_dev = RT_NULL;
static struct rt_semaphore g_uart_cmd_rx_sem;

typedef struct APP_PACKED_STRUCT
{
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
} app_mcu_version_t;

typedef struct APP_PACKED_STRUCT
{
    uint8_t head;
    uint8_t length;
    uint8_t type;
    app_mcu_version_t version;
    uint8_t crc_l;
    uint8_t crc_h;
    uint8_t tail;
} app_mcu_version_frame_t;


static rt_bool_t app_uart_cmd_ring_push(app_uart_cmd_ring_t *ring, uint8_t byte)
{
    if ((ring == RT_NULL) || (ring->count >= APP_UART_CMD_RX_BUF_SIZE)) {
        return RT_FALSE;
    }

    ring->buf[ring->head] = byte;
    ring->head = (uint16_t)((ring->head + 1U) % APP_UART_CMD_RX_BUF_SIZE);
    ring->count++;
    return RT_TRUE;
}

static rt_bool_t app_uart_cmd_ring_pop(app_uart_cmd_ring_t *ring, uint8_t *byte)
{
    if ((ring == RT_NULL) || (byte == RT_NULL) || (ring->count == 0U)) {
        return RT_FALSE;
    }

    *byte = ring->buf[ring->tail];
    ring->tail = (uint16_t)((ring->tail + 1U) % APP_UART_CMD_RX_BUF_SIZE);
    ring->count--;
    return RT_TRUE;
}

static void app_uart_cmd_dump_frame(const uint8_t *buf, uint8_t len)
{
    uint8_t i;

    rt_kprintf("[uart_cmd][frame] len=%u data:", len);
    for (i = 0; i < len; i++) {
        rt_kprintf(" %02X", buf[i]);
    }
    rt_kprintf("\n");
}


static uint16_t app_uart_cmd_crc16(const uint8_t *data, uint16_t len)
{
    static const uint16_t crc16_half_byte[16] = {
        0x0000U, 0x1021U, 0x2042U, 0x3063U,
        0x4084U, 0x50A5U, 0x60C6U, 0x70E7U,
        0x8108U, 0x9129U, 0xA14AU, 0xB16BU,
        0xC18CU, 0xD1ADU, 0xE1CEU, 0xF1EFU
    };

    uint8_t da;
    uint16_t crc = 0U;
    uint16_t i;

    if (data == RT_NULL) {
        return 0U;
    }

    for (i = 0U; i < len; i++) {
        da = (uint8_t)((crc >> 8) >> 4);
        crc <<= 4;
        crc ^= crc16_half_byte[da ^ (data[i] >> 4)];

        da = (uint8_t)((crc >> 8) >> 4);
        crc <<= 4;
        crc ^= crc16_half_byte[da ^ (data[i] & 0x0FU)];
    }

    return crc;
}

static rt_bool_t app_uart_cmd_check_crc(const uint8_t *buf, uint8_t len)
{
    uint16_t calc_crc;
    uint16_t recv_crc;
    uint8_t data_len;

    if ((buf == RT_NULL) || (len < 6U)) {
        return RT_FALSE;
    }

    /*
     * Frame: AA LEN CMD DATA CRC_L CRC_H 55
     * CRC range: DATA only.
     */
    data_len = (uint8_t)(len - 6U);

    calc_crc = app_uart_cmd_crc16(&buf[3], data_len);
    recv_crc = (uint16_t)buf[len - 3U] |
               ((uint16_t)buf[len - 2U] << 8);

    return (calc_crc == recv_crc) ? RT_TRUE : RT_FALSE;
}



static rt_bool_t app_uart_cmd_try_parse_frame(app_uart_cmd_frame_t *frame)
{
    static uint8_t state = 0U;
    static uint8_t temp[APP_UART_CMD_FRAME_MAX_SIZE];
    static uint8_t index = 0U;
    static uint8_t expect_len = 0U;
    uint8_t byte;

    while (app_uart_cmd_ring_pop(&g_uart_cmd_rx_ring, &byte) == RT_TRUE) {
        switch (state) {
        case 0:
            if (byte == APP_UART_CMD_FRAME_HEAD) {
                temp[0] = byte;
                index = 1U;
                expect_len = 0U;
                state = 1U;
            }
            break;

        case 1:
            temp[index++] = byte;
            expect_len = byte;

            if ((expect_len < 3U) || (expect_len > APP_UART_CMD_FRAME_MAX_SIZE)) {
                state = 0U;
                index = 0U;
                expect_len = 0U;
            } else {
                state = 2U;
            }
            break;

        case 2:
            temp[index++] = byte;

            if (index >= expect_len) {
                if ((temp[expect_len - 1U] == APP_UART_CMD_FRAME_TAIL) &&
                    (app_uart_cmd_check_crc(temp, expect_len) == RT_TRUE)) {
                    frame->length = expect_len;
                    rt_memcpy(frame->data, temp, expect_len);

                    state = 0U;
                    index = 0U;
                    expect_len = 0U;
                    return RT_TRUE;
                }

                rt_kprintf("[uart_cmd][crc/tail err] len=%u\n", expect_len);

                state = 0U;
                index = 0U;
                expect_len = 0U;
            }

            break;

        default:
            state = 0U;
            index = 0U;
            expect_len = 0U;
            break;
        }
    }

    return RT_FALSE;
}

static uint16_t app_uart_cmd_build_ack(uint8_t type, uint8_t ret, uint8_t *buf, uint16_t buf_size)
{
    uint16_t crc;

    if ((buf == RT_NULL) || (buf_size < 8U)) {
        return 0U;
    }

    buf[0] = APP_UART_CMD_FRAME_HEAD;
    buf[1] = 8U;
    buf[2] = type;
    buf[3] = ret;
    buf[4] = 0U;

    crc = app_uart_cmd_crc16(&buf[3], 2U);
    buf[5] = (uint8_t)(crc & 0xFFU);
    buf[6] = (uint8_t)(crc >> 8);
    buf[7] = APP_UART_CMD_FRAME_TAIL;

    return 8U;
}




static void app_uart_cmd_parse_version(app_mcu_version_t *version)
{
    const char *p = APP_SOFTWARE_VERSION;
    uint8_t index = 0U;
    uint16_t value = 0U;
    rt_bool_t has_digit = RT_FALSE;
    uint8_t values[3] = {0U, 0U, 0U};

    if (version == RT_NULL) {
        return;
    }

    version->major = 0U;
    version->minor = 0U;
    version->patch = 0U;

    if (p == RT_NULL) {
        return;
    }

    if ((*p == 'V') || (*p == 'v')) {
        p++;
    }

    while ((*p != '\0') && (index < 3U)) {
        if ((*p >= '0') && (*p <= '9')) {
            value = (uint16_t)(value * 10U + (uint16_t)(*p - '0'));
            has_digit = RT_TRUE;
        } else if (*p == '.') {
            if (has_digit == RT_TRUE) {
                values[index++] = (value > 255U) ? 255U : (uint8_t)value;
                value = 0U;
                has_digit = RT_FALSE;
            }
        } else {
            break;
        }

        p++;
    }

    if ((index < 3U) && (has_digit == RT_TRUE)) {
        values[index] = (value > 255U) ? 255U : (uint8_t)value;
    }

    version->major = values[0];
    version->minor = values[1];
    version->patch = values[2];
}


static uint16_t app_uart_cmd_build_mcu_version(uint8_t *buf, uint16_t buf_size)
{
    app_mcu_version_frame_t frame;
    uint16_t crc;

    if ((buf == RT_NULL) || (buf_size < sizeof(frame))) {
        return 0U;
    }

    rt_memset(&frame, 0, sizeof(frame));

    frame.head = APP_UART_CMD_FRAME_HEAD;
    frame.length = (uint8_t)sizeof(frame);
    frame.type = APP_UART_CMD_TYPE_MCU_VERSION;
    app_uart_cmd_parse_version(&frame.version);

    crc = app_uart_cmd_crc16((const uint8_t *)&frame.version,
                             sizeof(frame.version));
    frame.crc_l = (uint8_t)(crc & 0xFFU);
    frame.crc_h = (uint8_t)(crc >> 8);
    frame.tail = APP_UART_CMD_FRAME_TAIL;

    rt_memcpy(buf, &frame, sizeof(frame));
    return (uint16_t)sizeof(frame);
}





static void app_uart_cmd_uart_tx(const uint8_t *data, uint16_t len)
{
    if ((g_uart_cmd_dev == RT_NULL) || (data == RT_NULL) || (len == 0U)) {
        return;
    }

    rt_device_write(g_uart_cmd_dev, 0, data, len);
}

static rt_err_t app_uart_cmd_rx_indicate(rt_device_t dev, rt_size_t size)
{
    uint8_t ch;
    rt_bool_t has_data = RT_FALSE;

    RT_UNUSED(dev);

    while (size-- > 0U) {
        if (rt_device_read(g_uart_cmd_dev, 0, &ch, 1) == 1) {
            app_usart_cmd_push_byte(ch);
            has_data = RT_TRUE;


        } else {
            break;
        }
    }

    if (has_data == RT_TRUE) {
        rt_sem_release(&g_uart_cmd_rx_sem);
    }

    return RT_EOK;
}

static void app_uart_cmd_time_to_hms(uint32_t timestamp,
                                     uint8_t *hour,
                                     uint8_t *minute,
                                     uint8_t *second)
{
    uint32_t local_seconds;
    uint32_t seconds_of_day;

    local_seconds = timestamp + 8U * 3600U;
    seconds_of_day = local_seconds % 86400U;

    if (hour != RT_NULL) {
        *hour = (uint8_t)(seconds_of_day / 3600U);
    }
    if (minute != RT_NULL) {
        *minute = (uint8_t)((seconds_of_day % 3600U) / 60U);
    }
    if (second != RT_NULL) {
        *second = (uint8_t)(seconds_of_day % 60U);
    }
}

static void app_uart_cmd_seconds_to_hms(uint32_t total_seconds,
                                        uint8_t *hour,
                                        uint8_t *minute,
                                        uint8_t *second)
{
    uint32_t seconds_of_day;

    seconds_of_day = total_seconds % 86400U;

    if (hour != RT_NULL) {
        *hour = (uint8_t)(seconds_of_day / 3600U);
    }
    if (minute != RT_NULL) {
        *minute = (uint8_t)((seconds_of_day % 3600U) / 60U);
    }
    if (second != RT_NULL) {
        *second = (uint8_t)(seconds_of_day % 60U);
    }
}

static void app_uart_cmd_bcd_to_str18(const uint8_t raw[9], char str[19])
{
    uint8_t i;

    if ((raw == RT_NULL) || (str == RT_NULL)) {
        return;
    }

    for (i = 0; i < 9U; i++) {
        str[i * 2U]     = (char)('0' + ((raw[i] >> 4) & 0x0FU));
        str[i * 2U + 1] = (char)('0' + (raw[i] & 0x0FU));
    }

    str[18] = '\0';
}

static uint16_t app_uart_cmd_read_le16(const uint8_t *buf)
{
    if (buf == RT_NULL) {
        return 0U;
    }

    return (uint16_t)buf[0]
         | ((uint16_t)buf[1] << 8);
}

static uint32_t app_uart_cmd_read_le32(const uint8_t *buf)
{
    if (buf == RT_NULL) {
        return 0UL;
    }

    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}


static void app_uart_cmd_update_total_mileage(uint16_t speed_kmh_x10, uint32_t timestamp)
{
    static rt_bool_t has_last_sample = RT_FALSE;
    static rt_bool_t storage_loaded = RT_FALSE;
    static uint32_t last_timestamp = 0U;
    static uint32_t odo_km = 0U;
    static uint16_t odo_rem_m = 0U;
    static uint32_t meter_num_acc = 0U; /* unit: 1/36 meter */
    static uint32_t last_saved_odo_km = 0U;

    svc_storage_mileage_t mileage_store;
    uint32_t delta_sec;
    uint32_t add_m;
    uint32_t total_m;

    if (storage_loaded == RT_FALSE) {
        if (svc_storage_load_mileage(&mileage_store) == RT_TRUE) {
            odo_km = mileage_store.odo_km;
            odo_rem_m = mileage_store.odo_rem_m;
            last_saved_odo_km = odo_km;
        } else {
            odo_km = 0U;
            odo_rem_m = 0U;
            last_saved_odo_km = 0U;
        }

        storage_loaded = RT_TRUE;
        svc_lcd_update_total_mileage(odo_km, odo_rem_m);
    }

    if (has_last_sample == RT_FALSE) {
        has_last_sample = RT_TRUE;
        last_timestamp = timestamp;
        return;
    }

    if (timestamp <= last_timestamp) {
        last_timestamp = timestamp;
        return;
    }

    delta_sec = timestamp - last_timestamp;
    last_timestamp = timestamp;

    /* Avoid a large jump after link interruption. */
    if (delta_sec > 5U) {
        return;
    }

    /*
     * speed_kmh_x10 unit: 0.1 km/h
     * distance(m) = speed_kmh_x10 * delta_sec / 36
     * Keep the 1/36 meter remainder to avoid truncation loss.
     */
    meter_num_acc += (uint32_t)speed_kmh_x10 * delta_sec;

    add_m = meter_num_acc / 36U;
    meter_num_acc %= 36U;

    total_m = (uint32_t)odo_rem_m + add_m;
    odo_km += total_m / 1000U;
    odo_rem_m = (uint16_t)(total_m % 1000U);

    svc_lcd_update_total_mileage(odo_km, odo_rem_m);

    if ((odo_km - last_saved_odo_km) >= 1U) {
        mileage_store.odo_km = odo_km;
        mileage_store.odo_rem_m = odo_rem_m;
        mileage_store.reserved = 0U;

        if (svc_storage_save_mileage(&mileage_store) == RT_TRUE) {
            last_saved_odo_km = odo_km;
        }
    }
}





static rt_bool_t app_uart_cmd_parse_soc_status(const app_uart_cmd_frame_t *frame,
                                               app_soc_status_msg_t *msg)
{
    uint8_t status_bits_1;
    uint8_t status_bits_2;

    if ((frame == RT_NULL) || (msg == RT_NULL) || (frame->length < 85U)) {
        return RT_FALSE;
    }

    if ((frame->data[0] != APP_UART_CMD_FRAME_HEAD) ||
        (frame->data[2] != APP_UART_CMD_TYPE_SOC_STATUS)) {
        return RT_FALSE;
    }

    status_bits_1 = frame->data[3];
    status_bits_2 = frame->data[4];

    msg->camera1_status  = (status_bits_1 >> 0) & 0x01U;
    msg->camera2_status  = (status_bits_1 >> 1) & 0x01U;
    msg->camera3_status  = (status_bits_1 >> 2) & 0x01U;
    msg->camera4_status  = (status_bits_1 >> 3) & 0x01U;
    msg->record_status   = (status_bits_1 >> 4) & 0x01U;
    msg->location_status = (status_bits_1 >> 5) & 0x01U;
    msg->ic_card_status  = (status_bits_1 >> 6) & 0x01U;
    msg->udisk_status    = (status_bits_1 >> 7) & 0x01U;

    msg->ip1_connected   = (status_bits_2 >> 0) & 0x01U;
    msg->ip2_connected   = (status_bits_2 >> 1) & 0x01U;
    msg->gsm_connected   = (status_bits_2 >> 2) & 0x01U;
    msg->protect_storage = (status_bits_2 >> 3) & 0x01U;
    msg->sdcard_status   = (status_bits_2 >> 4) & 0x01U;
    msg->sim_status      = (status_bits_2 >> 5) & 0x01U;
    msg->reserved        = 0U;


    rt_memcpy(msg->driver_number, &frame->data[5], 9U);

    msg->total_capacity = app_uart_cmd_read_le32(&frame->data[14]);
    msg->free_capacity  = app_uart_cmd_read_le32(&frame->data[18]);
    msg->driver_time    = app_uart_cmd_read_le32(&frame->data[22]);

    msg->sim_signal = frame->data[26];

    rt_memcpy(msg->phone_number, &frame->data[27], 10U);

    msg->used_satellite = frame->data[37];
    msg->timestamp      = app_uart_cmd_read_le32(&frame->data[38]);
    msg->driver_speed   = app_uart_cmd_read_le16(&frame->data[42]);

    rt_memcpy(msg->terminal_id, &frame->data[44], 30U);

    msg->ip1 = app_uart_cmd_read_le32(&frame->data[74]);
    msg->ip2 = app_uart_cmd_read_le32(&frame->data[78]);

    return RT_TRUE;
}



static void app_usart_cmd_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1) {
        rt_sem_take(&g_uart_cmd_rx_sem, RT_WAITING_FOREVER);
        app_usart_cmd_poll();
    }
}

static void app_usart_cmd_version_tx_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1) {
        app_usart_cmd_send_mcu_version();
        rt_thread_mdelay(APP_UART_CMD_VERSION_TX_PERIOD_MS);
    }
}


int app_usart_cmd_init(void)
{
    rt_err_t result;

    rt_memset(&g_uart_cmd_rx_ring, 0, sizeof(g_uart_cmd_rx_ring));
    rt_memset(&g_uart_cmd_last_frame, 0, sizeof(g_uart_cmd_last_frame));
    g_uart_cmd_tx_cb = RT_NULL;

    result = rt_sem_init(&g_uart_cmd_rx_sem, "uart_cmd", 0, RT_IPC_FLAG_FIFO);
    if (result != RT_EOK) {
        return result;
    }

    g_uart_cmd_dev = rt_device_find(APP_UART_CMD_DEV_NAME);
    if (g_uart_cmd_dev == RT_NULL) {
        return -RT_ERROR;
    }

    result = rt_device_open(g_uart_cmd_dev, RT_DEVICE_FLAG_DMA_RX | RT_DEVICE_FLAG_RDWR);
    if (result != RT_EOK) {
        return result;
    }

    result = rt_device_set_rx_indicate(g_uart_cmd_dev, app_uart_cmd_rx_indicate);
    if (result != RT_EOK) {
        return result;
    }

    app_usart_cmd_set_tx_callback(app_uart_cmd_uart_tx);
    return RT_EOK;
}

int app_usart_cmd_task_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(APP_UART_CMD_TASK_NAME,
                              app_usart_cmd_thread_entry,
                              RT_NULL,
                              APP_UART_CMD_TASK_STACK_SIZE,
                              APP_UART_CMD_TASK_PRIORITY,
                              APP_UART_CMD_TASK_TICK);
    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    app_uart_cmd_update_total_mileage(0U, 1U);

    thread = rt_thread_create(APP_UART_CMD_VERSION_TX_THREAD_NAME,
                              app_usart_cmd_version_tx_thread_entry,
                              RT_NULL,
                              APP_UART_CMD_VERSION_TX_STACK_SIZE,
                              APP_UART_CMD_VERSION_TX_PRIORITY,
                              APP_UART_CMD_VERSION_TX_TICK);
    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    rt_thread_startup(thread);

    return RT_EOK;

}

void app_usart_cmd_set_tx_callback(app_uart_cmd_tx_fn_t tx_cb)
{
    g_uart_cmd_tx_cb = tx_cb;
}

void app_usart_cmd_push_byte(uint8_t byte)
{
    (void)app_uart_cmd_ring_push(&g_uart_cmd_rx_ring, byte);
}

const app_uart_cmd_frame_t *app_usart_cmd_get_last_frame(void)
{
    return &g_uart_cmd_last_frame;
}

rt_bool_t app_usart_cmd_send_ack(uint8_t type, uint8_t ret)
{
    uint8_t ack_buf[8];
    uint16_t ack_len;

    if (g_uart_cmd_tx_cb == RT_NULL) {
        return RT_FALSE;
    }

    ack_len = app_uart_cmd_build_ack(type, ret, ack_buf, sizeof(ack_buf));
    if (ack_len == 0U) {
        return RT_FALSE;
    }

    rt_kprintf("[uart_cmd][ack] %02X %02X %02X %02X %02X %02X %02X %02X\n",
               ack_buf[0], ack_buf[1], ack_buf[2], ack_buf[3],
               ack_buf[4], ack_buf[5], ack_buf[6], ack_buf[7]);

    g_uart_cmd_tx_cb(ack_buf, ack_len);
    return RT_TRUE;
}


rt_bool_t app_usart_cmd_send_mcu_version(void)
{
    uint8_t tx_buf[sizeof(app_mcu_version_frame_t)];
    uint16_t tx_len;

    if (g_uart_cmd_tx_cb == RT_NULL) {
        return RT_FALSE;
    }

    tx_len = app_uart_cmd_build_mcu_version(tx_buf, sizeof(tx_buf));
    if (tx_len == 0U) {
        return RT_FALSE;
    }

    rt_kprintf("[uart_cmd][tx ver] %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
               tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3],
               tx_buf[4], tx_buf[5], tx_buf[6], tx_buf[7], tx_buf[8]);

    g_uart_cmd_tx_cb(tx_buf, tx_len);
    return RT_TRUE;
}




void app_usart_cmd_poll(void)
{
    app_uart_cmd_frame_t frame;

    while (app_uart_cmd_try_parse_frame(&frame) == RT_TRUE) {
        app_soc_status_msg_t msg;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        uint8_t drive_hour;
        uint8_t drive_minute;
        uint8_t drive_second;
        char driver_number_str[19];


        g_uart_cmd_last_frame = frame;

        app_uart_cmd_dump_frame(frame.data, frame.length);

        if (app_uart_cmd_parse_soc_status(&frame, &msg) != RT_TRUE) {
            continue;
        }

        app_uart_cmd_time_to_hms(msg.timestamp, &hour, &minute, &second);
        app_uart_cmd_seconds_to_hms(msg.driver_time, &drive_hour, &drive_minute, &drive_second);
        app_uart_cmd_bcd_to_str18(msg.driver_number, driver_number_str);
        app_uart_cmd_update_total_mileage(msg.driver_speed, msg.timestamp);


        rt_kprintf("[uart_cmd][soc] cam=%u%u%u%u rec=%u loc=%u ic=%u udisk=%u ip=%u%u gsm=%u sd=%u sim_status=%u sim_signal=%u sat=%u total=%luKB free=%luKB ts=0x%08X drv=%lu spd=%u driver=%s\n",

                   msg.camera1_status,
                   msg.camera2_status,
                   msg.camera3_status,
                   msg.camera4_status,
                   msg.record_status,
                   msg.location_status,
                   msg.ic_card_status,
                   msg.udisk_status,
                   msg.ip1_connected,
                   msg.ip2_connected,
                   msg.gsm_connected,
                   msg.sdcard_status,
                   msg.sim_status,
                   msg.sim_signal,

                   msg.used_satellite,
                   (unsigned long)msg.total_capacity,
                   (unsigned long)msg.free_capacity,
                   msg.timestamp,
                   (unsigned long)msg.driver_time,
                   (unsigned int)msg.driver_speed,
                   driver_number_str);




        rt_kprintf("[uart_cmd][time] %02u:%02u:%02u drive=%02u:%02u:%02u\n",
                   hour, minute, second,
                   drive_hour, drive_minute, drive_second);

        svc_lcd_update_home_time(hour, minute, second);
        svc_lcd_update_top_status(msg.used_satellite);
        svc_lcd_update_home_speed(msg.driver_speed);
        svc_lcd_update_drive_time(drive_hour, drive_minute, drive_second);
        svc_lcd_update_card_id(driver_number_str);

    }
}
