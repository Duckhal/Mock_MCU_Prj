#ifndef CANTP_H_
#define CANTP_H_

#include "Cantp_Types.h"

#define CANTP_LOG_CAPACITY (32U)

extern volatile CanTp_LogRecordType CanTp_LogRecords[CANTP_LOG_CAPACITY];
extern volatile uint32_t CanTp_LogSequence;
extern volatile CanTp_AbortReasonType CanTp_LastTxAbortReason;
extern volatile CanTp_AbortReasonType CanTp_LastRxAbortReason;
extern volatile uint32_t CanTp_TxAbortCount;
extern volatile uint32_t CanTp_RxAbortCount;

/* Validate the fixed configuration and initialize CanTp. */
Std_ReturnType CanTp_Init(void);

/* Enable or disable reception of Data N-PDUs while no Rx session is active. */
Std_ReturnType CanTp_SetDataRxEnabled(uint8_t Enabled);

/* Accept one 1..62-byte N-SDU; report final status later through PduR. */
Std_ReturnType CanTp_Transmit(PduIdType TxNSduId, const PduInfoType *PduInfoPtr);

/* Process one complete eight-byte Data or Flow Control N-PDU. */
void CanTp_RxIndication(PduIdType RxNPduId, const PduInfoType *PduInfoPtr);

/* Process a local Tx confirmation for a Data or Flow Control N-PDU. */
void CanTp_TxConfirmation(PduIdType TxNPduId);

/* Advance retry, timeout, and STmin processing once per 1 ms tick. */
void CanTp_MainFunction(void);

#endif /* CANTP_H_ */
