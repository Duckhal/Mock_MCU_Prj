#ifndef CANIF_TYPES_H_
#define CANIF_TYPES_H_

#include <stdint.h>

#include "../common/comm_types.h"
#include "../../driver/Can_Types.h"

/* CanIf module lifecycle state. */
typedef enum
{
    CANIF_UNINIT = 0U,
    CANIF_INIT
} CanIf_StatusType;

/* Resolves a logical Tx L-PDU to its CAN identifier and Tx hardware object. */
typedef struct
{
    PduIdType txPduId;
    GlobalPduIdType globalPduId;
    Can_IdType canId;
    Can_HwHandleType hthRef;
    uint8_t lengthBytes;
} CanIf_TxPduConfigType;

/* Resolves an HRH plus CAN identifier to one logical Rx L-PDU. */
typedef struct
{
    PduIdType rxPduId;
    GlobalPduIdType globalPduId;
    Can_IdType canId;
    Can_HwHandleType hrhRef;
    uint8_t lengthBytes;
} CanIf_RxPduConfigType;

/* Root immutable CanIf configuration for Tx and Rx logical L-PDUs. */
typedef struct
{
    const CanIf_TxPduConfigType *txPdus;
    uint16_t txPduCount;
    const CanIf_RxPduConfigType *rxPdus;
    uint16_t rxPduCount;
} CanIf_ConfigType;

/* Module diagnostic counters for requests, callbacks and rejected events. */
typedef struct
{
    uint32_t txCalls;
    uint32_t txAccepted;
    uint32_t txRejected;
    uint32_t txConfirmations;
    uint32_t rxAccepted;
    uint32_t rxDropped;
    uint32_t invalidInput;
    uint32_t staleConfirmation;
    uint32_t busOffEvents;
} CanIf_StatsType;

#endif /* CANIF_TYPES_H_ */
