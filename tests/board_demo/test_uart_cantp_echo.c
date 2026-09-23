#include <assert.h>
#include <stdio.h>
#include <string.h>

#define APP_BOARD_ROLE (0U)
#include "app_host_fixture.h"

/** Verify uint16-LE PC framing and ordered 62-byte CanTp image chunks. */
int main(void)
{
    uint8_t input[132U];
    uint16_t index;

    (void)Test_RunApp;
    (void)Test_InjectSlaveStatus;
    (void)Test_InjectKeepAlive;
    (void)Test_QueueNodeRx;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);

    input[0] = 130U;
    input[1] = 0U;
    for (index = 0U; index < 130U; index++)
    {
        input[index + 2U] = (uint8_t)index;
    }
    Test_InjectUart(input, sizeof(input));

    assert(App_MainFunction(1U) == E_OK);
    assert(Test_NodeTransmitCount == 1U && Test_NodeTxLength[0] == 62U);
    Test_ConfirmNodeTx(E_OK);
    assert(App_MainFunction(2U) == E_OK);
    assert(Test_NodeTransmitCount == 2U && Test_NodeTxLength[1] == 62U);
    Test_ConfirmNodeTx(E_OK);
    assert(App_MainFunction(3U) == E_OK);
    assert(Test_NodeTransmitCount == 3U && Test_NodeTxLength[2] == 6U);
    Test_ConfirmNodeTx(E_OK);
    assert(App_MainFunction(4U) == E_OK);

    assert(memcmp(Test_NodeTxData[0], &input[2], 62U) == 0);
    assert(memcmp(Test_NodeTxData[1], &input[64], 62U) == 0);
    assert(memcmp(Test_NodeTxData[2], &input[126], 6U) == 0);
    assert(g_AppImageLength == 130U && g_AppImageBytesQueued == 130U);
    assert(g_AppImageChunksCompleted == 3U);
    assert(g_AppCanTpTxPending == 0U);

    puts("PASS: MASTER frames one PC image into ordered 62/62/6-byte CanTp chunks.");
    return 0;
}
