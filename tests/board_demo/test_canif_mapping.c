#include <assert.h>
#include <stdio.h>

#include "../../drivers/can/canif/CanIf_Cfg.h"

/** Verify the production Tx/Rx L-PDUs share the agreed CAN identifier. */
int main(void)
{
    assert(CANIF_NUM_TX_PDUS == 1U && CANIF_NUM_RX_PDUS == 1U);
    assert(CanIf_TxPduConfig[0].txPduId == CANIF_TX_PDU_VEHICLE_STATUS);
    assert(CanIf_RxPduConfig[0].rxPduId == CANIF_RX_PDU_VEHICLE_STATUS);
    assert(CanIf_TxPduConfig[0].canId == 0x100U);
    assert(CanIf_RxPduConfig[0].canId == 0x100U);
    puts("PASS: production CanIf Tx/Rx use CAN ID 0x100.");
    return 0;
}
