#include <assert.h>
#include <stdio.h>

#define APP_BOARD_ROLE (0U)
#define SYSTEM_ENABLE_UART_CANTP_LOOPBACK (1U)
#include "app_host_fixture.h"

/** Verify main enables both CAN and application sides of the UART fixture. */
int main(void)
{
    (void)Test_InjectUart;
    Test_RunApp(2U);

    assert(g_SystemStatus == SYSTEM_RUNNING);
    assert(Test_CanTpLoopbackEnableCount == 1U);
    assert(g_AppCanTpInternalLoopback == 1U);
    puts("PASS: main enables the complete single-board UART/CanTp fixture.");
    return 0;
}
