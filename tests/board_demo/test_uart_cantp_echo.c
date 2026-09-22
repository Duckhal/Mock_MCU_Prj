#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_host_fixture.h"

/** Verify Tx UART multicast and Rx UART delivery for the shared-ID profile. */
int main(void)
{
    static const uint8_t text[] = "Hello through CanTp";
    static const uint8_t loopbackText[] = "LB";
    uint8_t fullMessage[APP_MAX_LARGE_MESSAGE_LENGTH];
    uint32_t tick;

    (void)Test_RunApp;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    assert(g_AppModeTx == 0U);

    /* A receiver board discards local PC input and never starts CanTp Tx. */
    Test_InjectUart(text, (uint16_t)(sizeof(text) - 1U));
    assert(App_MainFunction(1U) == E_OK);
    assert(App_MainFunction(21U) == E_OK);
    assert(Test_NodeTransmitCount == 0U);
    assert(Test_UartRawTxLength == 0U);

    /* A sender closes the UART chunk after a 20 ms idle gap. */
    g_AppModeTx = 1U;
    Test_InjectUart(text, (uint16_t)(sizeof(text) - 1U));
    assert(App_MainFunction(22U) == E_OK);
    for (tick = 23U; tick < 42U; tick++)
    {
        assert(App_MainFunction(tick) == E_OK);
    }
    assert(Test_NodeTransmitCount == 0U);
    assert(App_MainFunction(42U) == E_OK);
    assert(Test_NodeTransmitCount == 1U);
    assert(Test_NodeTxLength == (sizeof(text) - 1U));
    assert(memcmp(Test_NodeTxData, text, sizeof(text) - 1U) == 0);
    assert(g_AppCanTpTxPending == 1U);
    assert(Test_UartRawTxLength == 0U);

    /* Local final confirmation completes Tx without waiting for a data echo. */
    NodeApp_LastTxResult = E_OK;
    NodeApp_TxConfirmationCount++;
    assert(App_MainFunction(43U) == E_OK);
    assert(g_AppCanTpTxPending == 0U);
    assert(g_AppCanTpTxCompleted == 1U);

    /* A sender consumes but does not print a received N-SDU. */
    memcpy(Test_NodeRxData, Test_NodeTxData, Test_NodeTxLength);
    Test_NodeRxLength = Test_NodeTxLength;
    Test_NodeReadyCount = 1U;
    assert(App_MainFunction(44U) == E_OK);
    assert(g_AppCanTpRxIgnored == 1U);
    assert(Test_UartRawTxLength == 0U);

    /* Every receiver prints the exact reassembled N-SDU to its local PC. */
    g_AppModeTx = 0U;
    memcpy(Test_NodeRxData, text, sizeof(text) - 1U);
    Test_NodeRxLength = sizeof(text) - 1U;
    Test_NodeReadyCount = 1U;
    assert(App_MainFunction(45U) == E_OK);
    assert(Test_UartRawTxLength == (sizeof(text) - 1U));
    assert(memcmp(Test_UartRawTx, text, sizeof(text) - 1U) == 0);
    assert(g_AppCanTpRxUartDeliveries == 1U);
    assert(g_AppCanTpRxMessages == 2U);

    /* A full 62-byte chunk starts immediately on a sender board. */
    for (tick = 0U; tick < sizeof(fullMessage); tick++)
    {
        fullMessage[tick] = (uint8_t)('A' + (tick % 26U));
    }
    g_AppModeTx = 1U;
    Test_InjectUart(fullMessage, sizeof(fullMessage));
    assert(App_MainFunction(46U) == E_OK);
    assert(Test_NodeTransmitCount == 2U);
    assert(Test_NodeTxLength == sizeof(fullMessage));
    assert(memcmp(Test_NodeTxData, fullMessage, sizeof(fullMessage)) == 0);
    assert(g_AppCanTpTxPending == 1U);

    /* The optional fixture keeps the old single-board UART self-echo path. */
    NodeApp_LastTxResult = E_OK;
    NodeApp_TxConfirmationCount++;
    assert(App_MainFunction(47U) == E_OK);
    assert(App_SetCanTpLoopbackMode(2U) == E_NOT_OK);
    assert(App_SetCanTpLoopbackMode(1U) == E_OK);
    g_AppModeTx = 0U;
    Test_InjectUart(loopbackText, sizeof(loopbackText) - 1U);
    assert(App_MainFunction(48U) == E_OK);
    assert(App_MainFunction(68U) == E_OK);
    assert(Test_NodeTransmitCount == 3U);
    memcpy(Test_NodeRxData, loopbackText, sizeof(loopbackText) - 1U);
    Test_NodeRxLength = sizeof(loopbackText) - 1U;
    Test_NodeReadyCount = 1U;
    NodeApp_LastTxResult = E_OK;
    NodeApp_TxConfirmationCount++;
    assert(App_MainFunction(69U) == E_OK);
    assert(Test_UartRawTxLength ==
           ((sizeof(text) - 1U) + (sizeof(loopbackText) - 1U)));
    assert(memcmp(&Test_UartRawTx[sizeof(text) - 1U], loopbackText,
                  sizeof(loopbackText) - 1U) == 0);

    puts("PASS: multi-board CanTp roles and optional UART loopback pass.");
    return 0;
}
