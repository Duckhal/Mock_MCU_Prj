#ifndef NODE_APP_H_
#define NODE_APP_H_

#include "../drivers/can/common/CanStack_Types.h"

#define NODE_APP_RX_QUEUE_SIZE (2U)
#define NODE_APP_NSDU_CAPACITY (64U)
#define NODE_APP_MAX_NSDU_LENGTH (62U)

typedef enum
{
    NODE_APP_SLOT_FREE = 0,
    NODE_APP_SLOT_RESERVED,
    NODE_APP_SLOT_READY
} NodeApp_RxSlotStateType;

extern volatile uint32_t NodeApp_TxConfirmationCount;
extern volatile Std_ReturnType NodeApp_LastTxResult;
extern volatile uint32_t NodeApp_RxIndicationCount;
extern volatile Std_ReturnType NodeApp_LastRxResult;

/* Reset the application Tx source and the two-slot Rx queue. */
Std_ReturnType NodeApp_Init(void);

/* Copy and submit one application N-SDU through PduR and CanTp. */
Std_ReturnType NodeApp_Transmit(const uint8_t *DataPtr, PduLengthType Length);

/* Copy and consume the oldest READY Rx message. */
Std_ReturnType NodeApp_Receive(uint8_t *DataPtr, PduLengthType Capacity,
                               PduLengthType *LengthPtr);

/* Return the number of complete N-SDUs waiting in the Rx queue. */
uint8_t NodeApp_GetReadyCount(void);

/* PduR-facing application callbacks used by the mock CanTp route. */
BufReq_ReturnType NodeApp_CanTpCopyTxData(GlobalPduIdType GlobalPduId, uint8_t *DestPtr, PduLengthType Length);
void NodeApp_CanTpTxConfirmation(GlobalPduIdType GlobalPduId, Std_ReturnType Result);
BufReq_ReturnType NodeApp_CanTpStartOfReception(GlobalPduIdType GlobalPduId, PduLengthType TotalLength);
BufReq_ReturnType NodeApp_CanTpCopyRxData(GlobalPduIdType GlobalPduId, const uint8_t *DataPtr, PduLengthType Length);
void NodeApp_CanTpRxIndication(GlobalPduIdType GlobalPduId, Std_ReturnType Result);

#endif /* NODE_APP_H_ */
