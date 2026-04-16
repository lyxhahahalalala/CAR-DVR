#ifndef APPLICATIONS_APP_CAN_H_
#define APPLICATIONS_APP_CAN_H_

#include <stdint.h>
#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CAN_NODE1 = 0,
    CAN_NODE2,
    CAN_NODE3,
    CAN_NODE_ALL
} CanNode_e;

typedef enum {
    NORMAL_MODE = 0,
    STANDBY_MODE = 1,
    GO_TO_SLEEP_MODE = 2,
} can_mode_t;

typedef enum {
    CAN_FRAME_ST_INIT = 0,
    CAN_FRAME_ST_RECVED = 1,
    CAN_FRAME_ST_TMO = 2,
} CanRxFrameSt_e;

typedef union {
    struct {
        uint8_t bit1 : 1;
        uint8_t bit2 : 1;
        uint8_t bit3 : 1;
        uint8_t bit4 : 1;
        uint8_t bit5 : 1;
        uint8_t bit6 : 1;
        uint8_t bit7 : 1;
        uint8_t bit8 : 1;
    };
    uint8_t byte;
} BYTE_t;

typedef union {
    uint8_t bData[8];
    struct {
        BYTE_t byte1;
        BYTE_t byte2;
        BYTE_t byte3;
        BYTE_t byte4;
        BYTE_t byte5;
        BYTE_t byte6;
        BYTE_t byte7;
        BYTE_t byte8;
    };
} GeneralUse_t;

int app_can_init(void);
int app_can_start(void);
int app_can_send(rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len);
rt_uint8_t app_can_get_life(uint8_t node_index);
rt_bool_t app_can_is_online(uint8_t node_index);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_APP_CAN_H_ */