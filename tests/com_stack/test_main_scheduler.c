#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BOARD_MODE 3
#define COM_STACK_UNIT_TEST
#include "../../src/main.c"

static char Test_Events[32];
static size_t Test_EventCount;

/** Record CAN completion polling before COM attempts the pending I-PDU. */
void Can_MainFunction_Write(void)
{ Test_Events[Test_EventCount++] = 'W'; }

/** Record CAN receive polling between completion and COM scheduling. */
void Can_MainFunction_Read(void)
{ Test_Events[Test_EventCount++] = 'R'; }

/** Record one COM scheduler call for each elapsed millisecond. */
void Com_MainFunctionTx(void)
{ Test_Events[Test_EventCount++] = 'C'; }

/** Verify zero/one/multiple elapsed ticks and uint32_t counter wraparound. */
int main(void)
{
    uint32_t lastTick = 0U;
    ComStack_ProcessElapsedTicks(0U, &lastTick);
    assert(Test_EventCount == 0U && g_ComStackProcessedTicks == 0U);

    ComStack_ProcessElapsedTicks(1U, &lastTick);
    assert(lastTick == 1U && Test_EventCount == 3U);
    assert(memcmp(Test_Events, "WRC", 3U) == 0);

    ComStack_ProcessElapsedTicks(3U, &lastTick);
    assert(lastTick == 3U && Test_EventCount == 9U);
    assert(memcmp(Test_Events, "WRCWRCWRC", 9U) == 0);
    assert(g_ComStackProcessedTicks == 3U && g_ComStackMaxBacklog == 2U);

    lastTick = UINT32_MAX;
    Test_EventCount = 0U;
    ComStack_ProcessElapsedTicks(1U, &lastTick);
    assert(lastTick == 1U && Test_EventCount == 6U);
    assert(memcmp(Test_Events, "WRCWRC", 6U) == 0);
    assert(g_ComStackProcessedTicks == 5U);

    puts("PASS: 1 ms stack order, catch-up and SysTick wraparound.");
    return 0;
}
