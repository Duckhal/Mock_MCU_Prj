#include <assert.h>
#include <stdio.h>
#include "../../drivers/can/com/Com.h"

/** Satisfy the lower Tx dependency while this test exercises COM Rx only. */
Std_ReturnType PduR_ComTransmit(PduIdType Id, const PduInfoType *InfoPtr)
{
    (void)Id;
    (void)InfoPtr;
    return E_OK;
}

#include "../../drivers/can/com/Com.c"
#include "../../drivers/can/com/Com_Cfg.c"

/** Verify KeepAlive Rx values and per-I-PDU event counting. */
int main(void)
{
    uint8_t frame[8] = {0x5AU, 4U, 0U, 0U, 0U, 0U, 0U, 0U};
    PduInfoType info = {frame, sizeof(frame)};
    uint32_t alive = 0U;
    uint32_t level = 0U;

    assert(Com_Init() == E_OK);
    Com_RxIndication(COM_IPDU_RX_KEEPALIVE, &info);
    assert(Com_ReceiveSignal(COM_SIGNAL_RX_ALIVE_COUNTER, &alive) == E_OK);
    assert(Com_ReceiveSignal(COM_SIGNAL_RX_KEEPALIVE_RATE, &level) == E_OK);
    assert(alive == 0x5AU && level == 4U);
    assert(Com_GetRxIPduIndicationCount(COM_IPDU_RX_KEEPALIVE) == 1U);
    info.SduLength = 7U;
    Com_RxIndication(COM_IPDU_RX_KEEPALIVE, &info);
    assert(Com_GetRxIPduIndicationCount(COM_IPDU_RX_KEEPALIVE) == 1U);
    puts("PASS: COM KeepAlive Rx decodes values and rejects a short frame.");
    return 0;
}
