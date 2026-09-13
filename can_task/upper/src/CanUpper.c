#include "../inc/CanUpper.h"
#include "../../driver/inc/Can.h"

#define CANUPPER_TX_PDU_COUNT   1U
#define CANUPPER_RX_PDU_COUNT   1U

/* TX PDU Table */
static CanUpper_TxPduType CanUpper_TxPduTable[CANUPPER_TX_PDU_COUNT] = {
    {
        .pduId  = PDU_TX_LED_COMMAND,
        .canId  = 0x123U,
        .hthId  = CAN_HTH_0,
        .status = PDU_TX_IDLE
    }
};

/* RX PDU Table */
static CanUpper_RxPduType CanUpper_RxPduTable[CANUPPER_RX_PDU_COUNT] = {
    {
        .pduId       = PDU_RX_LED_COMMAND,
        .hrhId       = CAN_HRH_0,
        .data        = {0U},
        .length      = 0U,
        .newDataFlag = 0U
    }
};

void CanUpper_Init(const Can_ConfigType *CanConfig)
{
    uint8_t i;

    /* Initialize lower driver with given configuration */
    Can_Init(CanConfig);

    /* Reset all TX PDU entries to IDLE */
    for (i = 0U; i < CANUPPER_TX_PDU_COUNT; i++)
    {
        CanUpper_TxPduTable[i].status = PDU_TX_IDLE;
    }

    /* Reset all RX PDU entries */
    for (i = 0U; i < CANUPPER_RX_PDU_COUNT; i++)
    {
        CanUpper_RxPduTable[i].length = 0U;
        CanUpper_RxPduTable[i].newDataFlag = 0U;
    }
}

Can_ReturnType CanUpper_Transmit(PduIdType TxPduId, const uint8_t *data, uint8_t length)
{
    uint8_t i;
    CanUpper_TxPduType *txEntry = (void *)0;
    Can_PduType pdu;
    Can_ReturnType ret;

    if ((length > 0U) && (data == (void *)0))
    {
        return CAN_NOT_OK;
    }

    for (i = 0U; i < CANUPPER_TX_PDU_COUNT; i++)
    {
        if (CanUpper_TxPduTable[i].pduId == TxPduId)
        {
            txEntry = &CanUpper_TxPduTable[i];
            break;
        }
    }

    if (txEntry == (void *)0)
    {
        return CAN_NOT_OK;
    }

    if (txEntry->status == PDU_TX_PENDING)
    {
        return CAN_BUSY;
    }

    pdu.id = txEntry->canId;
    pdu.length = length;
    pdu.sdu = data;

    ret = Can_Write(txEntry->hthId, &pdu);
    if (ret == CAN_OK)
    {
        txEntry->status = PDU_TX_PENDING;
    }

    return ret;
}

void CanUpper_TxConfirmation(uint8_t HthId)
{
    uint8_t i;

    for(i = 0U; i < CANUPPER_TX_PDU_COUNT; i++)
    {
        if (CanUpper_TxPduTable[i].hthId == HthId)
        {
            CanUpper_TxPduTable[i].status = PDU_TX_DONE;
            return;
        }
    }
}

void CanUpper_RxIndication(const Can_HwType *Mailbox, const Can_PduType *PduInfoPtr)
{
    uint8_t i;
    CanUpper_RxPduType *rxEntry = (void *)0;
    uint8_t byteIdx;
    uint8_t copyLen;

    /* Validate input pointers */
    if ((Mailbox == (void *)0) || (PduInfoPtr == (void *)0))
    {
        return;
    }

    /* Find matching RX PDU entry by HRH ID */
    for (i = 0U; i < CANUPPER_RX_PDU_COUNT; i++)
    {
        if (CanUpper_RxPduTable[i].hrhId == Mailbox->hohId)
        {
            rxEntry = &CanUpper_RxPduTable[i];
            break;
        }
    }

    /* Store received payload if entry found */
    if (rxEntry != (void *)0)
    {
        copyLen = PduInfoPtr->length;
        if (copyLen > 8U)
        {
            copyLen = 8U;
        }

        /* Copy payload bytes into PDU buffer */
        if ((copyLen > 0U) && (PduInfoPtr->sdu != (void *)0))
        {
            for (byteIdx = 0U; byteIdx < copyLen; byteIdx++)
            {
                rxEntry->data[byteIdx] = PduInfoPtr->sdu[byteIdx];
            }
        }

        rxEntry->length = copyLen;
        rxEntry->newDataFlag = 1U;
    }
}

CanUpper_TxStatusType CanUpper_GetTxStatus(PduIdType TxPduId)
{
    uint8_t i;

    /* Search for matching TX PDU ID */
    for (i = 0U; i < CANUPPER_TX_PDU_COUNT; i++)
    {
        if (CanUpper_TxPduTable[i].pduId == TxPduId)
        {
            return CanUpper_TxPduTable[i].status;
        }
    }

    /* Return default status if not found */
    return PDU_TX_IDLE;
}
Can_ReturnType CanUpper_GetRxData(PduIdType RxPduId, uint8_t *dataOut, uint8_t *lengthOut)
{
    uint8_t i;
    uint8_t byteIdx;
    CanUpper_RxPduType *rxEntry = (void *)0;

    /* Validate output pointers */
    if ((dataOut == (void *)0) || (lengthOut == (void *)0))
    {
        return CAN_NOT_OK;
    }

    /* Find matching RX PDU entry */
    for (i = 0U; i < CANUPPER_RX_PDU_COUNT; i++)
    {
        if (CanUpper_RxPduTable[i].pduId == RxPduId)
        {
            rxEntry = &CanUpper_RxPduTable[i];
            break;
        }
    }

    /* Return error if PDU ID is not found */
    if (rxEntry == (void *)0)
    {
        return CAN_NOT_OK;
    }

    /* Return error if no unread data is present */
    if (rxEntry->newDataFlag == 0U)
    {
        return CAN_NOT_OK;
    }

    /* Copy buffered payload to caller */
    for (byteIdx = 0U; byteIdx < rxEntry->length; byteIdx++)
    {
        dataOut[byteIdx] = rxEntry->data[byteIdx];
    }

    /* Set output length and clear new data flag */
    *lengthOut = rxEntry->length;
    rxEntry->newDataFlag = 0U;

    /* Return success */
    return CAN_OK;

}

void CanUpper_MainFunction(void)
{
    Can_MainFunction_Write();
    Can_MainFunction_Read();
}
