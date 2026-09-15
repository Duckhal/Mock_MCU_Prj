#ifndef COM_TYPES_H_
#define COM_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include "../common/comm_types.h"

/* COM-local handles used to resolve configured Signals and Signal Groups. */
typedef uint16_t Com_SignalIdType;
typedef uint16_t Com_SignalGroupIdType;

/* Unsigned application value types. */
typedef enum
{
    COM_UINT8 = 0U,
    COM_UINT16,
    COM_UINT32,
    COM_UINT64
} Com_SignalValueType;

/* Defines how one numeric Signal Slot is serialized into I-PDU bytes. */
typedef enum
{
    COM_BYTE_ORDER_LITTLE_ENDIAN = 0U,
    COM_BYTE_ORDER_BIG_ENDIAN
} Com_ByteOrderType;

/* Selects whether an I-PDU is scheduled for Tx or populated from Rx. */
typedef enum
{
    COM_IPDU_TX = 0U,
    COM_IPDU_RX
} Com_IPduDirectionType;

/* COM module lifecycle state; pending is tracked per Tx I-PDU instead. */
typedef enum
{
    COM_UNINIT = 0U,
    COM_INIT
} Com_StatusType;

/* Describes one Signal Slot and its initial logical payload value. */
typedef struct
{
    Com_SignalIdType signalId;
    Com_SignalValueType valueType;
    uint16_t slotStartBit;
    uint16_t slotLengthBits;
    Com_ByteOrderType byteOrder;
    uint64_t initialValue;
} Com_SignalConfigType;

/* Groups the Signal IDs packed into exactly one I-PDU. */
typedef struct
{
    Com_SignalGroupIdType groupId;
    const Com_SignalIdType *signalRefs;
    uint16_t signalCount;
} Com_SignalGroupConfigType;

/* Defines one COM I-PDU, including Tx timing and bounded retry policy. */
typedef struct
{
    PduIdType ipduId;
    GlobalPduIdType globalPduId;
    Com_IPduDirectionType direction;
    uint8_t lengthBytes;
    Com_SignalGroupIdType signalGroupRef;
    uint16_t periodTicks;
    uint16_t initialOffsetTicks;
    uint8_t maxRetries;
} Com_IPduConfigType;

/* Per-I-PDU diagnostic counters exposed without owning any payload data. */
typedef struct
{
    uint32_t txAttempts;
    uint32_t txAccepted;
    uint32_t txRejected;
    uint32_t txDropped;
    uint32_t rxAccepted;
    uint32_t rxRejected;
    uint32_t txConfirmations;
} Com_StatsType;

/* Root immutable COM configuration passed to Com_Init(). */
typedef struct
{
    const Com_SignalConfigType *signals;
    uint16_t signalCount;

    const Com_SignalGroupConfigType *signalGroups;
    uint16_t signalGroupCount;

    const Com_IPduConfigType *ipdus;
    uint16_t ipduCount;
} Com_ConfigType;

#endif /* COM_TYPES_H_ */
