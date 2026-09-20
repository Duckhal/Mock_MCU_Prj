#include "Cantp_Cfg.h"

/*
 * One dedicated bidirectional connection skeleton.
 * Actual CanIf/PduR table entries and CAN IDs must be added during integration.
 */
const CanTp_ConnectionConfigType
CanTp_ConnectionConfig[CANTP_NUM_CONNECTIONS] =
{
    {
        .txNSduId = CANTP_TX_NSDU,
        .rxNSduId = CANTP_RX_NSDU,
        .txDataNPduId = CANTP_TX_NPDU_DATA,
        .rxDataNPduId = CANTP_RX_NPDU_DATA,
        .txFcNPduId = CANTP_TX_NPDU_FC,
        .rxFcNPduId = CANTP_RX_NPDU_FC,
        .canIfTxDataPduId = CANTP_CANIF_TX_LPDU_DATA,
        .canIfTxFcPduId = CANTP_CANIF_TX_LPDU_FC
    }
};
