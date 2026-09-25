#include "CanIf_Cfg.h"
#include "../can_driver/Can_Cfg.h"
#include "../cantp/Cantp_Cfg.h"

const CanIf_TxPduConfigType CanIf_TxPduConfig[CANIF_NUM_TX_PDUS] =
{
    {
        .txPduId = CANIF_TX_PDU_KEEPALIVE,
        .canId = 0x100U,
        .hth = CAN_HTH_CAN0_TX
    },
    {
        .txPduId = CANIF_TX_PDU_SLAVE1_STATUS,
        .canId = 0x201U,
        .hth = CAN_HTH_CAN0_TX
    },
    {
        .txPduId = CANIF_TX_PDU_SLAVE2_STATUS,
        .canId = 0x202U,
        .hth = CAN_HTH_CAN0_TX
    },
    {
        .txPduId = CANIF_TX_PDU_CANTP_DATA,
        .canId = CANTP_DATA_CAN_ID,
        .hth = CAN_HTH_CAN0_TX
    },
    {
        .txPduId = CANIF_TX_PDU_CANTP_FC,
        .canId = CANTP_FC_CAN_ID,
        .hth = CAN_HTH_CAN0_TX
    }
};

const CanIf_RxPduConfigType CanIf_RxPduConfig[CANIF_NUM_RX_PDUS] =
{
    {
        .rxPduId = CANIF_RX_PDU_KEEPALIVE,
        .canId = 0x100U,
        .hrh = CAN_HRH_CAN0_RX
    },
    {
        .rxPduId = CANIF_RX_PDU_SLAVE1_STATUS,
        .canId = 0x201U,
        .hrh = CAN_HRH_CAN0_RX
    },
    {
        .rxPduId = CANIF_RX_PDU_SLAVE2_STATUS,
        .canId = 0x202U,
        .hrh = CAN_HRH_CAN0_RX
    },
    {
        .rxPduId = CANIF_RX_PDU_CANTP_DATA,
        .canId = CANTP_DATA_CAN_ID,
        .hrh = CAN_HRH_CAN0_RX
    },
    {
        .rxPduId = CANIF_RX_PDU_CANTP_FC,
        .canId = CANTP_FC_CAN_ID,
        .hrh = CAN_HRH_CAN0_RX
    }
};