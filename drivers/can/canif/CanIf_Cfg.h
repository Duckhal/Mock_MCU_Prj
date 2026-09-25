#ifndef CANIF_CFG_H_
#define CANIF_CFG_H_

#include "CanIf_Types.h"

/* =========================
 * Tx L-PDU configuration
 * ========================= */

#define CANIF_NUM_TX_PDUS               (5U)
#define CANIF_TX_PDU_KEEPALIVE           ((PduIdType)0U)
#define CANIF_TX_PDU_SLAVE1_STATUS       ((PduIdType)1U)
#define CANIF_TX_PDU_SLAVE2_STATUS       ((PduIdType)2U)
#define CANIF_TX_PDU_CANTP_DATA          ((PduIdType)3U)
#define CANIF_TX_PDU_CANTP_FC            ((PduIdType)4U)

/* =========================
 * Rx L-PDU configuration
 * ========================= */

#define CANIF_NUM_RX_PDUS               (5U)
#define CANIF_RX_PDU_KEEPALIVE           ((PduIdType)0U)
#define CANIF_RX_PDU_SLAVE1_STATUS       ((PduIdType)1U)
#define CANIF_RX_PDU_SLAVE2_STATUS       ((PduIdType)2U)
#define CANIF_RX_PDU_CANTP_DATA          ((PduIdType)3U)
#define CANIF_RX_PDU_CANTP_FC            ((PduIdType)4U)

/* =========================
 * Configuration tables
 * ========================= */

extern const CanIf_TxPduConfigType CanIf_TxPduConfig[CANIF_NUM_TX_PDUS];
extern const CanIf_RxPduConfigType CanIf_RxPduConfig[CANIF_NUM_RX_PDUS];

#endif /* CANIF_CFG_H_ */