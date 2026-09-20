#include <assert.h>
#include <stdio.h>

#include "../../drivers/can/canif/CanIf_Cfg.h"

/** Verify the production COM and dedicated CanTp Data/FC CAN identifiers. */
int main(void)
{
    assert(CANIF_NUM_TX_PDUS == 3U && CANIF_NUM_RX_PDUS == 3U);
    assert(CanIf_TxPduConfig[0].txPduId == CANIF_TX_PDU_VEHICLE_STATUS);
    assert(CanIf_RxPduConfig[0].rxPduId == CANIF_RX_PDU_VEHICLE_STATUS);
    assert(CanIf_TxPduConfig[0].canId == 0x100U);
    assert(CanIf_RxPduConfig[0].canId == 0x100U);
    assert(CanIf_TxPduConfig[1].canId == 0x650U);
    assert(CanIf_RxPduConfig[1].canId == 0x650U);
    assert(CanIf_TxPduConfig[2].canId == 0x658U);
    assert(CanIf_RxPduConfig[2].canId == 0x658U);
    puts("PASS: production CanIf maps COM 0x100 and CanTp 0x650/0x658.");
    return 0;
}
