#include <assert.h>
#include <stdio.h>

#define APP_BOARD_ROLE (2U)
#include "app_host_fixture.h"

/** Verify Slave 2 has a fixed status route and does not join CanTp image Rx. */
int main(void)
{
    static const uint8_t data[] = {0x11U, 0x22U, 0x33U};

    (void)Test_RunApp;
    (void)Test_InjectSlaveStatus;
    (void)Test_ConfirmNodeTx;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    assert(g_AppRole == APP_ROLE_SLAVE2);
    assert(Test_AdcStartCount == 0U);
    assert(Test_ComTxEnabled[COM_IPDU_TX_SLAVE2_STATUS] == 1U);
    assert(Test_ComTxEnabled[COM_IPDU_TX_KEEPALIVE] == 0U);
    assert(Test_ComTxEnabled[COM_IPDU_TX_SLAVE1_STATUS] == 0U);
    assert(Test_CanTpDataRxEnabled == 0U);
    assert(Test_ComSignalValue[COM_SIGNAL_TX_SLAVE2_STATUS] ==
           COM_SLAVE_STATUS_NORMAL);

    Test_InjectKeepAlive(9U, 3U);
    assert(App_MainFunction(10U) == E_OK);
    assert(Test_LedState[0] == 1U);

    Test_QueueNodeRx(data, sizeof(data));
    assert(App_MainFunction(11U) == E_OK);
    assert(g_AppCanTpRxIgnored == 1U);
    assert(g_AppCanTpRxUartDeliveries == 0U);
    assert(Test_UartRawTxLength == 0U);

    puts("PASS: SLAVE2 build fixes its status route and disables image reception.");
    return 0;
}
