#include "PduR.h"
#include "PduR_Cfg.h"
#include "../canif/CanIf.h"
#include "../cantp/Cantp.h"
#include "../cantp/Cantp_Cfg.h"
#include "../com/Com.h"
#include "../../../app/node_app.h"
#include <stddef.h>

volatile uint32_t PduR_TxConfirmationCount;
volatile uint32_t PduR_RxIndicationCount;
volatile PduIdType PduR_LastTxPduId;
volatile PduIdType PduR_LastRxPduId;
volatile PduLengthType PduR_LastRxLength;
volatile uint8_t PduR_LastRxBytes[8];

/** Reset all PduR-owned runtime observations to a deterministic state. */
Std_ReturnType PduR_Init(void)
{
    uint8_t index;

    PduR_TxConfirmationCount = 0U;
    PduR_RxIndicationCount = 0U;
    PduR_LastTxPduId = 0U;
    PduR_LastRxPduId = 0U;
    PduR_LastRxLength = 0U;
    for (index = 0U; index < sizeof(PduR_LastRxBytes); index++)
    {
        PduR_LastRxBytes[index] = 0U;
    }
    return E_OK;
}

/** Snapshot one valid lower-layer Rx callback for debugger inspection. */
static void PduR_SnapshotRx(PduIdType RxPduId,
                            const PduInfoType *PduInfoPtr)
{
    uint8_t b;
    PduR_LastRxPduId = RxPduId;
    PduR_LastRxLength = PduInfoPtr->SduLength;
    for (b = 0U; b < PduInfoPtr->SduLength; b++)
    {
        PduR_LastRxBytes[b] = PduInfoPtr->SduDataPtr[b];
    }
    PduR_RxIndicationCount++;
}

/** Route one COM Tx I-PDU to its configured CanIf Tx L-PDU. */
Std_ReturnType PduR_ComTransmit(PduIdType ComTxPduId,
                               const PduInfoType *PduInfoPtr)
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

/** Route one application global PDU to the configured CanTp Tx N-SDU. */
Std_ReturnType PduR_Transmit(GlobalPduIdType GlobalPduId,
                            const PduInfoType *PduInfoPtr)
{
    uint16_t index;
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL) ||
        (PduInfoPtr->SduLength == 0U) ||
        (PduInfoPtr->SduLength > CANTP_MAX_NSDU_LENGTH))
    {
        return E_NOT_OK;
    }
    for (index = 0U; index < PDUR_NUM_CANTP_TX_ROUTES; index++)
    {
        if (PduR_CanTpTxRouteConfig[index].globalPduId == GlobalPduId)
        {
            return CanTp_Transmit(PduR_CanTpTxRouteConfig[index].txNSduId,
                                  PduInfoPtr);
        }
    }
    return E_NOT_OK;
}

/** Ask the routed application source for the accepted Tx N-SDU snapshot. */
BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType TxNSduId, uint8_t *DestPtr,
                                       PduLengthType Length)
{
    uint16_t index;
    for (index = 0U; index < PDUR_NUM_CANTP_TX_ROUTES; index++)
    {
        if (PduR_CanTpTxRouteConfig[index].txNSduId == TxNSduId)
        {
            return NodeApp_CanTpCopyTxData(
                PduR_CanTpTxRouteConfig[index].globalPduId, DestPtr, Length);
        }
    }
    return BUFREQ_E_NOT_OK;
}

/** Forward one final CanTp Tx result to the routed application source. */
void PduR_CanTpTxConfirmation(PduIdType TxNSduId, Std_ReturnType Result)
{
    uint16_t index;
    for (index = 0U; index < PDUR_NUM_CANTP_TX_ROUTES; index++)
    {
        if (PduR_CanTpTxRouteConfig[index].txNSduId == TxNSduId)
        {
            NodeApp_CanTpTxConfirmation(
                PduR_CanTpTxRouteConfig[index].globalPduId, Result);
            return;
        }
    }
}

