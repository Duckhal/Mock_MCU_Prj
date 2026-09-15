#ifndef COM_H_
#define COM_H_

#include "com_types.h"

/* Validates the configuration and initializes all COM runtime data. */
Comm_ReturnType Com_Init(const Com_ConfigType *config);

/* Updates one Tx Signal value and sets its Update Bit without transmitting. */
Comm_ReturnType Com_SendSignal(Com_SignalIdType signalId, const void *signalData, 
                                uint16_t signalDataSize);

/* Copies the latest valid Rx Signal value to the caller. */
Comm_ReturnType Com_ReceiveSignal(Com_SignalIdType signalId, void *signalDataOut,
                                    uint16_t outputCapacity);

/* Advances every Tx I-PDU scheduler by exactly one 1 ms tick. */
void Com_MainFunctionTx(void);

/* Receives and atomically commits one I-PDU supplied by PduR. */
void Com_RxIndication(PduIdType rxIPduId, const PduInfoType *pduInfo);

/* Records the final lower-layer result for an accepted Tx request. */
void Com_TxConfirmation(PduIdType txIPduId, Comm_ReturnType result);

/* Copies the diagnostic counters of one configured I-PDU. */
Comm_ReturnType Com_GetStats(PduIdType ipduId, Com_StatsType *statsOut);

#endif /* COM_H_ */