#include "../system/System.h"

int main(void)
{
    System_Init();

    for (;;)
    {
        System_RunTask();
    }
}
