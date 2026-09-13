#ifndef CAN_UPPER_H_
#define CAN_UPPER_H_

#include "CanUpper_Types.h"
#include "../inc/CanUpper_Cfg.h"
#include "../../driver/inc/Can_Types.h"
#include "../../driver/inc/Can_Cfg.h"

void CanUpper_Init(const Can_ConfigType *CanConfig);
Can_ReturnType CanUpper_Transmit(PduIdType TxPduId, const uint8_t *data, uint8_t length);
void CanUpper_TxConfirmation(uint8_t HthId);
void CanUpper_RxIndication(const Can_HwType *Mailbox, const Can_PduType *PduInfoPtr);
CanUpper_TxStatusType CanUpper_GetTxStatus(PduIdType TxPduId);
Can_ReturnType CanUpper_GetRxData(PduIdType RxPduId, uint8_t *dataOut, uint8_t *lengthOut);
void CanUpper_MainFunction(void);

#endif /* CAN_UPPER_H_*/
