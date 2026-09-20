#ifndef CANTP_TYPES_H_
#define CANTP_TYPES_H_

#include "../common/CanStack_Types.h"
#include <stdint.h>

/* Result returned by the future PduR buffer callbacks. */
typedef enum
{
    BUFREQ_OK = 0,
    BUFREQ_E_NOT_OK,
    BUFREQ_E_BUSY,
    BUFREQ_E_OVFL
} BufReq_ReturnType;

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
} CanTp_ConnectionConfigType;

/* Runtime fields reserved for the future transmit state machine. */
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
    uint32_t lastCfConfirmedMs;
    uint32_t dataAttemptDueMs;
    uint32_t txTimerStartMs;
    uint8_t preparedPayloadBytes;
    CanTp_FrameType preparedFrameType;
} CanTp_TxRuntimeType;

/* Runtime fields reserved for the future receive state machine. */
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
    uint32_t fcAttemptDueMs;
    uint32_t fcAcceptedAtMs;
    uint32_t rxCrStartMs;
} CanTp_RxRuntimeType;

#endif /* CANTP_TYPES_H_ */
