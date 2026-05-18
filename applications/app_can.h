/*
 * app_can.h — CAN 通信底层类型和接口定义
 *
 * 定义 CAN 节点的枚举类型和通信接口。
 * 使用 extern "C" 包裹以支持 C++ 调用。
 *
 * BYTE_t / GeneralUse_t 是字节位操作联合体，
 * 方便按位访问 CAN 帧的 8 字节数据。
 */
#ifndef APPLICATIONS_APP_CAN_H_
#define APPLICATIONS_APP_CAN_H_

#include <stdint.h>
#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** CAN 节点索引枚举 */
typedef enum {
    CAN_NODE1 = 0,   /* can0：主 CAN 节点 */
    CAN_NODE2,       /* can2：辅助 CAN 节点 */
    CAN_NODE3,       /* can3：预留（当前未启用，dev_name=NULL） */
    CAN_NODE_ALL     /* 节点总数 */
} CanNode_e;

/** CAN 工作模式 */
typedef enum {
    NORMAL_MODE = 0,      /* 正常模式 */
    STANDBY_MODE = 1,     /* 待机模式 */
    GO_TO_SLEEP_MODE = 2, /* 睡眠模式 */
} can_mode_t;

/** CAN 接收帧状态 */
typedef enum {
    CAN_FRAME_ST_INIT = 0,   /* 初始状态（未收到帧） */
    CAN_FRAME_ST_RECVED = 1, /* 已收到帧 */
    CAN_FRAME_ST_TMO = 2,    /* 接收超时 */
} CanRxFrameSt_e;

/* 单字节的位域访问结构体（可逐位读写） */
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

/* CAN 标准 8 字节数据帧的位域表示（可逐字节或逐位读写） */
typedef union {
    uint8_t bData[8];                              /* 按字节数组访问 */
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

/** 初始化 CAN 模块（重置状态变量） */
int app_can_init(void);
/** 启动 CAN 系统（配置硬件、创建接收线程和监控线程） */
int app_can_start(void);
/** 在默认节点（节点 0）发送 CAN 帧 */
int app_can_send(uint32_t id, const uint8_t *data, uint8_t len);
/** 在指定节点发送 CAN 帧 */
int app_can_send_ex(uint8_t node_index, uint32_t id, const uint8_t *data, uint8_t len);
/** 获取指定节点的当前生命值 */
rt_uint8_t app_can_get_life(uint8_t node_index);
/** 查询指定节点是否在线（5s 内收到过生命帧） */
rt_bool_t app_can_is_online(uint8_t node_index);


#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_APP_CAN_H_ */
