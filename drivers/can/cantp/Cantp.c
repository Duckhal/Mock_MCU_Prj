#include "Cantp.h"
#include "Cantp_Cfg.h"
#include "../canif/CanIf.h"
#include "../pdur/PduR.h"
#include <stddef.h>
#include <string.h>

static uint8_t CanTp_Initialized;
static uint32_t CanTp_NowMs;
static CanTp_TxRuntimeType CanTp_TxRuntime;
static CanTp_RxRuntimeType CanTp_RxRuntime;

volatile CanTp_LogRecordType CanTp_LogRecords[CANTP_LOG_CAPACITY];
volatile uint32_t CanTp_LogSequence;

/** Append one bounded event for deterministic debugger and host-test evidence. */
static void CanTp_Log(CanTp_LogEventType Event, PduIdType PduId,
                      uint32_t State, uint32_t Detail)
{
    uint32_t sequence = CanTp_LogSequence;
    uint32_t index = sequence % CANTP_LOG_CAPACITY;
    CanTp_LogRecords[index].sequence = sequence;
    CanTp_LogRecords[index].event = Event;
    CanTp_LogRecords[index].pduId = PduId;
    CanTp_LogRecords[index].state = State;
    CanTp_LogRecords[index].detail = Detail;
    CanTp_LogSequence = sequence + 1U;
}

/** Return the single dedicated connection used by the Phase-1 baseline. */
static const CanTp_ConnectionConfigType *CanTp_GetConnection(void)
{
    return &CanTp_ConnectionConfig[0];
}

/** Validate fixed limits and every handle pair in its own ID namespace. */
static Std_ReturnType CanTp_ValidateConfig(void)
{
    const CanTp_ConnectionConfigType *config;
    if ((CANTP_NUM_CONNECTIONS != 1U) ||
        (CANTP_FRAME_LENGTH != 8U) ||
        (CANTP_MAX_NSDU_LENGTH > CANTP_CHUNK_CAPACITY) ||
        (CANTP_SF_MIN_LENGTH != 1U) ||
        (CANTP_SF_MAX_LENGTH != 6U) ||
        (CANTP_FF_MIN_LENGTH != 7U) ||
        (CANTP_FF_MAX_LENGTH != 62U) ||
        (CANTP_BLOCK_SIZE == 0U))
    {
        return E_NOT_OK;
    }

    config = CanTp_GetConnection();
    if ((config->txDataNPduId == config->txFcNPduId) ||
        (config->rxDataNPduId == config->rxFcNPduId) ||
        (config->canIfTxDataPduId == config->canIfTxFcPduId) ||
        (config->canIfRxDataPduId == config->canIfRxFcPduId))
    {
        return E_NOT_OK;
    }
    return E_OK;
}

/** Reset the logical Tx session while preserving an accepted pending frame. */
static void CanTp_ResetTxSession(void)
{
    uint8_t pending = CanTp_TxRuntime.txPduPending;
    memset(&CanTp_TxRuntime, 0, sizeof(CanTp_TxRuntime));
    CanTp_TxRuntime.state = CANTP_TX_IDLE;
    CanTp_TxRuntime.nextSN = 1U;
    CanTp_TxRuntime.preparedFrameType = CANTP_FRAME_INVALID;
    CanTp_TxRuntime.txPduPending = pending;
}

/** Reset the logical Rx session while preserving an accepted pending FC. */
static void CanTp_ResetRxSession(void)
{
    uint8_t pending = CanTp_RxRuntime.fcTxPending;
    uint8_t requestActive = CanTp_RxRuntime.fcRequestActive;
    uint8_t frame[CANTP_FRAME_LENGTH];
    memcpy(frame, CanTp_RxRuntime.txFcFrame, sizeof(frame));
    memset(&CanTp_RxRuntime, 0, sizeof(CanTp_RxRuntime));
    CanTp_RxRuntime.state = CANTP_RX_IDLE;
    CanTp_RxRuntime.expectedSN = 1U;
    CanTp_RxRuntime.fcTxPending = pending;
    CanTp_RxRuntime.fcRequestActive = requestActive;
    if ((pending != 0U) || (requestActive != 0U))
    {
        memcpy(CanTp_RxRuntime.txFcFrame, frame, sizeof(frame));
    }
}

