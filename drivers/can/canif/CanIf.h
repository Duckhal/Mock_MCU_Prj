#ifndef CANIF_H_
#define CANIF_H_

#include "CanIf_Types.h"

/* Validate the static configuration and initialize CanIf. */
Std_ReturnType CanIf_Init(void);

/* Map a local Tx PDU to its CAN ID and HTH, then request transmission. */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* Forward a CAN Driver transmission confirmation to PduR. */
void CanIf_TxConfirmation(PduIdType TxPduId);

/* Resolve an HRH and CAN ID, then forward the received frame to PduR. */
void CanIf_RxIndication(Can_HwHandleType Hrh, const Can_RxPduType *RxPdu);

#endif /* CANIF_H */