/** Reserve one queue slot for a routed CanTp Rx N-SDU. */
BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType RxNSduId,
                                             PduLengthType TotalLength)
{
    uint16_t index;
    for (index = 0U; index < PDUR_NUM_CANTP_RX_ROUTES; index++)
    {
        if (PduR_CanTpRxRouteConfig[index].rxNSduId == RxNSduId)
        {
            return NodeApp_CanTpStartOfReception(
                PduR_CanTpRxRouteConfig[index].globalPduId, TotalLength);
        }
    }
    return BUFREQ_E_NOT_OK;
}

/** Copy one complete reassembled N-SDU into its reserved application slot. */
BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType RxNSduId,
                                       const uint8_t *DataPtr,
                                       PduLengthType Length)
{
    uint16_t index;
    for (index = 0U; index < PDUR_NUM_CANTP_RX_ROUTES; index++)
    {
        if (PduR_CanTpRxRouteConfig[index].rxNSduId == RxNSduId)
        {
            return NodeApp_CanTpCopyRxData(
                PduR_CanTpRxRouteConfig[index].globalPduId, DataPtr, Length);
        }
    }
    return BUFREQ_E_NOT_OK;
}

/** Forward one final CanTp Rx result to the routed application queue. */
void PduR_CanTpRxIndication(PduIdType RxNSduId, Std_ReturnType Result)
{
    uint16_t index;
    for (index = 0U; index < PDUR_NUM_CANTP_RX_ROUTES; index++)
    {
        if (PduR_CanTpRxRouteConfig[index].rxNSduId == RxNSduId)
        {
            NodeApp_CanTpRxIndication(
                PduR_CanTpRxRouteConfig[index].globalPduId, Result);
            return;
        }
    }
}

/** Forward a mapped CanIf Rx PDU synchronously to COM. */
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    uint16_t index;
    uint16_t connection;
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduLength > 8U) ||
        ((PduInfoPtr->SduLength > 0U) && (PduInfoPtr->SduDataPtr == NULL)))
    {
        return;
    }
    for (index = 0U; index < PDUR_NUM_RX_ROUTES; index++)
    {
        if (PduR_RxRouteConfig[index].sourcePduId == RxPduId)
        {
            PduR_SnapshotRx(RxPduId, PduInfoPtr);
            Com_RxIndication(PduR_RxRouteConfig[index].destPduId, PduInfoPtr);
            return;
        }
    }
    for (connection = 0U; connection < CANTP_NUM_CONNECTIONS; connection++)
    {
        const CanTp_ConnectionConfigType *config =
            &CanTp_ConnectionConfig[connection];
        if ((RxPduId == config->canIfRxDataPduId) ||
            (RxPduId == config->canIfRxFcPduId))
        {
            PduIdType rxNPduId = (RxPduId == config->canIfRxDataPduId) ?
                config->rxDataNPduId : config->rxFcNPduId;
            if (PduInfoPtr->SduLength != CANTP_FRAME_LENGTH)
            {
                return;
            }
            PduR_SnapshotRx(RxPduId, PduInfoPtr);
            CanTp_RxIndication(rxNPduId, PduInfoPtr);
            return;
        }
    }
}

/** Forward a mapped CanIf Tx completion to its COM source. */
void PduR_CanIfTxConfirmation(PduIdType TxPduId)
{
    uint16_t index;
    uint16_t connection;
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
    for (connection = 0U; connection < CANTP_NUM_CONNECTIONS; connection++)
    {
        const CanTp_ConnectionConfigType *config =
            &CanTp_ConnectionConfig[connection];
        if ((TxPduId == config->canIfTxDataPduId) ||
            (TxPduId == config->canIfTxFcPduId))
        {
            PduIdType txNPduId = (TxPduId == config->canIfTxDataPduId) ?
                config->txDataNPduId : config->txFcNPduId;
            PduR_LastTxPduId = TxPduId;
            PduR_TxConfirmationCount++;
            CanTp_TxConfirmation(txNPduId);
            return;
        }
    }
}
