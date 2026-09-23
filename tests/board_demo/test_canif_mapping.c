#include <assert.h>
#include <stdio.h>

#include "../../drivers/can/canif/CanIf_Cfg.c"

/** Verify all production COM and CanTp CAN identifier bindings. */
int main(void)
{
    assert(CANIF_NUM_TX_PDUS == 5U && CANIF_NUM_RX_PDUS == 5U);
    assert(CanIf_TxPduConfig[CANIF_TX_PDU_KEEPALIVE].canId == 0x100U);
    assert(CanIf_TxPduConfig[CANIF_TX_PDU_SLAVE1_STATUS].canId == 0x201U);
    assert(CanIf_TxPduConfig[CANIF_TX_PDU_SLAVE2_STATUS].canId == 0x202U);
    assert(CanIf_TxPduConfig[CANIF_TX_PDU_CANTP_DATA].canId == 0x650U);
    assert(CanIf_TxPduConfig[CANIF_TX_PDU_CANTP_FC].canId == 0x658U);
    assert(CanIf_RxPduConfig[CANIF_RX_PDU_KEEPALIVE].canId == 0x100U);
    assert(CanIf_RxPduConfig[CANIF_RX_PDU_SLAVE1_STATUS].canId == 0x201U);
    assert(CanIf_RxPduConfig[CANIF_RX_PDU_SLAVE2_STATUS].canId == 0x202U);
    assert(CanIf_RxPduConfig[CANIF_RX_PDU_CANTP_DATA].canId == 0x650U);
    assert(CanIf_RxPduConfig[CANIF_RX_PDU_CANTP_FC].canId == 0x658U);
    puts("PASS: production CanIf maps all three COM and two CanTp IDs.");
    return 0;
}