/** Report one accepted Tx request as failed and return to the idle state. */
static void CanTp_AbortTx(void)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    uint8_t report = (uint8_t)(CanTp_TxRuntime.resultReported == 0U);
    CanTp_TxRuntime.resultReported = 1U;
    CanTp_Log(CANTP_LOG_ERROR, config->txNSduId,
              (uint32_t)CanTp_TxRuntime.state, CanTp_TxRuntime.txOffset);
    CanTp_ResetTxSession();
    if (report != 0U)
    {
        PduR_CanTpTxConfirmation(config->txNSduId, E_NOT_OK);
    }
}

/** Report successful local completion exactly once and release the Tx session. */
static void CanTp_CompleteTx(void)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    uint8_t report = (uint8_t)(CanTp_TxRuntime.resultReported == 0U);
    CanTp_TxRuntime.resultReported = 1U;
    CanTp_Log(CANTP_LOG_TX_COMPLETE, config->txNSduId,
              (uint32_t)CanTp_TxRuntime.state, CanTp_TxRuntime.txOffset);
    CanTp_ResetTxSession();
    if (report != 0U)
    {
        PduR_CanTpTxConfirmation(config->txNSduId, E_OK);
    }
}

/** Release one started Rx session and notify the application once. */
static void CanTp_AbortRx(void)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    uint8_t report = (uint8_t)((CanTp_RxRuntime.queueSlotReserved != 0U) &&
                               (CanTp_RxRuntime.resultReported == 0U));
    CanTp_RxRuntime.resultReported = 1U;
    CanTp_Log(CANTP_LOG_ERROR, config->rxNSduId,
              (uint32_t)CanTp_RxRuntime.state,
              CanTp_RxRuntime.receivedLength);
    CanTp_ResetRxSession();
    if (report != 0U)
    {
        PduR_CanTpRxIndication(config->rxNSduId, E_NOT_OK);
    }
}

/** Copy one complete N-SDU to the application before publishing Rx success. */
static void CanTp_CompleteRx(void)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    PduLengthType length = CanTp_RxRuntime.totalLength;
    if (PduR_CanTpCopyRxData(config->rxNSduId,
                             CanTp_RxRuntime.rxChunkBuffer,
                             length) != BUFREQ_OK)
    {
        CanTp_AbortRx();
        return;
    }

    CanTp_RxRuntime.resultReported = 1U;
    CanTp_Log(CANTP_LOG_RX_COMPLETE, config->rxNSduId,
              (uint32_t)CanTp_RxRuntime.state, length);
    CanTp_ResetRxSession();
    PduR_CanTpRxIndication(config->rxNSduId, E_OK);
}

/** Build one immutable SF, FF or CF and leave its progress uncommitted. */
static void CanTp_PrepareDataFrame(void)
{
    PduLengthType left =
        (PduLengthType)(CanTp_TxRuntime.totalLength - CanTp_TxRuntime.txOffset);
    uint8_t payloadLength;

    memset(CanTp_TxRuntime.txDataFrame, CANTP_PADDING_BYTE,
           CANTP_FRAME_LENGTH);
    if (CanTp_TxRuntime.totalLength <= CANTP_SF_MAX_LENGTH)
    {
        CanTp_TxRuntime.txDataFrame[0] = CANTP_PCI_SF;
        CanTp_TxRuntime.txDataFrame[1] = CanTp_TxRuntime.totalLength;
        memcpy(&CanTp_TxRuntime.txDataFrame[2],
               CanTp_TxRuntime.txChunkBuffer,
               CanTp_TxRuntime.totalLength);
        payloadLength = CanTp_TxRuntime.totalLength;
        CanTp_TxRuntime.preparedFrameType = CANTP_FRAME_SF;
    }
    else if (CanTp_TxRuntime.txOffset == 0U)
    {
        CanTp_TxRuntime.txDataFrame[0] = CANTP_PCI_FF;
        CanTp_TxRuntime.txDataFrame[1] = CanTp_TxRuntime.totalLength;
        memcpy(&CanTp_TxRuntime.txDataFrame[2],
               CanTp_TxRuntime.txChunkBuffer,
               CANTP_FF_PAYLOAD_LENGTH);
        payloadLength = CANTP_FF_PAYLOAD_LENGTH;
        CanTp_TxRuntime.preparedFrameType = CANTP_FRAME_FF;
    }
    else
    {
        payloadLength = (left < CANTP_CF_PAYLOAD_LENGTH) ?
            left : CANTP_CF_PAYLOAD_LENGTH;
        CanTp_TxRuntime.txDataFrame[0] =
            (uint8_t)(CANTP_PCI_CF |
                      (CanTp_TxRuntime.nextSN & CANTP_PCI_SN_MASK));
        memcpy(&CanTp_TxRuntime.txDataFrame[1],
               &CanTp_TxRuntime.txChunkBuffer[CanTp_TxRuntime.txOffset],
               payloadLength);
        CanTp_TxRuntime.preparedFrameType = CANTP_FRAME_CF;
    }
    CanTp_TxRuntime.preparedPayloadBytes = payloadLength;
    CanTp_TxRuntime.retryCount = 0U;
    CanTp_TxRuntime.state = CANTP_TX_REQUEST_TX;
}

