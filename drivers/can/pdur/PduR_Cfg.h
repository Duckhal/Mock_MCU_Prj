#ifndef PDUR_CFG_H
#define PDUR_CFG_H

#include "PduR_Types.h"

#define PDUR_NUM_TX_ROUTES    (1U)
#define PDUR_NUM_RX_ROUTES    (1U)

extern const PduR_TxRouteConfigType PduR_TxRouteConfig[PDUR_NUM_TX_ROUTES];
extern const PduR_RxRouteConfigType PduR_RxRouteConfig[PDUR_NUM_RX_ROUTES];

#endif /* PDUR_CFG_H */