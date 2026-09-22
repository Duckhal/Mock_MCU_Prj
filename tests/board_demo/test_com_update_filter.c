#include <assert.h>
#include <stdio.h>
#include "app_host_fixture.h"

/** Verify periodic U=0 COM frames do not repeat one LED application event. */
int main(void)
{
    uint32_t uartBaseline;

    (void)Test_RunApp;
    (void)Test_InjectUart;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    uartBaseline = Test_UartStringCount;

    Test_RxValid = 1U;
    Test_RxValue = 0U;
    Com_RxIndicationCount = 1U;
    Test_RxSignalUpdateCount = 1U;
    assert(App_MainFunction(1U) == E_OK);
    assert(g_AppRxCommands == 1U);
    assert(Test_UartStringCount == uartBaseline + 1U);

    Com_RxIndicationCount = 20U;
    assert(App_MainFunction(2U) == E_OK);
    assert(g_AppRxCommands == 1U);
    assert(Test_UartStringCount == uartBaseline + 1U);

    Test_RxSignalUpdateCount = 2U;
    assert(App_MainFunction(3U) == E_OK);
    assert(g_AppRxCommands == 2U);
    assert(Test_UartStringCount == uartBaseline + 2U);

    puts("PASS: only COM frames with a fresh LED Update Bit reach the app.");
    return 0;
}
