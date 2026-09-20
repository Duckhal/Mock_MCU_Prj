#include "CanIf.h"
#include "CanIf_Cfg.h"
#include "../can_driver/Can.h"
#include <stddef.h>

#define CANIF_STANDARD_ID_MAX    (0x7FFUL)
#define CANIF_MAX_DATA_LENGTH    (8U)
#define CANIF_LOG_CAPACITY       (16U)

/* PduR must provide these callbacks when the upper communication stack is linked. */
/** Forward a completed local CanIf Tx PDU handle to the PduR route lookup. */
extern void PduR_CanIfTxConfirmation(PduIdType TxPduId);
/** Forward a local Rx PDU and borrowed bytes; PduR must consume/copy them now. */
extern void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

typedef enum
{
    CANIF_LOG_INIT_OK = 0,
    CANIF_LOG_CONFIG_ERROR,
    CANIF_LOG_API_ERROR,
    CANIF_LOG_TX_ACCEPTED,
    CANIF_LOG_TX_FAILED,
    CANIF_LOG_TX_CONFIRMATION,
    CANIF_LOG_RX_INDICATION,
    CANIF_LOG_RX_UNMAPPED
} CanIf_LogEventType;

typedef enum
{
    CANIF_ERROR_NONE = 0,
    CANIF_ERROR_UNINIT,
    CANIF_ERROR_NULL_PDU,
    CANIF_ERROR_UNKNOWN_PDU,
    CANIF_ERROR_CAN_ID,
    CANIF_ERROR_LENGTH,
    CANIF_ERROR_NULL_DATA,
    CANIF_ERROR_HOH,
    CANIF_ERROR_DUPLICATE_PDU,
    CANIF_ERROR_DUPLICATE_CAN_ID,
    CANIF_ERROR_DUPLICATE_RX_MAPPING
} CanIf_ErrorType;

typedef struct
{
    uint32_t sequence;
    CanIf_LogEventType event;
    PduIdType pduId;
    Can_IdType canId;
    Can_HwHandleType hoh;
    uint32_t detail;
} CanIf_LogRecordType;

static uint8_t CanIf_Initialized = 0U;
/* Inspect the most recent structured events in the debugger without a UART. */
static volatile CanIf_LogRecordType CanIf_LogRecords[CANIF_LOG_CAPACITY];
static volatile uint32_t CanIf_LogSequence = 0U;

/** Append an event; detail contains a validation reason or driver return value. */
static void CanIf_Log(CanIf_LogEventType event, PduIdType pduId,
                      Can_IdType canId, Can_HwHandleType hoh, uint32_t detail)
{
    uint32_t sequence = CanIf_LogSequence;
    uint32_t index = sequence % CANIF_LOG_CAPACITY;

    CanIf_LogRecords[index].event = event;
    CanIf_LogRecords[index].pduId = pduId;
    CanIf_LogRecords[index].canId = canId;
    CanIf_LogRecords[index].hoh = hoh;
    CanIf_LogRecords[index].detail = detail;
    CanIf_LogRecords[index].sequence = sequence;
    CanIf_LogSequence = sequence + 1U;
}

/** Resolve one unique HOH of the expected type with one existing controller. */
static const Can_HardwareObjectConfigType *CanIf_GetHardwareObject(
    Can_HwHandleType handle, Can_ObjectType expectedType)
{
    const Can_HardwareObjectConfigType *object = NULL;
    size_t index;
    size_t controllers = 0U;

    for (index = 0U; index < CAN_NUM_HOH; index++)
    {
        if (Can_HardwareObjectConfig[index].objectId == handle)
        {
            if ((object != NULL) ||
                (Can_HardwareObjectConfig[index].objectType != expectedType))
            {
                return NULL;
            }
            object = &Can_HardwareObjectConfig[index];
        }
    }
    if (object == NULL)
    {
        return NULL;
    }
    for (index = 0U; index < CAN_NUM_CONTROLLERS; index++)
    {
        if (Can_ControllerConfig[index].controllerId == object->controllerId)
        {
            controllers++;
        }
    }
    return (controllers == 1U) ? object : NULL;
}

/** Find a Tx entry by its configured local PDU ID, without assuming ID=index. */
static const CanIf_TxPduConfigType *CanIf_GetTxPdu(PduIdType txPduId)
{
    size_t index;
    for (index = 0U; index < CANIF_NUM_TX_PDUS; index++)
    {
        if (CanIf_TxPduConfig[index].txPduId == txPduId)
        {
            return &CanIf_TxPduConfig[index];
        }
    }
    return NULL;
}

/** Resolve the BasicCAN receive key HRH plus CAN ID to one local Rx PDU. */
static const CanIf_RxPduConfigType *CanIf_GetRxPdu(
    Can_HwHandleType hrh, Can_IdType canId)
{
    size_t index;
    for (index = 0U; index < CANIF_NUM_RX_PDUS; index++)
    {
        if ((CanIf_RxPduConfig[index].hrh == hrh) &&
            (CanIf_RxPduConfig[index].canId == canId))
        {
            return &CanIf_RxPduConfig[index];
        }
    }
    return NULL;
}