/** Submit the prepared Data frame once; Phase 1 aborts if CanIf rejects it. */
static void CanTp_RequestPreparedDataFrame(void)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    PduInfoType frame =
    {
        CanTp_TxRuntime.txDataFrame,
        CANTP_FRAME_LENGTH
    };
    Std_ReturnType result =
        CanIf_Transmit(config->canIfTxDataPduId, &frame);

    CanTp_Log(CANTP_LOG_TX_FRAME_REQUEST, config->txDataNPduId,
              (uint32_t)CanTp_TxRuntime.state,
              (uint32_t)CanTp_TxRuntime.preparedFrameType);
    if (result == E_OK)
    {
        CanTp_TxRuntime.txPduPending = 1U;
        CanTp_TxRuntime.state = CANTP_TX_WAIT_CONFIRM;
    }
    else
    {
        CanTp_AbortTx();
    }
}

/** Build and submit one fixed CTS frame for the current receive block. */
static Std_ReturnType CanTp_RequestCts(void)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    PduInfoType frame;
    if ((CanTp_RxRuntime.fcRequestActive != 0U) ||
        (CanTp_RxRuntime.fcTxPending != 0U))
    {
        return E_NOT_OK;
    }

    memset(CanTp_RxRuntime.txFcFrame, CANTP_PADDING_BYTE,
           CANTP_FRAME_LENGTH);
    CanTp_RxRuntime.txFcFrame[0] =
        (uint8_t)(CANTP_PCI_FC | CANTP_FC_CTS);
    CanTp_RxRuntime.txFcFrame[1] = CANTP_BLOCK_SIZE;
    CanTp_RxRuntime.txFcFrame[2] = CANTP_STMIN_MS;
    frame.SduDataPtr = CanTp_RxRuntime.txFcFrame;
    frame.SduLength = CANTP_FRAME_LENGTH;
    CanTp_RxRuntime.fcRequestActive = 1U;
    CanTp_RxRuntime.state = CANTP_RX_FC_PENDING;

    if (CanIf_Transmit(config->canIfTxFcPduId, &frame) != E_OK)
    {
        CanTp_RxRuntime.fcRequestActive = 0U;
        return E_NOT_OK;
    }
    CanTp_RxRuntime.fcTxPending = 1U;
    CanTp_Log(CANTP_LOG_FC_REQUEST, config->txFcNPduId,
              (uint32_t)CanTp_RxRuntime.state, CANTP_FC_CTS);
    return E_OK;
}

