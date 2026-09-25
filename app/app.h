#ifndef APP_H_
#define APP_H_

#include "../drivers/can/common/CanStack_Types.h"

#define APP_MAX_LARGE_MESSAGE_LENGTH (62U)

/* Build one fixed firmware image for each physical ECU. */
#define APP_ROLE_MASTER  (0U)
#define APP_ROLE_SLAVE1  (1U)
#define APP_ROLE_SLAVE2  (2U)

/* Select one fixed role for the firmware image being built. */
#ifndef APP_BOARD_ROLE
#define APP_BOARD_ROLE APP_ROLE_SLAVE1
#endif
#if ((APP_BOARD_ROLE != APP_ROLE_MASTER) && \
     (APP_BOARD_ROLE != APP_ROLE_SLAVE1) && \
     (APP_BOARD_ROLE != APP_ROLE_SLAVE2))
#error "APP_BOARD_ROLE must be MASTER, SLAVE1, or SLAVE2"
#endif

#define APP_KEEPALIVE_LEVEL_COUNT       (7U)
#define APP_MASTER_LOST_TIMEOUT_TICKS   (2000U)
#define APP_SLAVE_OFFLINE_TIMEOUT_TICKS (2000U)
#define APP_IMAGE_MAX_RETRIES           (3U)

#define APP_STATE_ERROR_LIFECYCLE       (1UL << 0U)
#define APP_STATE_ERROR_KEEPALIVE_LEVEL (1UL << 1U)
#define APP_STATE_ERROR_SLAVE_STATUS    (1UL << 2U)

typedef enum
{
    APP_INIT_ERROR_NONE = 0,
    APP_INIT_ERROR_ADC,
    APP_INIT_ERROR_UART,
    APP_INIT_ERROR_UART_RX_BUFFER,
    APP_INIT_ERROR_UART_TX_BUFFER,
    APP_INIT_ERROR_HARDWARE_NOT_READY,
    APP_INIT_ERROR_NODE,
    APP_INIT_ERROR_COM_PROFILE,
    APP_INIT_ERROR_CANTP_PROFILE
} App_InitErrorType;

typedef enum
{
    APP_RUNTIME_OK = 0,
    APP_RUNTIME_NOT_INITIALIZED,
    APP_RUNTIME_STATE_CORRUPTION,
    APP_RUNTIME_ADC_ERROR,
    APP_RUNTIME_COM_SEND_ERROR,
    APP_RUNTIME_COM_RECEIVE_ERROR,
    APP_RUNTIME_CANTP_RECEIVE_ERROR,
    APP_RUNTIME_CANTP_TRANSMIT_ERROR,
    APP_RUNTIME_UART_TX_ERROR,
    APP_RUNTIME_IMAGE_FORMAT_ERROR
} App_RuntimeStatusType;

extern volatile App_InitErrorType g_AppInitError;
extern volatile App_RuntimeStatusType g_AppRuntimeStatus;
extern volatile uint8_t g_AppRole;
extern volatile uint32_t g_AppMainFunctionCount;
extern volatile uint32_t g_AppAdcConversions;
extern volatile uint16_t g_AppAdcValue;
extern volatile uint8_t g_AppAliveCounter;
extern volatile uint8_t g_AppKeepAliveRateLevel;
extern volatile uint32_t g_AppKeepAliveEvents;
extern volatile uint8_t g_AppLastAliveCounter;
extern volatile uint32_t g_AppLastAliveTick;
extern volatile uint8_t g_AppSlaveStatus;
extern volatile uint8_t g_AppSlaveOnline[2];
extern volatile uint8_t g_AppSlaveReportedStatus[2];
extern volatile uint8_t g_AppOnlineSlaveCount;
extern volatile uint32_t g_AppUartErrors;
extern volatile uint32_t g_AppUartRxBytes;
extern volatile uint32_t g_AppUartRxOverflows;
extern volatile uint32_t g_AppUartTxOverflows;
extern volatile uint32_t g_AppCanTpTxRequests;
extern volatile uint32_t g_AppCanTpTxRejects;
extern volatile uint32_t g_AppCanTpTxCompleted;
extern volatile uint32_t g_AppCanTpTxFailures;
extern volatile uint32_t g_AppCanTpRxMessages;
extern volatile uint32_t g_AppCanTpRxErrors;
extern volatile uint32_t g_AppCanTpRxUartDeliveries;
extern volatile uint32_t g_AppCanTpRxIgnored;
extern volatile uint8_t g_AppCanTpTxPending;
extern volatile uint8_t g_AppCanTpInternalLoopback;
extern volatile uint16_t g_AppImageLength;
extern volatile uint16_t g_AppImageBytesQueued;
extern volatile uint32_t g_AppImageChunksCompleted;
extern volatile uint32_t g_AppImageChunksDropped;
extern volatile uint32_t g_AppImageRetryCount;
extern volatile uint32_t g_AppStateCorruptionCount;
extern volatile uint32_t g_AppStateErrorMask;
extern volatile PduLengthType g_AppLastCanTpRxLength;
extern uint8_t g_AppLastCanTpRxData[APP_MAX_LARGE_MESSAGE_LENGTH];

/* Initialize peripherals owned by the application. */
Std_ReturnType App_HardwareInit(void);

/* Initialize the selected compile-time ECU role and application state. */
Std_ReturnType App_Init(void);

/* Execute all role-specific application work for one 1 ms tick. */
Std_ReturnType App_MainFunction(uint32_t Tick);

/* Return non-zero after initialization because every role owns one COM Tx I-PDU. */
uint8_t App_IsComTxEnabled(void);

/* Enable the optional single-board Master UART/CanTp loopback fixture. */
Std_ReturnType App_SetCanTpLoopbackMode(uint8_t Enabled);

/* Submit one Master-owned N-SDU while preserving its source until completion. */
Std_ReturnType App_SendLargeMessage(const uint8_t *DataPtr, PduLengthType Length);

#endif /* APP_H_ */