/** Return whether a Tx L-PDU belongs to CanTp and therefore requires DLC 8. */
static uint8_t CanIf_IsCanTpTxPdu(PduIdType txPduId)
{
    return (uint8_t)((txPduId == CANIF_TX_PDU_CANTP_DATA) ||
                     (txPduId == CANIF_TX_PDU_CANTP_FC));
}

/** Return whether an Rx L-PDU belongs to CanTp and therefore requires DLC 8. */
static uint8_t CanIf_IsCanTpRxPdu(PduIdType rxPduId)
{
    return (uint8_t)((rxPduId == CANIF_RX_PDU_CANTP_DATA) ||
                     (rxPduId == CANIF_RX_PDU_CANTP_FC));
}

/** Validate Tx IDs, Direct Binding uniqueness and references to Tx hardware. */
static Std_ReturnType CanIf_ValidateTxConfig(void)
{
    size_t index;
    size_t previous;
    for (index = 0U; index < CANIF_NUM_TX_PDUS; index++)
    {
        const CanIf_TxPduConfigType *config = &CanIf_TxPduConfig[index];
        CanIf_ErrorType error = CANIF_ERROR_NONE;

        if (config->canId > CANIF_STANDARD_ID_MAX)
        {
            error = CANIF_ERROR_CAN_ID;
        }
        else if (CanIf_GetHardwareObject(config->hth, CAN_OBJECT_TYPE_TX) == NULL)
        {
            error = CANIF_ERROR_HOH;
        }
        for (previous = 0U; (previous < index) && (error == CANIF_ERROR_NONE); previous++)
        {
            if (CanIf_TxPduConfig[previous].txPduId == config->txPduId)
            {
                error = CANIF_ERROR_DUPLICATE_PDU;
            }
            else if (CanIf_TxPduConfig[previous].canId == config->canId)
            {
                error = CANIF_ERROR_DUPLICATE_CAN_ID;
            }
        }
        if (error != CANIF_ERROR_NONE)
        {
            CanIf_Log(CANIF_LOG_CONFIG_ERROR, config->txPduId, config->canId,
                       config->hth, (uint32_t)error);
            return E_NOT_OK;
        }
    }
    return E_OK;
}

/** Validate Rx IDs, unique HRH/CAN ID keys and references to Rx hardware. */
static Std_ReturnType CanIf_ValidateRxConfig(void)
{
    size_t index;
    size_t previous;
    for (index = 0U; index < CANIF_NUM_RX_PDUS; index++)
    {
        const CanIf_RxPduConfigType *config = &CanIf_RxPduConfig[index];
        CanIf_ErrorType error = CANIF_ERROR_NONE;

        if (config->canId > CANIF_STANDARD_ID_MAX)
        {
            error = CANIF_ERROR_CAN_ID;
        }
        else if (CanIf_GetHardwareObject(config->hrh, CAN_OBJECT_TYPE_RX) == NULL)
        {
            error = CANIF_ERROR_HOH;
        }
        for (previous = 0U; (previous < index) && (error == CANIF_ERROR_NONE); previous++)
        {
            if (CanIf_RxPduConfig[previous].rxPduId == config->rxPduId)
            {
                error = CANIF_ERROR_DUPLICATE_PDU;
            }
            else if ((CanIf_RxPduConfig[previous].hrh == config->hrh) &&
                     (CanIf_RxPduConfig[previous].canId == config->canId))
            {
                error = CANIF_ERROR_DUPLICATE_RX_MAPPING;
            }
        }
        if (error != CANIF_ERROR_NONE)
        {
            CanIf_Log(CANIF_LOG_CONFIG_ERROR, config->rxPduId, config->canId,
                       config->hrh, (uint32_t)error);
            return E_NOT_OK;
        }
    }
    return E_OK;
}

/** Validate static tables and publish readiness; hardware init remains in CanDrv. */
Std_ReturnType CanIf_Init(void)
{
    CanIf_Initialized = 0U;
    if ((CanIf_ValidateTxConfig() != E_OK) || (CanIf_ValidateRxConfig() != E_OK))
    {
        return E_NOT_OK;
    }
    CanIf_Initialized = 1U;
    CanIf_Log(CANIF_LOG_INIT_OK, 0U, 0U, 0U, CANIF_ERROR_NONE);
    return E_OK;
}

