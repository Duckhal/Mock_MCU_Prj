#include <assert.h>
#include <stdio.h>
#include <string.h>

#define APP_BOARD_ROLE (1U)
#include "app_host_fixture.h"

/** Verify Slave 1 liveness filtering, LED timing, status and image delivery. */
int main(void)
{
    static const uint8_t text[] = "Slave1 image chunk";

    (void)Test_RunApp;
    (void)Test_InjectSlaveStatus;
    (void)Test_ConfirmNodeTx;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    assert(g_AppRole == APP_ROLE_SLAVE1);
    assert(Test_AdcStartCount == 0U);
    assert(Test_ComTxEnabled[COM_IPDU_TX_SLAVE1_STATUS] == 1U);
    assert(Test_ComTxEnabled[COM_IPDU_TX_KEEPALIVE] == 0U);
    assert(Test_ComTxEnabled[COM_IPDU_TX_SLAVE2_STATUS] == 0U);
    assert(Test_CanTpDataRxEnabled == 1U);
    assert(Test_ComSignalValue[COM_SIGNAL_TX_SLAVE1_STATUS] ==
           COM_SLAVE_STATUS_NORMAL);

    Test_InjectKeepAlive(1U, 6U);
    assert(App_MainFunction(1U) == E_OK);
    assert(g_AppLastAliveTick == 1U && Test_LedState[0] == 1U);

    Test_InjectKeepAlive(1U, 6U);
    assert(App_MainFunction(1900U) == E_OK);
    assert(g_AppLastAliveTick == 1U);
    assert(App_MainFunction(2001U) == E_OK);
    assert(g_AppSlaveStatus == COM_SLAVE_STATUS_MASTER_LOST);
    assert(Test_LedState[0] == 0U);

    Test_InjectKeepAlive(2U, 6U);
    assert(App_MainFunction(2002U) == E_OK);
    assert(g_AppSlaveStatus == COM_SLAVE_STATUS_NORMAL);
    assert(Test_LedState[0] == 1U);
    assert(App_MainFunction(2102U) == E_OK);
    assert(Test_LedState[0] == 0U);

    Test_QueueNodeRx(text, (PduLengthType)(sizeof(text) - 1U));
    assert(App_MainFunction(2103U) == E_OK);
    assert(g_AppCanTpRxUartDeliveries == 1U);
    assert(Test_UartRawTxLength == (sizeof(text) - 1U));
    assert(memcmp(Test_UartRawTx, text, sizeof(text) - 1U) == 0);

    puts("PASS: SLAVE1 filters AliveCounter, blinks, reports and outputs image data.");
    return 0;
}
