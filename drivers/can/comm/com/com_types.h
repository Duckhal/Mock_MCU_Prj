#ifndef COM_TYPES_H_
#define COM_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include "../common/comm_types.h"

typedef uint16_t Com_SignalIdType;
typedef uint16_t Com_SignalGroupIdType;

typedef enum
{
    COM_UINT8 = 0U,
    COM_UINT16,
    COM_UINT32,
    COM_UINT64
} Com_SignalValueType;

typedef enum
{
    COM_BYTE_ORDER_LITTLE_ENDIAN = 0U,
    COM_BYTE_ORDER_BIG_ENDIAN
} Com_ByteOrderType;

typedef enum
{
    COM_IPDU_TX = 0U,
    COM_IPDU_RX
} Com_IPduDirectionType;

typedef enum
{
    COM_UNINIT = 0U,
    COM_INIT
} Com_StatusType;

typedef struct
{
    Com_SignalIdType signalId;
    Com_SignalValueType valueType;
    uint16_t slotStartBit;
    uint16_t slotLengthBits;
    Com_ByteOrderType byteOrder;
    uint64_t initialValue;
} Com_SignalConfigType;

typedef struct
{
    Com_SignalGroupIdType groupId;
    const Com_SignalIdType *signalRefs;
    uint16_t signalCount;
} Com_SignalGroupConfigType;

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