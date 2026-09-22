#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_host_fixture.h"

/** Verify that Rx-mode UART bytes are echoed only after CanTp reassembly. */
int main(void)
{
    static const uint8_t text[] = "Hello through CanTp";
    uint8_t fullMessage[APP_MAX_LARGE_MESSAGE_LENGTH];
    uint32_t tick;

    (void)Test_RunApp;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    assert(g_AppModeTx == 0U);

    Test_InjectUart(text, (uint16_t)(sizeof(text) - 1U));
    assert(App_MainFunction(1U) == E_OK);
    assert(Test_NodeTransmitCount == 0U);
    assert(Test_UartRawTxLength == 0U);

    for (tick = 2U; tick < 21U; tick++)
    {
        assert(App_MainFunction(tick) == E_OK);
    }
    assert(Test_NodeTransmitCount == 0U);
    assert(App_MainFunction(21U) == E_OK);
    assert(Test_NodeTransmitCount == 1U);
    assert(Test_NodeTxLength == (sizeof(text) - 1U));
    assert(memcmp(Test_NodeTxData, text, sizeof(text) - 1U) == 0);
    assert(g_AppCanTpEchoRequests == 1U);
    assert(g_AppCanTpEchoPending == 1U);
    assert(Test_UartRawTxLength == 0U);

    memcpy(Test_NodeRxData, Test_NodeTxData, Test_NodeTxLength);
    Test_NodeRxLength = Test_NodeTxLength;
    Test_NodeReadyCount = 1U;
    assert(App_MainFunction(22U) == E_OK);
    assert(Test_UartRawTxLength == (sizeof(text) - 1U));
    assert(memcmp(Test_UartRawTx, text, sizeof(text) - 1U) == 0);
    assert(g_AppCanTpEchoResponses == 1U);
    assert(g_AppCanTpEchoMismatches == 0U);
    assert(g_AppCanTpEchoPending == 0U);
    assert(g_AppCanTpRxMessages == 1U);

    for (tick = 0U; tick < sizeof(fullMessage); tick++)
    {
        fullMessage[tick] = (uint8_t)('A' + (tick % 26U));
    }
    Test_InjectUart(fullMessage, sizeof(fullMessage));
    assert(App_MainFunction(23U) == E_OK);
    assert(Test_NodeTransmitCount == 2U);
    assert(Test_NodeTxLength == sizeof(fullMessage));
    assert(memcmp(Test_NodeTxData, fullMessage, sizeof(fullMessage)) == 0);

    memcpy(Test_NodeRxData, Test_NodeTxData, Test_NodeTxLength);
    Test_NodeRxLength = Test_NodeTxLength;
    Test_NodeReadyCount = 1U;
    assert(App_MainFunction(24U) == E_OK);
    assert(Test_UartRawTxLength ==
           ((sizeof(text) - 1U) + sizeof(fullMessage)));
    assert(memcmp(&Test_UartRawTx[sizeof(text) - 1U], fullMessage,
                  sizeof(fullMessage)) == 0);
    assert(g_AppCanTpEchoRequests == 2U);
    assert(g_AppCanTpEchoResponses == 2U);

    g_AppModeTx = 1U;
    Test_InjectUart(text, (uint16_t)(sizeof(text) - 1U));
    assert(App_MainFunction(25U) == E_OK);
    assert(Test_NodeTransmitCount == 2U);

    puts("PASS: Rx UART text echoes after CanTp; 62-byte and Tx-ignore pass.");
    return 0;
}
