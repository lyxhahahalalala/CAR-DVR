#ifndef APPLICATIONS_APP_CAN_H_
#define APPLICATIONS_APP_CAN_H_

#include <rtthread.h>

int app_can_init(void);
int app_can_start(void);
int app_can_send(rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len);

#endif /* APPLICATIONS_APP_CAN_H_ */
