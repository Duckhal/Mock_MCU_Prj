#include "PduR_Cfg.h"

#include "../com/Com_Cfg.h"
#include "../canif/CanIf_Cfg.h"
#include "../common/CanStack_Cfg.h"
#include "../cantp/Cantp_Cfg.h"

const PduR_TxRouteConfigType PduR_TxRouteConfig[PDUR_NUM_TX_ROUTES] =
{
    {
        .sourcePduId = COM_IPDU_TX_KEEPALIVE,
        .destPduId = CANIF_TX_PDU_KEEPALIVE,
        .globalPduId = GLOBAL_PDU_KEEPALIVE
    },
    {
        .sourcePduId = COM_IPDU_TX_SLAVE1_STATUS,
        .destPduId = CANIF_TX_PDU_SLAVE1_STATUS,
        .globalPduId = GLOBAL_PDU_SLAVE1_STATUS
    },
    {
        .sourcePduId = COM_IPDU_TX_SLAVE2_STATUS,
        .destPduId = CANIF_TX_PDU_SLAVE2_STATUS,
        .globalPduId = GLOBAL_PDU_SLAVE2_STATUS
    }
};

const PduR_RxRouteConfigType PduR_RxRouteConfig[PDUR_NUM_RX_ROUTES] =
{
    {
        .sourcePduId = CANIF_RX_PDU_KEEPALIVE,
        .destPduId = COM_IPDU_RX_KEEPALIVE,
        .globalPduId = GLOBAL_PDU_KEEPALIVE
    },
    {
        .sourcePduId = CANIF_RX_PDU_SLAVE1_STATUS,
        .destPduId = COM_IPDU_RX_SLAVE1_STATUS,
        .globalPduId = GLOBAL_PDU_SLAVE1_STATUS
    },
    {
        .sourcePduId = CANIF_RX_PDU_SLAVE2_STATUS,
        .destPduId = COM_IPDU_RX_SLAVE2_STATUS,
        .globalPduId = GLOBAL_PDU_SLAVE2_STATUS
    }
};

const PduR_CanTpTxRouteConfigType PduR_CanTpTxRouteConfig[PDUR_NUM_CANTP_TX_ROUTES] =
{
    { GLOBAL_PDU_CANTP_DATA, CANTP_TX_NSDU }
};

const PduR_CanTpRxRouteConfigType PduR_CanTpRxRouteConfig[PDUR_NUM_CANTP_RX_ROUTES] =
{
    { CANTP_RX_NSDU, GLOBAL_PDU_CANTP_DATA }
};
