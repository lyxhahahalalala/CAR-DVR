#ifndef APPLICATIONS_SVC_STORAGE_H_
#define APPLICATIONS_SVC_STORAGE_H_

#include <rtthread.h>
#include <stdint.h>


#define SVC_STORAGE_PLATE_PROVINCE_COUNT 31U


typedef struct
{
    uint32_t odo_km;
    uint16_t odo_rem_m;
    uint16_t reserved;
} svc_storage_mileage_t;

typedef struct
{
    char digits[12]; /* 11位数字 + '\0' */
} svc_storage_phone_t;

typedef struct
{
    uint8_t plate_class; /* 1=乘用车 2=货车 3=专用汽车 4=挂车 5=汽车列车 */
} svc_storage_plate_class_t;

typedef struct
{
    uint8_t plate_color; /* 1=蓝色 2=黄色 3=白色 4=黑色 5=绿色 */
} svc_storage_plate_color_t;

typedef struct
{
    uint8_t valid;          /* 0=未设置 1=已设置 */
    uint8_t province_index; /* 0=京 1=沪 2=湘 3=粤 */
    char letter;            /* A-Z */
    char digits[6];         /* 5位数字 + '\0' */
} svc_storage_plate_number_t;

typedef struct
{
    char local_phone[12];
    uint8_t plate_class;
    uint8_t plate_color;
    svc_storage_plate_number_t plate_number;
} svc_storage_config_t;


int svc_storage_init(void);
int svc_storage_task_start(void);

rt_bool_t svc_storage_load_mileage(svc_storage_mileage_t *mileage);
rt_bool_t svc_storage_save_mileage(const svc_storage_mileage_t *mileage);

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

#endif /* APPLICATIONS_SVC_STORAGE_H_ */
