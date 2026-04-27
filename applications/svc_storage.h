#ifndef APPLICATIONS_SVC_STORAGE_H_
#define APPLICATIONS_SVC_STORAGE_H_

#include <rtthread.h>
#include <stdint.h>

typedef struct
{
    uint32_t odo_km;
    uint16_t odo_rem_m;
    uint16_t reserved;
} svc_storage_mileage_t;

int svc_storage_init(void);
int svc_storage_task_start(void);

rt_bool_t svc_storage_load_mileage(svc_storage_mileage_t *mileage);
rt_bool_t svc_storage_save_mileage(const svc_storage_mileage_t *mileage);

#endif /* APPLICATIONS_SVC_STORAGE_H_ */
