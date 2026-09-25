#ifndef CANTP_CFG_H_
#define CANTP_CFG_H_

#include "Cantp_Types.h"
#include "../canif/CanIf_Cfg.h"

/* Mock CanTp limits and fixed timing profile. */
#define CANTP_NUM_CONNECTIONS          (1U)
#define CANTP_MAX_NSDU_LENGTH          (62U)
#define CANTP_CHUNK_CAPACITY           (64U)
#define CANTP_FRAME_LENGTH             (8U)
#define CANTP_SF_PAYLOAD_LENGTH        (6U)
#define CANTP_FF_PAYLOAD_LENGTH        (6U)
#define CANTP_CF_PAYLOAD_LENGTH        (7U)
#define CANTP_BLOCK_SIZE               (4U)
#define CANTP_STMIN_MS                 (5U)
#define CANTP_MAX_RETRIES              (3U)
#define CANTP_N_AS_MS                  (100U)
#define CANTP_N_AR_MS                  (100U)
#define CANTP_N_BS_MS                  (100U)
#define CANTP_N_CR_MS                  (100U)

/*
 * The classroom wire-format update overrides the older guide examples:
 * SF = [0x00][Length][up to 6 data bytes]
 * FF = [0x10][Length][6 data bytes]
 * CF = [0x20 | SN][up to 7 data bytes]
 * FC = [0x30 | FS][BS][STmin][padding]
 */
#define CANTP_SF_MIN_LENGTH            (1U)
#define CANTP_SF_MAX_LENGTH            (6U)
#define CANTP_FF_MIN_LENGTH            (7U)
#define CANTP_FF_MAX_LENGTH            CANTP_MAX_NSDU_LENGTH

#define CANTP_PCI_SF                   (0x00U)
#define CANTP_PCI_FF                   (0x10U)
#define CANTP_PCI_CF                   (0x20U)
#define CANTP_PCI_FC                   (0x30U)
#define CANTP_PCI_TYPE_MASK            (0xF0U)
#define CANTP_PCI_SN_MASK              (0x0FU)
#define CANTP_PADDING_BYTE             (0x00U)

/* CanTp handles. Each group is a separate ID namespace. */
#define CANTP_TX_NSDU                  ((PduIdType)0U)
#define CANTP_RX_NSDU                  ((PduIdType)0U)
#define CANTP_TX_NPDU_DATA             ((PduIdType)0U)
#define CANTP_TX_NPDU_FC               ((PduIdType)1U)
#define CANTP_RX_NPDU_DATA             ((PduIdType)0U)
#define CANTP_RX_NPDU_FC               ((PduIdType)1U)
#define CANTP_CANIF_TX_LPDU_DATA       CANIF_TX_PDU_CANTP_DATA
#define CANTP_CANIF_TX_LPDU_FC         CANIF_TX_PDU_CANTP_FC
#define CANTP_CANIF_RX_LPDU_DATA       CANIF_RX_PDU_CANTP_DATA
#define CANTP_CANIF_RX_LPDU_FC         CANIF_RX_PDU_CANTP_FC

#define CANTP_DATA_CAN_ID              (0x650U)
#define CANTP_FC_CAN_ID                (0x658U)

extern const CanTp_ConnectionConfigType CanTp_ConnectionConfig[CANTP_NUM_CONNECTIONS];

#endif /* CANTP_CFG_H_ */