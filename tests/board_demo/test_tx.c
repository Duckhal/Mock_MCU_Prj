#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../drivers/can/com/Com.h"

static uint8_t Test_Frame[8];
static uint32_t Test_FrameCount;

/** Capture COM's actual eight-byte transmission before Update Bit clearing. */
Std_ReturnType PduR_ComTransmit(PduIdType id, const PduInfoType *info)
{
    assert(id == COM_IPDU_VEHICLE_STATUS);
    assert(info != NULL && info->SduLength == sizeof(Test_Frame));
    memcpy(Test_Frame, info->SduDataPtr, sizeof(Test_Frame));
    Test_FrameCount++;
    return E_OK;
}

/** Verify commands 0 through 3 use the COM slot and periodic scheduler. */
int main(void)
{
    uint32_t command;
    uint32_t tick;
    assert(Com_Init() == E_OK);
    for (command = 0U; command < 4U; command++)
    {
        assert(Com_SendSignal(COM_SIGNAL_LED_COMMAND, &command) == E_OK);
        for (tick = 0U; tick < ((command == 0U) ? 1U : 10U); tick++)
        { Com_MainFunctionTx(); }
        assert(Test_FrameCount == command + 1U);
        assert(Test_Frame[0] == 0U && Test_Frame[1] == 0U);
        assert(Test_Frame[2] == ((command << 1U) | 1U));
        for (tick = 3U; tick < sizeof(Test_Frame); tick++)
        { assert(Test_Frame[tick] == 0U); }
    }
    for (tick = 0U; tick < 10U; tick++) { Com_MainFunctionTx(); }
    assert(Test_FrameCount == 5U && Test_Frame[2] == 6U);
    puts("PASS: COM periodic DLC8 Tx encodes LED command and Update Bit.");
    return 0;
}
