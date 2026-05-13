#include "lcd_ui_time.h"

void lcd_timestamp_to_local_ymdhm(uint32_t timestamp,
                                  uint16_t *year,
                                  uint8_t *month,
                                  uint8_t *day,
                                  uint8_t *hour,
                                  uint8_t *minute)
{
    uint32_t local_seconds;
    uint32_t days;
    uint32_t secs_of_day;
    int32_t z;
    int32_t era;
    uint32_t doe;
    uint32_t yoe;
    uint32_t doy;
    uint32_t mp;
    uint32_t d;
    uint32_t m;
    uint32_t y;

    local_seconds = timestamp + 8U * 3600U;
    days = local_seconds / 86400U;
    secs_of_day = local_seconds % 86400U;

    z = (int32_t)days + 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (uint32_t)(z - era * 146097);
    yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
    y = yoe + (uint32_t)era * 400U;
    doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
    mp = (5U * doy + 2U) / 153U;
    d = doy - (153U * mp + 2U) / 5U + 1U;
    if (mp < 10U) {
        m = mp + 3U;
    } else {
        m = mp - 9U;
        y += 1U;
    }

    if (year != 0) {
        *year = (uint16_t)y;
    }
    if (month != 0) {
        *month = (uint8_t)m;
    }
    if (day != 0) {
        *day = (uint8_t)d;
    }
    if (hour != 0) {
        *hour = (uint8_t)(secs_of_day / 3600U);
    }
    if (minute != 0) {
        *minute = (uint8_t)((secs_of_day % 3600U) / 60U);
    }
}
