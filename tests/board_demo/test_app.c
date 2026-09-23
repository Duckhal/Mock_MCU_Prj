#include <assert.h>
#include <stdio.h>

#define APP_BOARD_ROLE (0U)
#include "app_host_fixture.h"

/** Verify the Master image selects only its compile-time communication profile. */
int main(void)
{
    (void)Test_RunApp;
    (void)Test_InjectKeepAlive;
    (void)Test_QueueNodeRx;
    (void)Test_ConfirmNodeTx;

    assert(App_MainFunction(1U) == E_NOT_OK);
    assert(g_AppRuntimeStatus == APP_RUNTIME_NOT_INITIALIZED);
    assert(App_Init() == E_NOT_OK);
    assert(g_AppInitError == APP_INIT_ERROR_HARDWARE_NOT_READY);
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);

    assert(g_AppRole == APP_ROLE_MASTER);
    assert(Test_ComTxEnabled[COM_IPDU_TX_KEEPALIVE] == 1U);
    assert(Test_ComTxEnabled[COM_IPDU_TX_SLAVE1_STATUS] == 0U);
    assert(Test_ComTxEnabled[COM_IPDU_TX_SLAVE2_STATUS] == 0U);
    assert(Test_CanTpDataRxEnabled == 1U);
    assert(Test_AdcStartCount == 1U);

    Test_InjectSlaveStatus(0U, COM_SLAVE_STATUS_NORMAL);
    Test_InjectSlaveStatus(1U, COM_SLAVE_STATUS_MASTER_LOST);
    assert(App_MainFunction(1U) == E_OK);
    assert(g_AppOnlineSlaveCount == 2U);
    assert(g_AppSlaveReportedStatus[0] == COM_SLAVE_STATUS_NORMAL);
    assert(g_AppSlaveReportedStatus[1] == COM_SLAVE_STATUS_MASTER_LOST);

    g_AppRole = APP_ROLE_SLAVE1;
    assert(App_MainFunction(2U) == E_NOT_OK);
    assert(g_AppRuntimeStatus == APP_RUNTIME_STATE_CORRUPTION);
    assert((g_AppStateErrorMask & APP_STATE_ERROR_LIFECYCLE) != 0U);

    puts("PASS: MASTER build enables only the Master communication profile.");
    return 0;
}
