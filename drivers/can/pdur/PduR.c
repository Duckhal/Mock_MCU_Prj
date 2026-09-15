#include "PduR.h"
#include "PduR_Cfg.h"
#include "../canif/CanIf.h"
#include <stddef.h>

Std_ReturnType PduR_ComTransmit(PduIdType ComTxPduId, const PduInfoType *PduInfoPtr)
{
    uint16_t index;

    if (PduInfoPtr == NULL)
    {
        return E_NOT_OK;
    }

    for (index = 0U; index < PDUR_NUM_TX_ROUTES; index++)
    {
        if (PduR_TxRouteConfig[index].sourcePduId == ComTxPduId)
        {
            return CanIf_Transmit(PduR_TxRouteConfig[index].destPduId, PduInfoPtr);
        }
    }

    return E_NOT_OK;
}