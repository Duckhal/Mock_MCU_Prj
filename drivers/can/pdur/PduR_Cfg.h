#ifndef PDUR_CFG_H_
#define PDUR_CFG_H_

#include "PduR_Types.h"

#define PDUR_NUM_TX_ROUTES    (3U)
#define PDUR_NUM_RX_ROUTES    (3U)
#define PDUR_NUM_CANTP_TX_ROUTES (1U)
#define PDUR_NUM_CANTP_RX_ROUTES (1U)

extern const PduR_TxRouteConfigType PduR_TxRouteConfig[PDUR_NUM_TX_ROUTES];
extern const PduR_RxRouteConfigType PduR_RxRouteConfig[PDUR_NUM_RX_ROUTES];
extern const PduR_CanTpTxRouteConfigType PduR_CanTpTxRouteConfig[PDUR_NUM_CANTP_TX_ROUTES];
extern const PduR_CanTpRxRouteConfigType PduR_CanTpRxRouteConfig[PDUR_NUM_CANTP_RX_ROUTES];

#endif /* PDUR_CFG_H_ */
