#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_host_fixture.h"

/** Verify that app.c owns CanTp submission and consumes NodeApp Rx messages. */
int main(void)
{
    uint8_t txData[8] = {0x10U, 0x20U, 0x30U, 0x40U,
                         0x50U, 0x60U, 0x70U, 0x80U};
    uint8_t rxData[5] = {0xA1U, 0xA2U, 0xA3U, 0xA4U, 0xA5U};

    (void)Test_RunApp;
    assert(App_MainFunction(1U) == E_NOT_OK);
    assert(g_AppRuntimeStatus == APP_RUNTIME_NOT_INITIALIZED);
    assert(App_Init() == E_NOT_OK);
    assert(g_AppInitError == APP_INIT_ERROR_HARDWARE_NOT_READY);
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    assert(App_SendLargeMessage(txData, sizeof(txData)) == E_OK);
    assert(Test_NodeTransmitCount == 1U);
    assert(Test_NodeTxLength == sizeof(txData));
    assert(memcmp(Test_NodeTxData, txData, sizeof(txData)) == 0);

    assert(App_SendLargeMessage(NULL, sizeof(txData)) == E_NOT_OK);
    assert(g_AppCanTpTxRequests == 2U && g_AppCanTpTxRejects == 1U);

    memcpy(Test_NodeRxData, rxData, sizeof(rxData));
    Test_NodeRxLength = sizeof(rxData);
    Test_NodeReadyCount = 1U;
    assert(App_MainFunction(1U) == E_OK);
    assert(g_AppCanTpRxMessages == 1U && g_AppCanTpRxErrors == 0U);
    assert(g_AppLastCanTpRxLength == sizeof(rxData));
    assert(memcmp(g_AppLastCanTpRxData, rxData, sizeof(rxData)) == 0);

    App_NextCommand = 0x100U;
    assert(App_MainFunction(2U) == E_NOT_OK);
    assert(g_AppRuntimeStatus == APP_RUNTIME_STATE_CORRUPTION);
    assert(g_AppStateCorruptionCount == 1U);
    assert(g_AppStateErrorMask == APP_STATE_ERROR_TX_COMMAND);
    assert(g_AppLastInvalidTxCommand == 0x100U);

    puts("PASS: app owns CanTp messages and rejects corrupt UART command state.");
    return 0;
}
