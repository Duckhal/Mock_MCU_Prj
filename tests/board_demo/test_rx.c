#include <assert.h>
#include <stdio.h>
#include "../../drivers/can/com/Com.h"

/** Satisfy the COM Tx callback while this test exercises only the Rx path. */
Std_ReturnType PduR_ComTransmit(PduIdType id, const PduInfoType *info)
{ (void)id; (void)info; return E_OK; }

/** Verify COM decodes four commands from its eight-byte Rx signal slot. */
int main(void)
{
    uint8_t frame[8] = {0U};
    PduInfoType info = {frame, sizeof(frame)};
    uint32_t command;
    uint32_t received = UINT32_MAX;
    assert(Com_Init() == E_OK);
    assert(Com_ReceiveSignal(COM_SIGNAL_RX_LED_COMMAND, &received) == E_NOT_OK);
    for (command = 0U; command < 4U; command++)
    {
        frame[2] = (uint8_t)((command << 1U) | 1U);
        Com_RxIndication(COM_IPDU_RX_VEHICLE_STATUS, &info);
        assert(Com_ReceiveSignal(COM_SIGNAL_RX_LED_COMMAND, &received) == E_OK);
        assert(received == command);
    }
    assert(Com_GetRxIndicationCount() == 4U);
    info.SduLength = 7U;
    Com_RxIndication(COM_IPDU_RX_VEHICLE_STATUS, &info);
    assert(Com_GetRxIndicationCount() == 4U);
    puts("PASS: COM DLC8 Rx decodes LED commands and rejects short frames.");
    return 0;
}
