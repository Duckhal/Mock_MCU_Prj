#ifndef PDUR_CFG_H_
#define PDUR_CFG_H_

#include "../../comm/pdur/pdur_types.h"

/* Static route-table limits enforced later by PduR_Init(). */
#define PDUR_MAX_TX_ROUTE_COUNT    (8U)
#define PDUR_MAX_RX_ROUTE_COUNT    (8U)

/* PduR-local route handles; they need not equal endpoint PDU IDs. */
enum
{
    PDUR_ROUTE_VEHICLE_STATUS_TX = 11U
};

/* Immutable PduR configuration selected by Node_Config. */
extern const PduR_ConfigType PduR_Config;

#endif /* PDUR_CFG_H_ */
