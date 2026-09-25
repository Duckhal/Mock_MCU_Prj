#ifndef CAN_H_
#define CAN_H_

#include "Can_Types.h"
#include "Can_Cfg.h"

/* Initialize and validate the CAN0 polling driver configuration.
 *  Return CAN_OK when ready, otherwise CAN_NOT_OK. */
Can_ReturnType Can_Init(void);

/* Request an asynchronous CAN0 transmission through the configured HTH.
 *  Return CAN_OK, CAN_BUSY, or CAN_NOT_OK according to request status. */
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);

/* Poll CAN0 Tx completion, release the mailbox, and notify CanIf. */
void Can_MainFunction_Write(void);

/* Poll CAN0 Rx and synchronously deliver a valid frame to CanIf. */
void Can_MainFunction_Read(void);

#endif /* CAN_H_ */