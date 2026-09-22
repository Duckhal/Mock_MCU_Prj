#include <assert.h>
#include <stdio.h>
#include "app_host_fixture.h"

/** Verify CanTp failures remain observable without stopping the application. */
int main(void)
{
    static const uint8_t byte = 'X';

    (void)Test_RunApp;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    g_AppModeTx = 1U;
    Test_InjectUart(&byte, 1U);
    assert(App_MainFunction(1U) == E_OK);
    assert(App_MainFunction(21U) == E_OK);
    assert(g_AppCanTpTxPending == 1U);

    NodeApp_LastTxResult = E_NOT_OK;
    NodeApp_TxConfirmationCount++;
    assert(App_MainFunction(22U) == E_OK);
    assert(g_AppRuntimeStatus == APP_RUNTIME_CANTP_TRANSMIT_ERROR);
    assert(g_AppCanTpTxFailures == 1U);
    assert(g_AppCanTpTxPending == 0U);
    g_AppModeTx = 0U;
    assert(App_MainFunction(23U) == E_OK);

    assert(App_Init() == E_OK);
    g_AppModeTx = 1U;
    Test_InjectUart(&byte, 1U);
    assert(App_MainFunction(1U) == E_OK);
    assert(App_MainFunction(21U) == E_OK);
    assert(g_AppCanTpTxPending == 1U);

    assert(App_MainFunction(520U) == E_OK);
    assert(App_MainFunction(521U) == E_OK);
    assert(g_AppRuntimeStatus == APP_RUNTIME_CANTP_TX_TIMEOUT);
    assert(g_AppCanTpTxPending == 0U);
    assert(g_AppCanTpTxFailures == 1U);
    assert(App_MainFunction(522U) == E_OK);

    puts("PASS: CanTp failure and timeout remain recoverable application events.");
    return 0;
}
