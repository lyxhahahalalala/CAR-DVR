#include "app_init.h"
#include "app_task.h"

int main(void)
{
    app_framework_init();
    app_task_start();

    return 0;
}
