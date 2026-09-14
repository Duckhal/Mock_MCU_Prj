#include "pdur_cfg.h"

#include <stddef.h>

#include "../canif/canif_cfg.h"
#include "../comm_matrix_cfg.h"
#include "../com/com_cfg.h"

#define PDUR_ARRAY_COUNT(array) ((uint16_t)(sizeof(array) / sizeof((array)[0])))

/* Direct Tx route: COM owns the source; CanIf owns the destination. */
static const PduR_TxRouteConfigType PduR_TxRoutes[] =
{
    {
        .routeId = PDUR_ROUTE_VEHICLE_STATUS_TX,
        .globalPduId = COMM_MATRIX_GLOBAL_PDU_VEHICLE_STATUS,
        .sourceComIPduId = COM_IPDU_VEHICLE_STATUS,
        .destinationCanIfTxPduId = CANIF_TX_PDU_VEHICLE_STATUS
    }
};

/* Current firmware profile publishes VehicleStatus and has no COM Rx route. */
const PduR_ConfigType PduR_Config =
{
    .txRoutes = PduR_TxRoutes,
    .txRouteCount = PDUR_ARRAY_COUNT(PduR_TxRoutes),
    .rxRoutes = NULL,
    .rxRouteCount = 0U
};
