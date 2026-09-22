#include <assert.h>
#include <stdio.h>

#define APP_BOARD_ROLE (1U)
#include "app_host_fixture.h"

/** Verify every ADC boundary maps to the required LED mode and state. */
int main(void)
{
    static const uint16_t samples[] =
    {
        0U, 999U, 1000U, 1999U, 2000U,
        2999U, 3000U, 3999U, 4000U, 4095U
    };
    static const uint32_t expected[] =
    {
        COM_LED_COMMAND_ENCODE(COM_LED_MODE_STEADY, COM_LED_STATE_OFF),
        COM_LED_COMMAND_ENCODE(COM_LED_MODE_STEADY, COM_LED_STATE_OFF),
        COM_LED_COMMAND_ENCODE(COM_LED_MODE_BLINK_500_MS, COM_LED_STATE_ON),
        COM_LED_COMMAND_ENCODE(COM_LED_MODE_BLINK_500_MS, COM_LED_STATE_ON),
        COM_LED_COMMAND_ENCODE(COM_LED_MODE_BLINK_1000_MS, COM_LED_STATE_ON),
        COM_LED_COMMAND_ENCODE(COM_LED_MODE_BLINK_1000_MS, COM_LED_STATE_ON),
        COM_LED_COMMAND_ENCODE(COM_LED_MODE_BLINK_2000_MS, COM_LED_STATE_ON),
        COM_LED_COMMAND_ENCODE(COM_LED_MODE_BLINK_2000_MS, COM_LED_STATE_ON),
        COM_LED_COMMAND_ENCODE(COM_LED_MODE_STEADY, COM_LED_STATE_ON),
        COM_LED_COMMAND_ENCODE(COM_LED_MODE_STEADY, COM_LED_STATE_ON)
    };
    unsigned index;

    (void)Test_RunApp;
    (void)Test_InjectUart;
    assert(App_HardwareInit() == E_OK);
    assert(App_Init() == E_OK);
    assert(g_AppModeTx == APP_ROLE_TX);

    for (index = 0U; index < (sizeof(samples) / sizeof(samples[0])); index++)
    {
        Test_AdcValue = samples[index];
        assert(App_MainFunction(1U + ((uint32_t)index * 10U)) == E_OK);
        assert(Test_SendCount == (uint32_t)(index + 1U));
        assert(Test_SentCommands[index] == expected[index]);
    }

    assert(Test_SentCommands[0] == 0U);
    assert(Test_SentCommands[1] == 0U);
    assert(Test_UartStringCount == 0U && Test_UartRawTxLength == 0U);
    puts("PASS: ADC boundaries map 0-999 to mode 0/state 0 and all other ranges correctly.");
    return 0;
}