/** Commit one Data frame only after its matching local confirmation. */
static void CanTp_HandleDataTxConfirmation(void)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    if (CanTp_TxRuntime.txPduPending == 0U)
    {
        return;
    }
    CanTp_TxRuntime.txPduPending = 0U;
    if (CanTp_TxRuntime.state != CANTP_TX_WAIT_CONFIRM)
    {
        return;
    }

    CanTp_TxRuntime.txOffset = (PduLengthType)(
        CanTp_TxRuntime.txOffset + CanTp_TxRuntime.preparedPayloadBytes);
    if (CanTp_TxRuntime.preparedFrameType == CANTP_FRAME_CF)
    {
        CanTp_TxRuntime.nextSN =
            (uint8_t)((CanTp_TxRuntime.nextSN + 1U) & CANTP_PCI_SN_MASK);
        CanTp_TxRuntime.blockCount++;
        CanTp_TxRuntime.priorCfExists = 1U;
        CanTp_TxRuntime.lastCfConfirmedMs = CanTp_NowMs;
    }
    CanTp_Log(CANTP_LOG_TX_FRAME_CONFIRMATION, config->txDataNPduId,
              (uint32_t)CanTp_TxRuntime.state, CanTp_TxRuntime.txOffset);

    if (CanTp_TxRuntime.txOffset == CanTp_TxRuntime.totalLength)
    {
        CanTp_CompleteTx();
    }
    else if ((CanTp_TxRuntime.preparedFrameType == CANTP_FRAME_FF) ||
             (CanTp_TxRuntime.blockCount == CANTP_BLOCK_SIZE))
    {
        CanTp_TxRuntime.fcPermissionGranted = 0U;
        CanTp_TxRuntime.state = CANTP_TX_WAIT_FC;
    }
    else
    {
        CanTp_TxRuntime.state = CANTP_TX_WAIT_STMIN;
    }
}

/** Resolve one matching CTS confirmation before accepting further CF data. */
static void CanTp_HandleFcTxConfirmation(void)
{
    if (CanTp_RxRuntime.fcTxPending == 0U)
    {
        return;
    }
    CanTp_RxRuntime.fcTxPending = 0U;
    CanTp_RxRuntime.fcRequestActive = 0U;
    if (CanTp_RxRuntime.state == CANTP_RX_FC_PENDING)
    {
        CanTp_RxRuntime.blockCount = 0U;
        CanTp_RxRuntime.state = CANTP_RX_WAIT_CF;
    }
}

/** Accept one valid SF into a reserved queue slot and complete immediately. */
static void CanTp_HandleSingleFrame(const PduInfoType *Frame)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    PduLengthType length = Frame->SduDataPtr[1];
    if ((CanTp_RxRuntime.state != CANTP_RX_IDLE) ||
        (length < CANTP_SF_MIN_LENGTH) ||
        (length > CANTP_SF_MAX_LENGTH))
    {
        return;
    }
    if (PduR_CanTpStartOfReception(config->rxNSduId, length) != BUFREQ_OK)
    {
        return;
    }

    memset(&CanTp_RxRuntime, 0, sizeof(CanTp_RxRuntime));
    CanTp_RxRuntime.state = CANTP_RX_IDLE;
    CanTp_RxRuntime.totalLength = length;
    CanTp_RxRuntime.receivedLength = length;
    CanTp_RxRuntime.queueSlotReserved = 1U;
    memcpy(CanTp_RxRuntime.rxChunkBuffer, &Frame->SduDataPtr[2], length);
    CanTp_CompleteRx();
}

/** Start one segmented receive session and request its first CTS. */
static void CanTp_HandleFirstFrame(const PduInfoType *Frame)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    PduLengthType totalLength = Frame->SduDataPtr[1];
    if ((CanTp_RxRuntime.state != CANTP_RX_IDLE) ||
        (totalLength < CANTP_FF_MIN_LENGTH) ||
        (totalLength > CANTP_FF_MAX_LENGTH))
    {
        return;
    }
    if (PduR_CanTpStartOfReception(config->rxNSduId,
                                    totalLength) != BUFREQ_OK)
    {
        return;
    }

    memset(&CanTp_RxRuntime, 0, sizeof(CanTp_RxRuntime));
    CanTp_RxRuntime.totalLength = totalLength;
    CanTp_RxRuntime.receivedLength = CANTP_FF_PAYLOAD_LENGTH;
    CanTp_RxRuntime.expectedSN = 1U;
    CanTp_RxRuntime.queueSlotReserved = 1U;
    memcpy(CanTp_RxRuntime.rxChunkBuffer, &Frame->SduDataPtr[2],
           CANTP_FF_PAYLOAD_LENGTH);
    if (CanTp_RequestCts() != E_OK)
    {
        CanTp_AbortRx();
    }
}

