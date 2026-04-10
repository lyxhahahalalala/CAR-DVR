#ifndef APPLICATIONS_SVC_VEHICLE_IO_H_
#define APPLICATIONS_SVC_VEHICLE_IO_H_

#include "app_types.h"

int svc_vehicle_io_init(void);
int svc_vehicle_io_task_start(void);
const app_vehicle_io_state_t *svc_vehicle_io_get_state(void);

#endif /* APPLICATIONS_SVC_VEHICLE_IO_H_ */