#include "PduR.h"
#include "PduR_Cfg.h"
#include "../canif/CanIf.h"
#include "../com/Com.h"
#include <stddef.h>

volatile uint32_t PduR_TxConfirmationCount;
volatile uint32_t PduR_RxIndicationCount;
volatile PduIdType PduR_LastTxPduId;
volatile PduIdType PduR_LastRxPduId;
volatile PduLengthType PduR_LastRxLength;
volatile uint8_t PduR_LastRxBytes[8];

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

/** Forward a mapped CanIf Rx PDU synchronously to COM. */
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    uint16_t index;
    uint8_t b;
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduLength > 8U) ||
        ((PduInfoPtr->SduLength > 0U) && (PduInfoPtr->SduDataPtr == NULL)))
    {
        return;
    }
    for (index = 0U; index < PDUR_NUM_RX_ROUTES; index++)
    {
        if (PduR_RxRouteConfig[index].sourcePduId == RxPduId)
        {
            PduR_LastRxPduId = RxPduId;
            PduR_LastRxLength = PduInfoPtr->SduLength;
            for (b = 0U; b < PduInfoPtr->SduLength; b++)
            { PduR_LastRxBytes[b] = PduInfoPtr->SduDataPtr[b]; }
            PduR_RxIndicationCount++;
            Com_RxIndication(PduR_RxRouteConfig[index].destPduId, PduInfoPtr);
            return;
        }
    }
}

/** Forward a mapped CanIf Tx completion to its COM source. */
void PduR_CanIfTxConfirmation(PduIdType TxPduId)
{
    uint16_t index;
    for (index = 0U; index < PDUR_NUM_TX_ROUTES; index++)
    {
        if (PduR_TxRouteConfig[index].destPduId == TxPduId)
        {
            PduR_LastTxPduId = TxPduId;
            PduR_TxConfirmationCount++;
            Com_TxConfirmation(PduR_TxRouteConfig[index].sourcePduId);
            return;
        }
    }
}
