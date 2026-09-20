#ifndef CANTP_H_
#define CANTP_H_

#include "Cantp_Types.h"

/*
 * @brief Initialize the mock CanTp module.
 * @return E_NOT_OK until configuration validation and runtime setup are implemented.
 */
Std_ReturnType CanTp_Init(void);

/*
 * @brief Accept one upper-layer N-SDU transmission request.
 * @param TxNSduId Configured transmit N-SDU handle.
 * @param PduInfoPtr Complete payload request with length from 1 to 62 bytes.
 * @return E_NOT_OK while the transport state machine is not implemented.
 *
 * A future E_OK return will mean request accepted, not transfer completed.
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
 * @brief Advance retries, timeout checks and STmin scheduling by one 1 ms tick.
 */
void CanTp_MainFunction(void);

#endif /* CANTP_H_ */