/** Append one in-sequence CF and request another CTS only after a full block. */
static void CanTp_HandleConsecutiveFrame(const PduInfoType *Frame)
{
    PduLengthType remaining;
    PduLengthType realBytes;
    uint8_t sn;
    if (CanTp_RxRuntime.state != CANTP_RX_WAIT_CF)
    {
        return;
    }

    sn = (uint8_t)(Frame->SduDataPtr[0] & CANTP_PCI_SN_MASK);
    if (sn != CanTp_RxRuntime.expectedSN)
    {
        CanTp_AbortRx();
        return;
    }

    remaining = (PduLengthType)(CanTp_RxRuntime.totalLength -
                                CanTp_RxRuntime.receivedLength);
    realBytes = (remaining < CANTP_CF_PAYLOAD_LENGTH) ?
        remaining : CANTP_CF_PAYLOAD_LENGTH;
    memcpy(&CanTp_RxRuntime.rxChunkBuffer[CanTp_RxRuntime.receivedLength],
           &Frame->SduDataPtr[1], realBytes);
    CanTp_RxRuntime.receivedLength =
        (PduLengthType)(CanTp_RxRuntime.receivedLength + realBytes);
    CanTp_RxRuntime.expectedSN =
        (uint8_t)((CanTp_RxRuntime.expectedSN + 1U) & CANTP_PCI_SN_MASK);
    CanTp_RxRuntime.blockCount++;

    if (CanTp_RxRuntime.receivedLength == CanTp_RxRuntime.totalLength)
    {
        CanTp_CompleteRx();
    }
    else if (CanTp_RxRuntime.blockCount == CANTP_BLOCK_SIZE)
    {
        if (CanTp_RequestCts() != E_OK)
        {
            CanTp_AbortRx();
        }
    }
}

/** Grant a new Tx block only for the fixed Phase-1 CTS encoding. */
static void CanTp_HandleFlowControl(const PduInfoType *Frame)
{
    uint8_t flowStatus =
        (uint8_t)(Frame->SduDataPtr[0] & CANTP_PCI_SN_MASK);
    if (CanTp_TxRuntime.state != CANTP_TX_WAIT_FC)
    {
        return;
    }
    if ((flowStatus != CANTP_FC_CTS) ||
        (Frame->SduDataPtr[1] != CANTP_BLOCK_SIZE) ||
        (Frame->SduDataPtr[2] != CANTP_STMIN_MS))
    {
        CanTp_AbortTx();
        return;
    }

    CanTp_TxRuntime.blockCount = 0U;
    CanTp_TxRuntime.fcPermissionGranted = 1U;
    if (CanTp_TxRuntime.priorCfExists == 0U)
    {
        CanTp_PrepareDataFrame();
    }
    else
    {
        CanTp_TxRuntime.state = CANTP_TX_WAIT_STMIN;
    }
}

/** Validate the fixed profile and initialize one independent Tx/Rx session. */
Std_ReturnType CanTp_Init(void)
{
    CanTp_Initialized = 0U;
    CanTp_LogSequence = 0U;
    if (CanTp_ValidateConfig() != E_OK)
    {
        CanTp_Log(CANTP_LOG_CONFIG_ERROR, 0U, 0U, 0U);
        return E_NOT_OK;
    }
    CanTp_NowMs = 0U;
    memset(&CanTp_TxRuntime, 0, sizeof(CanTp_TxRuntime));
    memset(&CanTp_RxRuntime, 0, sizeof(CanTp_RxRuntime));
    CanTp_TxRuntime.state = CANTP_TX_IDLE;
    CanTp_TxRuntime.nextSN = 1U;
    CanTp_TxRuntime.preparedFrameType = CANTP_FRAME_INVALID;
    CanTp_RxRuntime.state = CANTP_RX_IDLE;
    CanTp_RxRuntime.expectedSN = 1U;
    CanTp_Initialized = 1U;
    CanTp_Log(CANTP_LOG_INIT_OK, 0U, 0U, CANTP_NUM_CONNECTIONS);
    return E_OK;
}

