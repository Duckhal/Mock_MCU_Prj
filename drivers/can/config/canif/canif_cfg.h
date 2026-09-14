#ifndef CANIF_CFG_H_
#define CANIF_CFG_H_

#include "../../comm/canif/canif_types.h"

/* Static logical L-PDU limits enforced later by CanIf_Init(). */
#define CANIF_MAX_TX_PDU_COUNT    (8U)
#define CANIF_MAX_RX_PDU_COUNT    (8U)

/* CanIf-local Tx L-PDU handles for the current training profile. */
enum
{
    CANIF_TX_PDU_VEHICLE_STATUS = 7U
};

/* Immutable CanIf configuration selected by Node_Config. */
extern const CanIf_ConfigType CanIf_Config;

#endif /* CANIF_CFG_H_ */
