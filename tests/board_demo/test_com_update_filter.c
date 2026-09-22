#include <assert.h>
#include <stdio.h>
#include "app_host_fixture.h"

/** Verify repeated 10 ms frames do not restart the receiver blink interval. */
int main(void)
{
    (void)Test_RunApp;
    (void)Test_InjectUart;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    assert(g_AppModeTx == APP_ROLE_RX);

    Test_RxValid = 1U;
    Test_RxValue = COM_LED_COMMAND_ENCODE(COM_LED_MODE_BLINK_500_MS,
                                          COM_LED_STATE_ON);
    Com_RxIndicationCount = 1U;
    assert(App_MainFunction(1U) == E_OK);
    assert(g_AppRxCommands == 1U);
    assert(Test_LedState[0] == 1U);

    Com_RxIndicationCount = 20U;
    assert(App_MainFunction(400U) == E_OK);
    assert(g_AppRxCommands == 2U);
    assert(Test_LedState[0] == 1U);

    Com_RxIndicationCount = 30U;
    assert(App_MainFunction(501U) == E_OK);
    assert(Test_LedState[0] == 0U);
    Com_RxIndicationCount = 40U;
    assert(App_MainFunction(1001U) == E_OK);
    assert(Test_LedState[0] == 1U);

    Test_RxValue = COM_LED_COMMAND_ENCODE(COM_LED_MODE_STEADY,
                                          COM_LED_STATE_OFF);
    Com_RxIndicationCount = 41U;
    assert(App_MainFunction(1002U) == E_OK);
    assert(g_AppLedMode == COM_LED_MODE_STEADY);
    assert(g_AppLedState == COM_LED_STATE_OFF);
    assert(Test_LedState[0] == 0U);

    Test_RxValue = COM_LED_COMMAND_ENCODE(COM_LED_MODE_STEADY,
                                          COM_LED_STATE_ON);
    Com_RxIndicationCount = 42U;
    assert(App_MainFunction(1003U) == E_OK);
    assert(Test_LedState[0] == 1U);
    assert(Test_UartStringCount == 0U && Test_UartRawTxLength == 0U);

    puts("PASS: repeated COM frames preserve 500 ms ON and 500 ms OFF timing.");
    return 0;
}