/** Accept one N-SDU, snapshot it once and prepare its first immutable frame. */
Std_ReturnType CanTp_Transmit(PduIdType TxNSduId,
                              const PduInfoType *PduInfoPtr)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    if ((CanTp_Initialized == 0U) || (TxNSduId != config->txNSduId) ||
        (PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL) ||
        (PduInfoPtr->SduLength < CANTP_SF_MIN_LENGTH) ||
        (PduInfoPtr->SduLength > CANTP_MAX_NSDU_LENGTH) ||
        (CanTp_TxRuntime.state != CANTP_TX_IDLE) ||
        (CanTp_TxRuntime.txPduPending != 0U))
    {
        return E_NOT_OK;
    }

    memset(&CanTp_TxRuntime, 0, sizeof(CanTp_TxRuntime));
    CanTp_TxRuntime.state = CANTP_TX_PREPARE;
    CanTp_TxRuntime.totalLength = PduInfoPtr->SduLength;
    CanTp_TxRuntime.nextSN = 1U;
    if (PduR_CanTpCopyTxData(TxNSduId,
                              CanTp_TxRuntime.txChunkBuffer,
                              CanTp_TxRuntime.totalLength) != BUFREQ_OK)
    {
        CanTp_AbortTx();
        return E_OK;
    }

    CanTp_PrepareDataFrame();
    CanTp_Log(CANTP_LOG_TX_ACCEPTED, TxNSduId,
              (uint32_t)CanTp_TxRuntime.state,
              CanTp_TxRuntime.totalLength);
    return E_OK;
}

/** Decode one mapped Data or FC N-PDU without retaining the borrowed pointer. */
void CanTp_RxIndication(PduIdType RxNPduId,
                        const PduInfoType *PduInfoPtr)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    uint8_t pci;
    if ((CanTp_Initialized == 0U) || (PduInfoPtr == NULL) ||
        (PduInfoPtr->SduDataPtr == NULL) ||
        (PduInfoPtr->SduLength != CANTP_FRAME_LENGTH))
    {
        return;
    }

    pci = PduInfoPtr->SduDataPtr[0];
    CanTp_Log(CANTP_LOG_RX_FRAME, RxNPduId,
              (uint32_t)CanTp_RxRuntime.state, pci);
    if (RxNPduId == config->rxDataNPduId)
    {
        if (pci == CANTP_PCI_SF)
        {
            CanTp_HandleSingleFrame(PduInfoPtr);
        }
        else if (pci == CANTP_PCI_FF)
        {
            CanTp_HandleFirstFrame(PduInfoPtr);
        }
        else if ((pci & CANTP_PCI_TYPE_MASK) == CANTP_PCI_CF)
        {
            CanTp_HandleConsecutiveFrame(PduInfoPtr);
        }
    }
    else if ((RxNPduId == config->rxFcNPduId) &&
             ((pci & CANTP_PCI_TYPE_MASK) == CANTP_PCI_FC))
    {
        CanTp_HandleFlowControl(PduInfoPtr);
    }
}

/** Dispatch one local confirmation by the configured Data or FC N-PDU ID. */
void CanTp_TxConfirmation(PduIdType TxNPduId)
{
    const CanTp_ConnectionConfigType *config = CanTp_GetConnection();
    if (CanTp_Initialized == 0U)
    {
        return;
    }
    if (TxNPduId == config->txDataNPduId)
    {
        CanTp_HandleDataTxConfirmation();
    }
    else if (TxNPduId == config->txFcNPduId)
    {
        CanTp_HandleFcTxConfirmation();
    }
}

/** Advance the Phase-1 STmin gate and submit at most one Data frame per tick. */
void CanTp_MainFunction(void)
{
    if (CanTp_Initialized == 0U)
    {
        return;
    }
    CanTp_NowMs++;
    if ((CanTp_TxRuntime.state == CANTP_TX_WAIT_STMIN) &&
        (CanTp_TxRuntime.fcPermissionGranted != 0U) &&
        ((CanTp_TxRuntime.priorCfExists == 0U) ||
         ((uint32_t)(CanTp_NowMs -
                     CanTp_TxRuntime.lastCfConfirmedMs) >= CANTP_STMIN_MS)))
    {
        CanTp_PrepareDataFrame();
    }
    if (CanTp_TxRuntime.state == CANTP_TX_REQUEST_TX)
    {
        CanTp_RequestPreparedDataFrame();
    }
}
