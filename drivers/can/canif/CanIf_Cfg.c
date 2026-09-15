#include "CanIf_Cfg.h"
#include "../can_driver/Can_Cfg.h"

const CanIf_TxPduConfigType CanIf_TxPduConfig[CANIF_NUM_TX_PDUS] =
{
    {
        .txPduId = CANIF_TX_PDU_VEHICLE_STATUS,
        .canId = 0x321U,
        .hth = CAN_HTH_CAN0_TX
    }
};

const CanIf_RxPduConfigType CanIf_RxPduConfig[CANIF_NUM_RX_PDUS] =
{
    {
        .rxPduId = CANIF_RX_PDU_VEHICLE_STATUS,
        .canId = 0x321U,
        .hrh = CAN_HRH_CAN0_RX
    }
};