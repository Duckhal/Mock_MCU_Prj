#include <assert.h>
#include <stdio.h>
#include "app_host_fixture.h"

/** Verify SW2 changes roles, SW3 updates COM and only Rx applies LED data. */
int main(void)
{
    Test_RxAt = 145U;
    Test_RxValue = 2U;
    Test_RunApp(160U);

    assert(g_AppStatus == APP_RUNNING && g_AppModeTx == 0U);
    assert(g_AppProcessedTicks == 160U);
    assert(Test_WriteCount == 160U && Test_ReadCount == 160U);
    assert(Test_SendCount == 2U && g_AppTxSignalUpdates == 2U);
    assert(Test_SentCommands[0] == 0U && Test_SentCommands[1] == 1U);
    assert(Test_SchedulerCount == 105U); /* Tx ticks 25 through 129. */
    assert(Test_ReceiveCount == 1U && g_AppRxCommands == 1U);
    assert(Test_LedState[0] == 1U && Test_LedState[1] == 0U &&
           Test_LedState[2] == 0U);
    assert(strcmp(Test_LastUart, "RX LED BLUE\r\n") == 0);
    puts("PASS: app SW2/SW3 use COM and only Rx applies received LED command.");
    return 0;
}
