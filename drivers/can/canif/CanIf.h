#ifndef CANIF_H
#define CANIF_H

#include "CanIf_Types.h"

/**
 * @brief Validate the static Tx/Rx configuration and initialize CanIf.
 * @return E_OK on success; E_NOT_OK if configuration is invalid.
 */
Std_ReturnType CanIf_Init(void);

/**
 * @brief Map a local Tx PDU ID to CAN ID/HTH and request transmission.
 * @param TxPduId Local CanIf Tx PDU handle.
 * @param PduInfoPtr Payload pointer and length; input pointers are not retained.
 * @return E_OK if Can_Write accepts the request; E_NOT_OK otherwise.
 */
Std_ReturnType CanIf_Transmit(
    PduIdType TxPduId,
    const PduInfoType *PduInfoPtr
);

/**
 * @brief Receive driver completion and forward the notification to PduR.
 * @param TxPduId Software PDU handle saved by the CAN driver.
 */
void CanIf_TxConfirmation(PduIdType TxPduId);

/**
 * @brief Resolve HRH plus CAN ID to a local Rx PDU and forward it to PduR.
 * @param Hrh Receive hardware object handle.
 * @param RxPdu Received frame; its payload is valid only during this callback.
 */
void CanIf_RxIndication(
    Can_HwHandleType Hrh,
    const Can_RxPduType *RxPdu
);

#endif /* CANIF_H */