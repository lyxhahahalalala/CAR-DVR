#ifndef APPLICATIONS_SVC_CAN_H_
#define APPLICATIONS_SVC_CAN_H_

#include <rtthread.h>

int svc_can_init(void);
int svc_can_task_start(void);
int svc_can_send(rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len);

#endif /* APPLICATIONS_SVC_CAN_H_ */
