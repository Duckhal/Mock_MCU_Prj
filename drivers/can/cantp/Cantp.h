#ifndef CANTP_H_
#define CANTP_H_

#include "Cantp_Types.h"

#define CANTP_LOG_CAPACITY (32U)

extern volatile CanTp_LogRecordType CanTp_LogRecords[CANTP_LOG_CAPACITY];
extern volatile uint32_t CanTp_LogSequence;

/*
 * @brief Initialize the mock CanTp module.
 * @return E_OK when the fixed Phase-1 configuration is valid; otherwise E_NOT_OK.
 */
Std_ReturnType CanTp_Init(void);

/*
 * @brief Accept one upper-layer N-SDU transmission request.
 * @param TxNSduId Configured transmit N-SDU handle.
 * @param PduInfoPtr Complete payload request with length from 1 to 62 bytes.
 * @return E_OK when the request is accepted; otherwise E_NOT_OK.
 *
 * E_OK means that CanTp owns the request. Final transfer status is reported
 * later through the PduR Tx-confirmation callback.
 */
Std_ReturnType CanTp_Transmit(PduIdType TxNSduId,
                              const PduInfoType *PduInfoPtr);

/*
 * @brief Receive one complete eight-byte Data or Flow Control N-PDU.
 * @param RxNPduId Configured receive N-PDU handle.
 * @param PduInfoPtr Frame payload, valid only during this callback.
 */
void CanTp_RxIndication(PduIdType RxNPduId,
                        const PduInfoType *PduInfoPtr);

/*
 * @brief Receive local confirmation for one Data or Flow Control N-PDU.
 * @param TxNPduId Configured transmit N-PDU handle.
 */
void CanTp_TxConfirmation(PduIdType TxNPduId);

/*
 * @brief Advance Phase-1 STmin scheduling and submit at most one Data frame.
 *
 * Retry and timeout processing is added in Phase 2.
 */
void CanTp_MainFunction(void);

#endif /* CANTP_H_ */
