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

/* Reset PduR runtime observations before communication starts. */
Std_ReturnType PduR_Init(void);

/* Route one COM Tx I-PDU to its configured CanIf Tx L-PDU. */
Std_ReturnType PduR_ComTransmit(PduIdType ComTxPduId, const PduInfoType *PduInfoPtr);
/* Route one application large-message request to its CanTp Tx N-SDU. */
Std_ReturnType PduR_Transmit(GlobalPduIdType GlobalPduId, const PduInfoType *PduInfoPtr);

/* Copy the application Tx source into CanTp exactly once per accepted N-SDU. */
BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType TxNSduId, uint8_t *DestPtr, PduLengthType Length);
/* Forward one final CanTp Tx result to the application. */
void PduR_CanTpTxConfirmation(PduIdType TxNSduId, Std_ReturnType Result);
/* Reserve one application Rx queue slot for a complete incoming N-SDU. */
BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType RxNSduId, PduLengthType TotalLength);
/* Copy one complete reassembled N-SDU into the reserved application slot. */
BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType RxNSduId, const uint8_t *DataPtr, PduLengthType Length);
/* Forward one final CanTp Rx result to the application. */
void PduR_CanTpRxIndication(PduIdType RxNSduId, Std_ReturnType Result);
/* Route one CanIf Rx L-PDU to its COM Rx I-PDU. */
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
/* Route one CanIf Tx confirmation to its COM Tx I-PDU. */
void PduR_CanIfTxConfirmation(PduIdType TxPduId);

#endif /* PDUR_H_ */