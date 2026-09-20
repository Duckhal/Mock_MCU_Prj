#ifndef CANIF_CFG_H_
#define CANIF_CFG_H_

#include "CanIf_Types.h"

/* =========================
 * Tx L-PDU configuration
 * ========================= */

#define CANIF_NUM_TX_PDUS               (3U)
#define CANIF_TX_PDU_VEHICLE_STATUS     ((PduIdType)0U)
#define CANIF_TX_PDU_CANTP_DATA          ((PduIdType)1U)
#define CANIF_TX_PDU_CANTP_FC            ((PduIdType)2U)

/* =========================
 * Rx L-PDU configuration
 * ========================= */

#define CANIF_NUM_RX_PDUS               (3U)
#define CANIF_RX_PDU_VEHICLE_STATUS     ((PduIdType)0U)
#define CANIF_RX_PDU_CANTP_DATA          ((PduIdType)1U)
#define CANIF_RX_PDU_CANTP_FC            ((PduIdType)2U)

/* =========================
 * Configuration tables
 * ========================= */

extern const CanIf_TxPduConfigType CanIf_TxPduConfig[CANIF_NUM_TX_PDUS];
extern const CanIf_RxPduConfigType CanIf_RxPduConfig[CANIF_NUM_RX_PDUS];

#endif /* CANIF_CFG_H_ */
