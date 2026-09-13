#include "../drivers/can/test/Can_LoopbackTest.h"

/** Run the current CAN-layer board test once, then preserve its debug state. */
int main(void)
{
    Can_LoopbackTest_Run();

    /* Inspect g_CanLoopbackTestResult in the debugger.  A green LED means
     * the internal FlexCAN loopback payload matched byte-for-byte. */
    for (;;)
    {
    }
}
