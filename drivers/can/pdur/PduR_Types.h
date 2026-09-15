#ifndef PDUR_TYPES_H
#define PDUR_TYPES_H

#include "../common/CanStack_Types.h"

typedef struct
{
    PduIdType sourcePduId;
    PduIdType destPduId;
    GlobalPduIdType globalPduId;
} PduR_TxRouteConfigType;

typedef struct
{
    PduIdType sourcePduId;
    PduIdType destPduId;
    GlobalPduIdType globalPduId;
} PduR_RxRouteConfigType;

#endif /* PDUR_TYPES_H */