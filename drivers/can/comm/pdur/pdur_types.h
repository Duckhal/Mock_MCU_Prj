#ifndef PDUR_TYPES_H_
#define PDUR_TYPES_H_

#include <stdint.h>

#include "../common/comm_types.h"

/* PduR-local handle used to identify one configured route. */
typedef uint16_t PduR_RouteIdType;

/* PduR module lifecycle state. */
typedef enum
{
    PDUR_UNINIT = 0U,
    PDUR_INIT
} PduR_StatusType;

/* Maps one COM Tx I-PDU to one CanIf Tx L-PDU. */
typedef struct
{
    PduR_RouteIdType routeId;
    GlobalPduIdType globalPduId;
    PduIdType sourceComIPduId;
    PduIdType destinationCanIfTxPduId;
} PduR_TxRouteConfigType;

/* Maps one CanIf Rx L-PDU to one COM Rx I-PDU. */
typedef struct
{
    PduR_RouteIdType routeId;
    GlobalPduIdType globalPduId;
    PduIdType sourceCanIfRxPduId;
    PduIdType destinationComIPduId;
} PduR_RxRouteConfigType;

/* Root immutable PduR configuration containing direction-specific routes. */
typedef struct
{
    const PduR_TxRouteConfigType *txRoutes;
    uint16_t txRouteCount;
    const PduR_RxRouteConfigType *rxRoutes;
    uint16_t rxRouteCount;
} PduR_ConfigType;

/* Module diagnostic counters for routed, rejected and unresolved requests. */
typedef struct
{
    uint32_t txCalls;
    uint32_t txAccepted;
    uint32_t txRejected;
    uint32_t rxRouted;
    uint32_t confirmationsRouted;
    uint32_t invalidInput;
    uint32_t routeMiss;
} PduR_StatsType;

#endif /* PDUR_TYPES_H_ */
