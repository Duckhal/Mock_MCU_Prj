#include "canif_cfg.h"

#include <stddef.h>

#include "../can/Can_Cfg.h"
#include "../comm_matrix_cfg.h"

#define CANIF_ARRAY_COUNT(array) ((uint16_t)(sizeof(array) / sizeof((array)[0])))

/* Direct CAN Binding for the VehicleStatus Tx L-PDU. */
static const CanIf_TxPduConfigType CanIf_TxPdus[] =
{
    {
        .txPduId = CANIF_TX_PDU_VEHICLE_STATUS,
        .globalPduId = COMM_MATRIX_GLOBAL_PDU_VEHICLE_STATUS,
        .canId = COMM_MATRIX_CAN_ID_VEHICLE_STATUS,
        .hthRef = CAN_HTH_0,
        .lengthBytes = COMM_MATRIX_VEHICLE_STATUS_LENGTH_BYTES
    }
};

/* Current firmware profile is Tx-only at COM/CanIf level. */
const CanIf_ConfigType CanIf_Config =
{
    .txPdus = CanIf_TxPdus,
    .txPduCount = CANIF_ARRAY_COUNT(CanIf_TxPdus),
    .rxPdus = NULL,
    .rxPduCount = 0U
};
