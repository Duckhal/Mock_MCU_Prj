#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../board_demo/app_host_fixture.h"

/** Verify catch-up preserves CAN Write, CAN Read and COM Tx call order. */
int main(void)
{
    Test_JumpAt = 61U; /* The next poll observes tick 63 instead. */
    Test_RunApp(70U);
    assert(g_AppStatus == APP_RUNNING && g_AppModeTx == 1U);
    assert(g_AppProcessedTicks == 70U);
    assert(Test_WriteCount == 70U && Test_ReadCount == 70U);
    assert(Test_SchedulerCount == 46U); /* Tx ticks 25 through 70. */
    assert(Test_JumpEventCount == 9U);
    assert(memcmp(Test_JumpEvents, "WRCWRCWRC", 9U) == 0);
    puts("PASS: 1 ms CAN Write/Read/COM order survives a three-tick backlog.");
    return 0;
}
