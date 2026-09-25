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

/*
 * @brief Initialize the mock CanTp module.
 * @return E_OK when the fixed configuration is valid; otherwise E_NOT_OK.
 */
Std_ReturnType CanTp_Init(void);

/* Enable or disable reception of Data N-PDUs while no Rx session is active. */
Std_ReturnType CanTp_SetDataRxEnabled(uint8_t Enabled);

/*
 * @brief Accept one upper-layer N-SDU transmission request.
 * @param TxNSduId Configured transmit N-SDU handle.
 * @param PduInfoPtr Complete payload request with length from 1 to 62 bytes.
 * @return E_OK when the request is accepted; otherwise E_NOT_OK.
 *
 * E_OK means that CanTp owns the request. Final transfer status is reported
 * later through the PduR Tx-confirmation callback.
 */
Std_ReturnType CanTp_Transmit(PduIdType TxNSduId, const PduInfoType *PduInfoPtr);

/*
 * @brief Receive one complete eight-byte Data or Flow Control N-PDU.
 * @param RxNPduId Configured receive N-PDU handle.
 * @param PduInfoPtr Frame payload, valid only during this callback.
 */
void CanTp_RxIndication(PduIdType RxNPduId, const PduInfoType *PduInfoPtr);

/*
 * @brief Receive local confirmation for one Data or Flow Control N-PDU.
 * @param TxNPduId Configured transmit N-PDU handle.
 */
void CanTp_TxConfirmation(PduIdType TxNPduId);

/*
 * @brief Advance retry, timeout and STmin processing by one millisecond.
 *
 * The integration scheduler shall call this function exactly once per 1 ms
 * tick after dispatching lower-layer confirmations and received frames.
 */
void CanTp_MainFunction(void);

#endif /* CANTP_H_ */
