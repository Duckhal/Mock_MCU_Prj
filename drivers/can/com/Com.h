#ifndef COM_H_
#define COM_H_

#include "Com_Types.h"
#include "Com_Cfg.h"

/* Bounded event history for debugger inspection. */
#define COM_LOG_CAPACITY (16U)
typedef enum
{
    COM_LOG_INIT_OK,
    COM_LOG_CONFIG_ERROR,
    COM_LOG_TX_ACCEPTED,
    COM_LOG_TX_RETRY,
    COM_LOG_TX_DROP,
    COM_LOG_RX_ACCEPTED,
    COM_LOG_RX_REJECTED
} Com_LogEventType;

typedef struct
{
    uint32_t sequence;
    Com_LogEventType event;
    PduIdType ipduId;
    uint32_t detail;
} Com_LogRecordType;

extern volatile Com_LogRecordType Com_LogRecords[COM_LOG_CAPACITY];
extern volatile uint32_t Com_LogSequence;

/* Read-only debugger counters reset by Com_Init(). */
extern volatile uint32_t Com_TxConfirmationCount;
extern volatile uint32_t Com_RxIndicationCount;
extern volatile uint32_t Com_TxDropCount;

/** Validate COM configuration and initialize its buffers and scheduler. */
Std_ReturnType Com_Init(void);

/** Read a caller-owned uint32_t, store the Signal and set U without transmitting. */
Std_ReturnType Com_SendSignal(PduIdType SignalId, const void *SignalDataPtr);

/** Write the latest Rx value to a caller-owned uint32_t; U remains internal. */
Std_ReturnType Com_ReceiveSignal(PduIdType SignalId, void *SignalDataPtr);

/** Return the number of valid Rx I-PDU indications since Com_Init(). */
uint32_t Com_GetRxIndicationCount(void);

/** Run the non-blocking Tx scheduler once per 1 ms. */
void Com_MainFunctionTx(void);

/** Copy one valid Rx I-PDU from PduR into COM's receive buffer. */
void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType *PduInfoPtr);

/** Record completion; Update Bits were already cleared at acceptance. */
void Com_TxConfirmation(PduIdType ComTxPduId);

#endif /* COM_H_ */
