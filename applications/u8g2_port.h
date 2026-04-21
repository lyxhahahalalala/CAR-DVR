#ifndef U8G2_PORT_H
#define U8G2_PORT_H

#include "u8g2.h"

void u8g2_port_init(void);
void u8g2_port_test_draw(void);
u8g2_t *u8g2_port_get(void);

void u8g2_port_clear_buffer(void);
void u8g2_port_flush_buffer(void);

#endif
