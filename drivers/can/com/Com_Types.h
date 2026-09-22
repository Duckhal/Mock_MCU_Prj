#ifndef COM_TYPES_H_
#define COM_TYPES_H_

#include <stdint.h>
#include "../common/CanStack_Types.h"

typedef enum
{
    COM_IPDU_TX = 0,
    COM_IPDU_RX
} Com_IPduDirectionType;

typedef struct
{
    PduIdType signalId;
    PduIdType signalGroupId;

    uint16_t slotStartBit;
    uint16_t slotLength;
    uint8_t useUpdateBit;
} Com_SignalConfigType;

typedef struct
{
    PduIdType signalGroupId;

    const PduIdType *signalList;
    uint8_t numSignals;
} Com_SignalGroupConfigType;

typedef struct
{
    PduIdType ipduId;
    GlobalPduIdType globalPduId;

    Com_IPduDirectionType direction;
    PduLengthType length;

    PduIdType signalGroupId;

    uint16_t periodTicks;
    uint16_t initialOffsetTicks;

    uint8_t maxRetries;
} Com_IPduConfigType;

#endif /* COM_TYPES_H_ */
