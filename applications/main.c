#include "app_init.h"
#include "app_task.h"

#include "u8g2_port.h"

int main(void)
{
    app_framework_init();

    u8g2_port_init();
    u8g2_port_test_draw();

    app_task_start();

    return 0;
}

