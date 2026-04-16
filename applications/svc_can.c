#include "svc_can.h"
#include "app_can.h"

int svc_can_init(void)
{
    return app_can_init();
}

int svc_can_task_start(void)
{
    return app_can_start();
}

int svc_can_send(rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len)
{
    return app_can_send(id, data, len);
}

int svc_can_send_ex(rt_uint8_t node_index, rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len)
{
    return app_can_send_ex(node_index, id, data, len);
}
