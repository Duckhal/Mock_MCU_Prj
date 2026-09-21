#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../board_demo/app_host_fixture.h"

/** Verify catch-up preserves CAN Write, CAN Read, CanTp and COM call order. */
int main(void)
{
    Test_JumpAt = 61U; /* The next poll observes tick 63 instead. */
    Test_RunApp(71U);
    assert(g_AppStatus == APP_RUNNING && g_AppModeTx == 1U);
    assert(g_AppProcessedTicks == 70U);
    assert(Test_CanTpLoopbackCount == 1U);
    assert(Test_WriteCount == 70U && Test_ReadCount == 70U &&
           Test_CanTpCount == 70U);
    assert(Test_SchedulerCount == 47U); /* Tx ticks 25 through 71. */
    assert(Test_JumpEventCount == 12U);
    assert(memcmp(Test_JumpEvents, "WRTCWRTCWRTC", 12U) == 0);
    puts("PASS: 1 ms CAN Write/Read/CanTp/COM order survives a three-tick backlog.");
    return 0;
}
