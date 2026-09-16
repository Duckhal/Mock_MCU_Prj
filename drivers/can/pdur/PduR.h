#ifndef PDUR_H_
#define PDUR_H_

#include "PduR_Types.h"

/* Debugger snapshots of valid CanIf callbacks, including short test frames. */
extern volatile uint32_t PduR_TxConfirmationCount;
extern volatile uint32_t PduR_RxIndicationCount;
extern volatile PduIdType PduR_LastTxPduId;
extern volatile PduIdType PduR_LastRxPduId;
extern volatile PduLengthType PduR_LastRxLength;
extern volatile uint8_t PduR_LastRxBytes[8];

Std_ReturnType PduR_ComTransmit(PduIdType ComTxPduId, const PduInfoType *PduInfoPtr);
/** Route one CanIf Rx L-PDU to its COM Rx I-PDU. */
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
/** Route one CanIf Tx confirmation to its COM Tx I-PDU. */
void PduR_CanIfTxConfirmation(PduIdType TxPduId);

#endif /* PDUR_H_ */
