#include <assert.h>
#include <stdio.h>

#define APP_BOARD_ROLE (0U)
#include "app_host_fixture.h"

/** Verify all seven ADC rate ranges and the fastest KeepAlive application rate. */
int main(void)
{
    static const uint16_t samples[] =
    {
        0U, 585U, 586U, 1170U, 1171U, 1755U, 1756U,
        2340U, 2341U, 2925U, 2926U, 3510U, 3511U, 4095U
    };
    static const uint8_t expected[] =
    {
        0U, 0U, 1U, 1U, 2U, 2U, 3U,
        3U, 4U, 4U, 5U, 5U, 6U, 6U
    };
    uint32_t index;

    (void)Test_RunApp;
    (void)Test_InjectSlaveStatus;
    (void)Test_InjectKeepAlive;
    (void)Test_QueueNodeRx;
    (void)Test_ConfirmNodeTx;
    assert(App_HardwareInit() == E_OK);

    for (index = 0U; index < (sizeof(samples) / sizeof(samples[0])); index++)
    {
        assert(App_Init() == E_OK);
        Test_AdcValue = samples[index];
        assert(App_MainFunction(1U) == E_OK);
        assert(g_AppKeepAliveRateLevel == expected[index]);
    }

    assert(App_Init() == E_OK);
    Test_AdcValue = 4095U;
    for (index = 1U; index <= 5U; index++)
    {
        assert(App_MainFunction(index) == E_OK);
    }
    assert(g_AppKeepAliveEvents == 1U);
    assert(Test_ComSignalValue[COM_SIGNAL_TX_ALIVE_COUNTER] == 1U);
    assert(Test_ComSignalValue[COM_SIGNAL_TX_KEEPALIVE_RATE] == 6U);
    assert(Test_ComSendCount[COM_SIGNAL_TX_ALIVE_COUNTER] == 1U);
    assert(Test_ComSendCount[COM_SIGNAL_TX_KEEPALIVE_RATE] == 1U);

    /* Level 6 has a 200 ms full cycle: 100 ms ON, then 100 ms OFF. */
    assert(Test_LedState[0] == 1U);
    for (index = 6U; index <= 100U; index++)
    {
        assert(App_MainFunction(index) == E_OK);
    }
    assert(Test_LedState[0] == 1U);
    assert(App_MainFunction(101U) == E_OK);
    assert(Test_LedState[0] == 0U);
    for (index = 102U; index <= 200U; index++)
    {
        assert(App_MainFunction(index) == E_OK);
    }
    assert(Test_LedState[0] == 0U);
    assert(App_MainFunction(201U) == E_OK);
    assert(Test_LedState[0] == 1U);

    puts("PASS: MASTER maps ADC levels, emits KeepAlive, and mirrors LED timing.");
    return 0;
}
