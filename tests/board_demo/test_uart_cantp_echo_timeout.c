#include <assert.h>
#include <stdio.h>
#include "app_host_fixture.h"

/** Verify that a lost loopback response cannot leave the app pending forever. */
int main(void)
{
    static const uint8_t byte = 'X';

    (void)Test_RunApp;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    Test_InjectUart(&byte, 1U);
    assert(App_MainFunction(1U) == E_OK);
    assert(App_MainFunction(21U) == E_OK);
    assert(g_AppCanTpEchoPending == 1U);

    assert(App_MainFunction(520U) == E_OK);
    assert(App_MainFunction(521U) == E_NOT_OK);
    assert(g_AppRuntimeStatus == APP_RUNTIME_CANTP_ECHO_TIMEOUT);
    assert(g_AppCanTpEchoPending == 0U);
    assert(g_AppCanTpRxErrors == 1U);

    puts("PASS: missing CanTp echo response fails at the 500 ms boundary.");
    return 0;
}
