#include "node_app.h"
#include "../drivers/can/common/CanStack_Cfg.h"
#include "../drivers/can/pdur/PduR.h"
#include <stddef.h>
#include <string.h>

static uint8_t NodeApp_TxSource[NODE_APP_NSDU_CAPACITY];
static PduLengthType NodeApp_TxLength;
static uint8_t NodeApp_TxActive;

static uint8_t NodeApp_RxData[NODE_APP_RX_QUEUE_SIZE][NODE_APP_NSDU_CAPACITY];
static PduLengthType NodeApp_RxLength[NODE_APP_RX_QUEUE_SIZE];
static NodeApp_RxSlotStateType NodeApp_RxState[NODE_APP_RX_QUEUE_SIZE];
static uint8_t NodeApp_RxReservedIndex;
static uint8_t NodeApp_RxReservationActive;
static uint8_t NodeApp_RxReadIndex;
static uint8_t NodeApp_RxWriteIndex;

volatile uint32_t NodeApp_TxConfirmationCount;
volatile Std_ReturnType NodeApp_LastTxResult;
volatile uint32_t NodeApp_RxIndicationCount;
volatile Std_ReturnType NodeApp_LastRxResult;

/* Reset all application-owned transport buffers and observable results. */
Std_ReturnType NodeApp_Init(void)
{
    memset(NodeApp_TxSource, 0, sizeof(NodeApp_TxSource));
    memset(NodeApp_RxData, 0, sizeof(NodeApp_RxData));
    memset(NodeApp_RxLength, 0, sizeof(NodeApp_RxLength));
    memset(NodeApp_RxState, 0, sizeof(NodeApp_RxState));
    NodeApp_TxLength = 0U;
    NodeApp_TxActive = 0U;
    NodeApp_RxReservedIndex = 0U;
    NodeApp_RxReservationActive = 0U;
    NodeApp_RxReadIndex = 0U;
    NodeApp_RxWriteIndex = 0U;
    NodeApp_TxConfirmationCount = 0U;
    NodeApp_LastTxResult = E_NOT_OK;
    NodeApp_RxIndicationCount = 0U;
    NodeApp_LastRxResult = E_NOT_OK;
    return E_OK;
}

/* Own a stable application Tx source before asking PduR to route it. */
Std_ReturnType NodeApp_Transmit(const uint8_t *DataPtr, PduLengthType Length)
{
    PduInfoType request;
    if ((DataPtr == NULL) || (Length == 0U) ||
        (Length > NODE_APP_MAX_NSDU_LENGTH) || (NodeApp_TxActive != 0U))
    {
        return E_NOT_OK;
    }

    memcpy(NodeApp_TxSource, DataPtr, Length);
    NodeApp_TxLength = Length;
    NodeApp_TxActive = 1U;
    request.SduDataPtr = NodeApp_TxSource;
    request.SduLength = Length;
    if (PduR_Transmit(GLOBAL_PDU_CANTP_DATA, &request) != E_OK)
    {
        NodeApp_TxActive = 0U;
        NodeApp_TxLength = 0U;
        return E_NOT_OK;
    }
    return E_OK;
}

/* Consume one complete queue entry without exposing internal slot storage. */
Std_ReturnType NodeApp_Receive(uint8_t *DataPtr, PduLengthType Capacity,
                               PduLengthType *LengthPtr)
{
    uint8_t index = NodeApp_RxReadIndex;
    if ((DataPtr == NULL) || (LengthPtr == NULL))
    {
        return E_NOT_OK;
    }
    if (NodeApp_RxState[index] == NODE_APP_SLOT_READY)
    {
        if (Capacity < NodeApp_RxLength[index])
        {
            return E_NOT_OK;
        }
        memcpy(DataPtr, NodeApp_RxData[index], NodeApp_RxLength[index]);
        *LengthPtr = NodeApp_RxLength[index];
        NodeApp_RxLength[index] = 0U;
        NodeApp_RxState[index] = NODE_APP_SLOT_FREE;
        NodeApp_RxReadIndex =
            (uint8_t)((index + 1U) % NODE_APP_RX_QUEUE_SIZE);
        return E_OK;
    }
    return E_NOT_OK;
}

