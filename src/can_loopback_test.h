#ifndef CAN_LOOPBACK_TEST_H_
#define CAN_LOOPBACK_TEST_H_

#include "../drivers/can/common/CanStack_Types.h"

#define CANTP_LOOPBACK_PAYLOAD_LENGTH (62U)

typedef enum
{
    CANTP_LOOPBACK_NOT_STARTED = 0,
    CANTP_LOOPBACK_RUNNING,
    CANTP_LOOPBACK_PASS,
    CANTP_LOOPBACK_FAIL
} CanTpLoopback_StatusType;

typedef enum
{
    CANTP_LOOPBACK_STAGE_IDLE = 0,
    CANTP_LOOPBACK_STAGE_ENABLE_MODE,
    CANTP_LOOPBACK_STAGE_TRANSMIT,
    CANTP_LOOPBACK_STAGE_POLL,
    CANTP_LOOPBACK_STAGE_VERIFY,
    CANTP_LOOPBACK_STAGE_DISABLE_MODE,
    CANTP_LOOPBACK_STAGE_DONE
} CanTpLoopback_StageType;

typedef enum
{
    CANTP_LOOPBACK_ERROR_NONE = 0,
    CANTP_LOOPBACK_ERROR_FREEZE_ENTRY,
    CANTP_LOOPBACK_ERROR_FREEZE_EXIT,
    CANTP_LOOPBACK_ERROR_MODE,
    CANTP_LOOPBACK_ERROR_TRANSMIT,
    CANTP_LOOPBACK_ERROR_TX_RESULT,
    CANTP_LOOPBACK_ERROR_RX_RESULT,
    CANTP_LOOPBACK_ERROR_TIMEOUT,
    CANTP_LOOPBACK_ERROR_RECEIVE,
    CANTP_LOOPBACK_ERROR_LENGTH,
    CANTP_LOOPBACK_ERROR_PAYLOAD
} CanTpLoopback_ErrorType;

typedef struct
{
    CanTpLoopback_StatusType status;
    CanTpLoopback_StageType stage;
    CanTpLoopback_ErrorType error;
    Std_ReturnType transmitResult;
    Std_ReturnType txResult;
    Std_ReturnType rxResult;
    PduLengthType receivedLength;
    uint32_t elapsedTicks;
    uint32_t txConfirmationCount;
    uint32_t rxIndicationCount;
    uint32_t mcr;
    uint32_t ctrl1;
    uint32_t esr1;
    uint32_t iflag1;
    uint8_t expectedData[CANTP_LOOPBACK_PAYLOAD_LENGTH];
    uint8_t receivedData[CANTP_LOOPBACK_PAYLOAD_LENGTH];
} CanTpLoopback_ResultType;

extern volatile CanTpLoopback_ResultType g_CanTpLoopbackTestResult;

/* Run the CanDrv/CanIf/PduR/COM internal-loopback board tests. */
void CanLoopbackTest_Run(void);

/* Run one complete 62-byte CanTp transfer through CAN0 internal loopback. */
Std_ReturnType CanTpLoopbackTest_Run(void);

/* Enable or disable CAN0 internal loopback for interactive board testing. */
Std_ReturnType CanTpLoopbackTest_SetEnabled(uint8_t Enable);

#endif /* CAN_LOOPBACK_TEST_H_ */