/** Map one local Tx PDU to CAN ID/HTH and ask the driver exactly once. */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    const CanIf_TxPduConfigType *config;
    Can_PduType canPdu;
    Can_ReturnType result;
    CanIf_ErrorType error = CANIF_ERROR_NONE;

    if (CanIf_Initialized == 0U)
    {
        error = CANIF_ERROR_UNINIT;
    }
    else if (PduInfoPtr == NULL)
    {
        error = CANIF_ERROR_NULL_PDU;
    }
    else if (PduInfoPtr->SduLength > CANIF_MAX_DATA_LENGTH)
    {
        error = CANIF_ERROR_LENGTH;
    }
    else if ((PduInfoPtr->SduLength > 0U) && (PduInfoPtr->SduDataPtr == NULL))
    {
        error = CANIF_ERROR_NULL_DATA;
    }
    if (error != CANIF_ERROR_NONE)
    {
        CanIf_Log(CANIF_LOG_API_ERROR, TxPduId, 0U, 0U, (uint32_t)error);
        return E_NOT_OK;
    }
    config = CanIf_GetTxPdu(TxPduId);
    if (config == NULL)
    {
        CanIf_Log(CANIF_LOG_API_ERROR, TxPduId, 0U, 0U, CANIF_ERROR_UNKNOWN_PDU);
        return E_NOT_OK;
    }
    if ((CanIf_IsCanTpTxPdu(TxPduId) != 0U) &&
        (PduInfoPtr->SduLength != CANIF_MAX_DATA_LENGTH))
    {
        CanIf_Log(CANIF_LOG_API_ERROR, TxPduId, config->canId,
                  config->hth, CANIF_ERROR_LENGTH);
        return E_NOT_OK;
    }
    canPdu.swPduHandle = config->txPduId;
    canPdu.id = config->canId;
    canPdu.length = PduInfoPtr->SduLength;
    canPdu.sdu = PduInfoPtr->SduDataPtr;
    result = Can_Write(config->hth, &canPdu);
    CanIf_Log((result == CAN_OK) ? CANIF_LOG_TX_ACCEPTED : CANIF_LOG_TX_FAILED,
               TxPduId, config->canId, config->hth, (uint32_t)result);
    /* No buffering/retry and no Update Bit modification; COM owns these policies. */
    return (result == CAN_OK) ? E_OK : E_NOT_OK;
}

/** Forward a valid completion handle; the driver owns exactly-once detection. */
void CanIf_TxConfirmation(PduIdType TxPduId)
{
    const CanIf_TxPduConfigType *config;
    if (CanIf_Initialized == 0U)
    {
        CanIf_Log(CANIF_LOG_API_ERROR, TxPduId, 0U, 0U, CANIF_ERROR_UNINIT);
        return;
    }
    config = CanIf_GetTxPdu(TxPduId);
    if (config == NULL)
    {
        CanIf_Log(CANIF_LOG_API_ERROR, TxPduId, 0U, 0U, CANIF_ERROR_UNKNOWN_PDU);
        return;
    }
    CanIf_Log(CANIF_LOG_TX_CONFIRMATION, TxPduId, config->canId,
               config->hth, CANIF_ERROR_NONE);
    PduR_CanIfTxConfirmation(config->txPduId);
}

/** Validate an Rx frame, resolve HRH/CAN ID and synchronously forward its bytes. */
void CanIf_RxIndication(Can_HwHandleType Hrh, const Can_RxPduType *RxPdu)
{
    const CanIf_RxPduConfigType *config;
    PduInfoType pduInfo;
    CanIf_ErrorType error = CANIF_ERROR_NONE;

    if (CanIf_Initialized == 0U)
    {
        error = CANIF_ERROR_UNINIT;
    }
    else if (RxPdu == NULL)
    {
        error = CANIF_ERROR_NULL_PDU;
    }
    else if (RxPdu->canId > CANIF_STANDARD_ID_MAX)
    {
        error = CANIF_ERROR_CAN_ID;
    }
    else if (RxPdu->length > CANIF_MAX_DATA_LENGTH)
    {
        error = CANIF_ERROR_LENGTH;
    }
    else if ((RxPdu->length > 0U) && (RxPdu->dataPtr == NULL))
    {
        error = CANIF_ERROR_NULL_DATA;
    }
    if (error != CANIF_ERROR_NONE)
    {
        CanIf_Log(CANIF_LOG_API_ERROR, 0U, 0U, Hrh, (uint32_t)error);
        return;
    }
    config = CanIf_GetRxPdu(Hrh, RxPdu->canId);
    if (config == NULL)
    {
        CanIf_Log(CANIF_LOG_RX_UNMAPPED, 0U, RxPdu->canId, Hrh, CANIF_ERROR_UNKNOWN_PDU);
        return;
    }
    if ((CanIf_IsCanTpRxPdu(config->rxPduId) != 0U) &&
        (RxPdu->length != CANIF_MAX_DATA_LENGTH))
    {
        CanIf_Log(CANIF_LOG_API_ERROR, config->rxPduId, config->canId,
                  Hrh, CANIF_ERROR_LENGTH);
        return;
    }
    pduInfo.SduDataPtr = RxPdu->dataPtr;
    pduInfo.SduLength = RxPdu->length;
    CanIf_Log(CANIF_LOG_RX_INDICATION, config->rxPduId, config->canId,
               Hrh, CANIF_ERROR_NONE);
    PduR_CanIfRxIndication(config->rxPduId, &pduInfo);
}
