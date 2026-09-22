#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../drivers/can/com/Com.h"

static uint8_t Test_Frame[8];
static uint32_t Test_FrameCount;

/** Capture COM's actual eight-byte LED command transmission. */
Std_ReturnType PduR_ComTransmit(PduIdType id, const PduInfoType *info)
{
    assert(id == COM_IPDU_VEHICLE_STATUS);
    assert(info != NULL && info->SduLength == sizeof(Test_Frame));
    memcpy(Test_Frame, info->SduDataPtr, sizeof(Test_Frame));
    Test_FrameCount++;
    return E_OK;
}

/** Verify modes 0 through 3 use the new COM frame and periodic scheduler. */
int main(void)
{
    uint32_t command;
    uint32_t mode;
    uint32_t tick;
    assert(Com_Init() == E_OK);
    for (mode = 0U; mode < 4U; mode++)
    {
        command = COM_LED_COMMAND_ENCODE(
            mode, (mode == 0U) ? COM_LED_STATE_OFF : COM_LED_STATE_ON);
        assert(Com_SendSignal(COM_SIGNAL_LED_COMMAND, &command) == E_OK);
        for (tick = 0U; tick < ((mode == 0U) ? 1U : 10U); tick++)
        { Com_MainFunctionTx(); }
        assert(Test_FrameCount == mode + 1U);
        assert(Test_Frame[0] == 0U);
        assert(Test_Frame[1] == mode);
        assert(Test_Frame[2] == ((mode == 0U) ? 0U : 1U));
        for (tick = 3U; tick < sizeof(Test_Frame); tick++)
        { assert(Test_Frame[tick] == 0U); }
    }
    for (tick = 0U; tick < 10U; tick++) { Com_MainFunctionTx(); }
    assert(Test_FrameCount == 5U && Test_Frame[1] == 3U &&
           Test_Frame[2] == 1U);
    puts("PASS: COM periodic DLC8 Tx encodes Global ID, mode, and state.");
    return 0;
}
