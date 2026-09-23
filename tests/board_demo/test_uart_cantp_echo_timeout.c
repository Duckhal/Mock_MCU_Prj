#include <assert.h>
#include <stdio.h>

#define APP_BOARD_ROLE (0U)
#include "app_host_fixture.h"

/** Verify one failed image chunk gets exactly three application retries. */
int main(void)
{
    static const uint8_t image[] = {1U, 0U, 'X'};
    uint32_t tick;

    (void)Test_RunApp;
    (void)Test_InjectSlaveStatus;
    (void)Test_InjectKeepAlive;
    (void)Test_QueueNodeRx;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    Test_InjectUart(image, sizeof(image));

    assert(App_MainFunction(1U) == E_OK);
    for (tick = 2U; tick <= 5U; tick++)
    {
        Test_ConfirmNodeTx(E_NOT_OK);
        assert(App_MainFunction(tick) == E_OK);
    }
    assert(Test_NodeTransmitCount == 4U);
    assert(g_AppImageRetryCount == APP_IMAGE_MAX_RETRIES);
    assert(g_AppImageChunksDropped == 1U);
    assert(g_AppCanTpTxFailures == 4U);
    assert(g_AppCanTpTxPending == 0U);

    puts("PASS: MASTER drops only the failed chunk after three bounded retries.");
    return 0;
}
