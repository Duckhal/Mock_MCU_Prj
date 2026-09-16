#include "PduR_Cfg.h"

#include "../com/Com_Cfg.h"
#include "../canif/CanIf_Cfg.h"
#include "../common/CanStack_Cfg.h"

const PduR_TxRouteConfigType PduR_TxRouteConfig[PDUR_NUM_TX_ROUTES] =
{
    {
        .sourcePduId = COM_IPDU_VEHICLE_STATUS,
        .destPduId = CANIF_TX_PDU_VEHICLE_STATUS,
        .globalPduId = GLOBAL_PDU_VEHICLE_STATUS
    }
};

const PduR_RxRouteConfigType
PduR_RxRouteConfig[PDUR_NUM_RX_ROUTES] =
{
    {
        .sourcePduId = CANIF_RX_PDU_VEHICLE_STATUS,
        .destPduId = COM_IPDU_RX_VEHICLE_STATUS,
        .globalPduId = GLOBAL_PDU_VEHICLE_STATUS
    }
};
