#include <assert.h>
#include <stdio.h>

#define APP_BOARD_ROLE (1U)
#include "app_host_fixture.h"

/** Verify the Tx firmware role remains fixed and updates COM every 10 ms. */
int main(void)
{
    (void)Test_InjectUart;
    Test_AdcValue = 1500U;
    Test_RunApp(161U);

    assert(g_SystemStatus == SYSTEM_RUNNING && g_AppModeTx == APP_ROLE_TX);
    assert(g_SystemProcessedTicks == 160U);
    assert(g_AppMainFunctionCount == 160U);
    assert(Test_PduRInitCount == 1U);
    assert(Test_CanTpLoopbackCount == 0U);
    assert(Test_CanTpLoopbackEnableCount == 0U);
    assert(Test_WriteCount == 160U && Test_ReadCount == 160U);
    assert(Test_SendCount == 16U && g_AppTxSignalUpdates == 16U);
    assert(Test_SentCommands[0] ==
           COM_LED_COMMAND_ENCODE(COM_LED_MODE_BLINK_500_MS,
                                  COM_LED_STATE_ON));
    assert(Test_SentCommands[15] == Test_SentCommands[0]);
    assert(Test_SchedulerCount == 160U);
    assert(Test_ReceiveCount == 0U && g_AppRxCommands == 0U);
    assert(Test_LedState[0] == 0U && Test_LedState[1] == 1U &&
           Test_LedState[2] == 0U);
    assert(Test_UartStringCount == 0U && Test_UartRawTxLength == 0U);
    puts("PASS: compile-time Tx role stays fixed and updates COM every 10 ms.");
    return 0;
}
