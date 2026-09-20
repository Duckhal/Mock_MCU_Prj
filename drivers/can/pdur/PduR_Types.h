#ifndef PDUR_TYPES_H_
#define PDUR_TYPES_H_

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

typedef struct
{
    GlobalPduIdType globalPduId;
    PduIdType txNSduId;
} PduR_CanTpTxRouteConfigType;

typedef struct
{
    PduIdType rxNSduId;
    GlobalPduIdType globalPduId;
} PduR_CanTpRxRouteConfigType;

#endif /* PDUR_TYPES_H_ */
