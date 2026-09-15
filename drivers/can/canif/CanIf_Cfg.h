#ifndef CANIF_CFG_H
#define CANIF_CFG_H

#include "CanIf_Types.h"

/* =========================
 * Tx L-PDU configuration
 * ========================= */

#define CANIF_NUM_TX_PDUS               (1U)
#define CANIF_TX_PDU_VEHICLE_STATUS     ((PduIdType)0U)

/* =========================
 * Rx L-PDU configuration
 * ========================= */

#define CANIF_NUM_RX_PDUS               (1U)
#define CANIF_RX_PDU_VEHICLE_STATUS     ((PduIdType)0U)

/* =========================
 * Configuration tables
 * ========================= */

extern const CanIf_TxPduConfigType CanIf_TxPduConfig[CANIF_NUM_TX_PDUS];
extern const CanIf_RxPduConfigType CanIf_RxPduConfig[CANIF_NUM_RX_PDUS];

#endif /* CANIF_CFG_H */