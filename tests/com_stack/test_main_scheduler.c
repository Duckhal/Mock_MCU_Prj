#include <assert.h>
#include <stdio.h>
#include <string.h>

#define APP_BOARD_ROLE (0U)
#include "../board_demo/app_host_fixture.h"

/** Verify catch-up preserves CAN Write, CAN Read, CanTp and COM call order. */
int main(void)
{
    Test_JumpAt = 61U; /* The next poll observes tick 63 instead. */
    Test_RunApp(71U);
    assert(g_SystemStatus == SYSTEM_RUNNING && g_AppRole == APP_ROLE_MASTER);
    assert(g_SystemProcessedTicks == 70U);
    assert(g_AppMainFunctionCount == 70U);
    assert(Test_PduRInitCount == 1U);
    assert(Test_CanTpLoopbackCount == 0U);
    assert(Test_WriteCount == 70U && Test_ReadCount == 70U &&
           Test_CanTpCount == 70U);
    assert(Test_SchedulerCount == 70U);
    assert(Test_JumpEventCount == 12U);
    assert(memcmp(Test_JumpEvents, "WRTCWRTCWRTC", 12U) == 0);
    puts("PASS: 1 ms CAN Write/Read/CanTp/App/COM order survives a three-tick backlog.");
    return 0;
}
