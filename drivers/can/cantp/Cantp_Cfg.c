#include "Cantp_Cfg.h"

/* Configure one dedicated bidirectional Phase-2 CanTp connection. */
const CanTp_ConnectionConfigType CanTp_ConnectionConfig[CANTP_NUM_CONNECTIONS] =
{
    {
        .txNSduId = CANTP_TX_NSDU,
        .rxNSduId = CANTP_RX_NSDU,
        .txDataNPduId = CANTP_TX_NPDU_DATA,
        .rxDataNPduId = CANTP_RX_NPDU_DATA,
        .txFcNPduId = CANTP_TX_NPDU_FC,
        .rxFcNPduId = CANTP_RX_NPDU_FC,
        .canIfTxDataPduId = CANTP_CANIF_TX_LPDU_DATA,
        .canIfTxFcPduId = CANTP_CANIF_TX_LPDU_FC,
        .canIfRxDataPduId = CANTP_CANIF_RX_LPDU_DATA,
        .canIfRxFcPduId = CANTP_CANIF_RX_LPDU_FC
    }
};
