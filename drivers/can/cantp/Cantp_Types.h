#ifndef CANTP_TYPES_H_
#define CANTP_TYPES_H_

#include "../common/CanStack_Types.h"
#include <stdint.h>

/* Transmit states required by the mock CanTp state machine. */
typedef enum
{
    CANTP_TX_IDLE = 0,
    CANTP_TX_PREPARE,
    CANTP_TX_REQUEST_TX,
    CANTP_TX_WAIT_CONFIRM,
    CANTP_TX_WAIT_FC,
    CANTP_TX_WAIT_STMIN
} CanTp_TxStateType;

/* Receive states required by the mock CanTp state machine. */
typedef enum
{
    CANTP_RX_IDLE = 0,
    CANTP_RX_FC_PENDING,
    CANTP_RX_WAIT_CF
} CanTp_RxStateType;

/* Frame classes encoded in the upper nibble or first PCI byte. */
typedef enum
{
    CANTP_FRAME_SF = 0,
    CANTP_FRAME_FF,
    CANTP_FRAME_CF,
    CANTP_FRAME_FC,
    CANTP_FRAME_INVALID
} CanTp_FrameType;

/* Supported Flow Control status values. */
typedef enum
{
    CANTP_FC_CTS = 0,
    CANTP_FC_OVFLW = 2
} CanTp_FlowStatusType;

/* Observable reasons for the final Phase-2 session abort. */
typedef enum
{
    CANTP_ABORT_NONE = 0,
    CANTP_ABORT_DATA_RETRY_EXHAUSTED,
    CANTP_ABORT_FC_RETRY_EXHAUSTED,
    CANTP_ABORT_N_AS_TIMEOUT,
    CANTP_ABORT_N_AR_TIMEOUT,
    CANTP_ABORT_N_BS_TIMEOUT,
    CANTP_ABORT_N_CR_TIMEOUT,
    CANTP_ABORT_INVALID_FLOW_CONTROL,
    CANTP_ABORT_SEQUENCE_NUMBER,
    CANTP_ABORT_COPY_FAILURE
} CanTp_AbortReasonType;

/*
 * Static mapping for one dedicated bidirectional mock CanTp connection.
 * N-SDU, N-PDU and CanIf L-PDU handles are separate namespaces.
 */
typedef struct
{
    PduIdType txNSduId;
    PduIdType rxNSduId;
    PduIdType txDataNPduId;
    PduIdType rxDataNPduId;
    PduIdType txFcNPduId;
    PduIdType rxFcNPduId;
    PduIdType canIfTxDataPduId;
    PduIdType canIfTxFcPduId;
    PduIdType canIfRxDataPduId;
    PduIdType canIfRxFcPduId;
} CanTp_ConnectionConfigType;

/* Runtime data for one Phase-2 transmit session. */
typedef struct
{
    CanTp_TxStateType state;
    uint8_t txChunkBuffer[64U];
    uint8_t txDataFrame[8U];
    PduLengthType totalLength;
    PduLengthType txOffset;
    uint8_t nextSN;
    uint8_t blockCount;
    uint8_t retryCount;
    uint8_t txPduPending;
    uint8_t resultReported;
    uint8_t priorCfExists;
    uint8_t fcPermissionGranted;
    uint8_t nAsActive;
    uint8_t nBsActive;
    uint32_t lastCfConfirmedMs;
    uint32_t dataAttemptDueMs;
    uint32_t nAsStartMs;
    uint32_t nBsStartMs;
    uint8_t preparedPayloadBytes;
    CanTp_FrameType preparedFrameType;
} CanTp_TxRuntimeType;

/* Runtime data for one Phase-2 receive session and its FC resource. */
typedef struct
{
    CanTp_RxStateType state;
    uint8_t rxChunkBuffer[64U];
    uint8_t txFcFrame[8U];
    PduLengthType totalLength;
    PduLengthType receivedLength;
    uint8_t expectedSN;
    uint8_t blockCount;
    uint8_t fcRetryCount;
    uint8_t queueSlotReserved;
    uint8_t fcRequestActive;
    uint8_t fcTxPending;
    uint8_t resultReported;
    uint8_t nArActive;
    uint8_t nCrActive;
    uint32_t fcAttemptDueMs;
    uint32_t nArStartMs;
    uint32_t nCrStartMs;
} CanTp_RxRuntimeType;

/* Structured Phase-2 events retained for debugger inspection. */
typedef enum
{
    CANTP_LOG_INIT_OK = 0,
    CANTP_LOG_CONFIG_ERROR,
    CANTP_LOG_TX_ACCEPTED,
    CANTP_LOG_TX_FRAME_REQUEST,
    CANTP_LOG_TX_FRAME_CONFIRMATION,
    CANTP_LOG_TX_COMPLETE,
    CANTP_LOG_RX_FRAME,
    CANTP_LOG_FC_REQUEST,
    CANTP_LOG_RX_COMPLETE,
    CANTP_LOG_RETRY,
    CANTP_LOG_TIMEOUT,
    CANTP_LOG_ABORT,
    CANTP_LOG_ERROR
} CanTp_LogEventType;

typedef struct
{
    uint32_t sequence;
    CanTp_LogEventType event;
    PduIdType pduId;
    uint32_t state;
    uint32_t detail;
} CanTp_LogRecordType;

#endif /* CANTP_TYPES_H_ */