/* Count complete messages; RESERVED partial data is deliberately invisible. */
uint8_t NodeApp_GetReadyCount(void)
{
    uint8_t index;
    uint8_t count = 0U;
    for (index = 0U; index < NODE_APP_RX_QUEUE_SIZE; index++)
    {
        if (NodeApp_RxState[index] == NODE_APP_SLOT_READY)
        {
            count++;
        }
    }
    return count;
}

/* Copy the stable application Tx source into CanTp exactly once. */
BufReq_ReturnType NodeApp_CanTpCopyTxData(GlobalPduIdType GlobalPduId,
                                          uint8_t *DestPtr,
                                          PduLengthType Length)
{
    if ((GlobalPduId != GLOBAL_PDU_CANTP_DATA) || (DestPtr == NULL) ||
        (NodeApp_TxActive == 0U) || (Length != NodeApp_TxLength))
    {
        return BUFREQ_E_NOT_OK;
    }
    memcpy(DestPtr, NodeApp_TxSource, Length);
    return BUFREQ_OK;
}

/* Release the application Tx source after the one final CanTp result. */
void NodeApp_CanTpTxConfirmation(GlobalPduIdType GlobalPduId,
                                 Std_ReturnType Result)
{
    if ((GlobalPduId == GLOBAL_PDU_CANTP_DATA) && (NodeApp_TxActive != 0U))
    {
        NodeApp_TxActive = 0U;
        if (Result == E_OK)
        {
            NodeApp_TxLength = 0U;
        }
        NodeApp_LastTxResult = Result;
        NodeApp_TxConfirmationCount++;
    }
}

/* Reserve one FREE queue slot without exposing partial data as READY. */
BufReq_ReturnType NodeApp_CanTpStartOfReception(GlobalPduIdType GlobalPduId,
                                                PduLengthType TotalLength)
{
    uint8_t index = NodeApp_RxWriteIndex;
    if ((GlobalPduId != GLOBAL_PDU_CANTP_DATA) || (TotalLength == 0U) ||
        (TotalLength > NODE_APP_MAX_NSDU_LENGTH) ||
        (NodeApp_RxReservationActive != 0U))
    {
        return BUFREQ_E_NOT_OK;
    }
    if (NodeApp_RxState[index] == NODE_APP_SLOT_FREE)
    {
        NodeApp_RxState[index] = NODE_APP_SLOT_RESERVED;
        NodeApp_RxLength[index] = TotalLength;
        NodeApp_RxReservedIndex = index;
        NodeApp_RxReservationActive = 1U;
        return BUFREQ_OK;
    }
    return BUFREQ_E_OVFL;
}

/* Publish a queue slot only after the complete N-SDU has been copied. */
BufReq_ReturnType NodeApp_CanTpCopyRxData(GlobalPduIdType GlobalPduId,
                                          const uint8_t *DataPtr,
                                          PduLengthType Length)
{
    uint8_t index = NodeApp_RxReservedIndex;
    if ((GlobalPduId != GLOBAL_PDU_CANTP_DATA) || (DataPtr == NULL) ||
        (NodeApp_RxReservationActive == 0U) ||
        (NodeApp_RxState[index] != NODE_APP_SLOT_RESERVED) ||
        (Length != NodeApp_RxLength[index]))
    {
        return BUFREQ_E_NOT_OK;
    }
    memcpy(NodeApp_RxData[index], DataPtr, Length);
    NodeApp_RxState[index] = NODE_APP_SLOT_READY;
    return BUFREQ_OK;
}

/* Finalize a receive session and release its reservation on failure. */
void NodeApp_CanTpRxIndication(GlobalPduIdType GlobalPduId,
                               Std_ReturnType Result)
{
    uint8_t index = NodeApp_RxReservedIndex;
    if ((GlobalPduId != GLOBAL_PDU_CANTP_DATA) ||
        (NodeApp_RxReservationActive == 0U))
    {
        return;
    }

    if ((Result != E_OK) || (NodeApp_RxState[index] != NODE_APP_SLOT_READY))
    {
        NodeApp_RxState[index] = NODE_APP_SLOT_FREE;
        NodeApp_RxLength[index] = 0U;
        Result = E_NOT_OK;
    }
    NodeApp_RxReservationActive = 0U;
    if (Result == E_OK)
    {
        NodeApp_RxWriteIndex =
            (uint8_t)((index + 1U) % NODE_APP_RX_QUEUE_SIZE);
    }
    NodeApp_LastRxResult = Result;
    NodeApp_RxIndicationCount++;
}
