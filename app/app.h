#ifndef APP_H_
#define APP_H_

#include "../drivers/can/common/CanStack_Types.h"

#define APP_MAX_LARGE_MESSAGE_LENGTH (62U)

#define APP_ROLE_RX (0U)
#define APP_ROLE_TX (1U)

/* Select one fixed role for each firmware image before building. */
#ifndef APP_BOARD_ROLE
#define APP_BOARD_ROLE APP_ROLE_TX
#endif
#if ((APP_BOARD_ROLE != APP_ROLE_RX) && (APP_BOARD_ROLE != APP_ROLE_TX))
#error "APP_BOARD_ROLE must be APP_ROLE_RX or APP_ROLE_TX"
#endif

#define APP_STATE_ERROR_LIFECYCLE   (1UL << 0U)
#define APP_STATE_ERROR_LED_COMMAND (1UL << 1U)

typedef enum
{
    APP_INIT_ERROR_NONE = 0,
    APP_INIT_ERROR_ADC,
    APP_INIT_ERROR_UART,
    APP_INIT_ERROR_UART_BUFFER,
    APP_INIT_ERROR_HARDWARE_NOT_READY,
    APP_INIT_ERROR_NODE
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
    APP_RUNTIME_CANTP_TX_TIMEOUT,
    APP_RUNTIME_UART_TX_ERROR
} App_RuntimeStatusType;

extern volatile App_InitErrorType g_AppInitError;
extern volatile App_RuntimeStatusType g_AppRuntimeStatus;
extern volatile uint8_t g_AppModeTx;
extern volatile uint32_t g_AppMainFunctionCount;
extern volatile uint32_t g_AppTxSignalUpdates;
extern volatile uint32_t g_AppRxCommands;
extern volatile uint32_t g_AppInvalidRxCommands;
extern volatile uint32_t g_AppAdcConversions;
extern volatile uint16_t g_AppAdcValue;
extern volatile uint8_t g_AppLedMode;
extern volatile uint8_t g_AppLedState;
extern volatile uint32_t g_AppUartErrors;
extern volatile uint32_t g_AppCanTpTxRequests;
extern volatile uint32_t g_AppCanTpTxRejects;
extern volatile uint32_t g_AppCanTpRxMessages;
extern volatile uint32_t g_AppCanTpRxErrors;
extern volatile uint32_t g_AppUartRxBytes;
extern volatile uint32_t g_AppUartRxOverflows;
extern volatile uint32_t g_AppCanTpTxCompleted;
extern volatile uint32_t g_AppCanTpTxFailures;
extern volatile uint32_t g_AppCanTpRxUartDeliveries;
extern volatile uint32_t g_AppCanTpRxIgnored;
extern volatile uint8_t g_AppCanTpTxPending;
extern volatile uint8_t g_AppCanTpInternalLoopback;
extern volatile uint32_t g_AppStateCorruptionCount;
extern volatile uint32_t g_AppStateErrorMask;
extern volatile PduLengthType g_AppLastCanTpRxLength;
extern uint8_t g_AppLastCanTpRxData[APP_MAX_LARGE_MESSAGE_LENGTH];

/** Initialize the ADC, LED, and UART peripherals owned by the application. */
Std_ReturnType App_HardwareInit(void);

/** Initialize application state and CanTp-owned buffers. */
Std_ReturnType App_Init(void);

/** Execute ADC, COM, LED, UART, and CanTp application work for one tick. */
Std_ReturnType App_MainFunction(uint32_t Tick);

/** Return non-zero when the application permits periodic COM transmission. */
uint8_t App_IsComTxEnabled(void);

/** Enable the single-board UART/CanTp echo fixture after CAN loopback starts. */
Std_ReturnType App_SetCanTpLoopbackMode(uint8_t Enabled);

/** Submit one Tx-board application message through NodeApp and CanTp. */
Std_ReturnType App_SendLargeMessage(const uint8_t *DataPtr,
                                    PduLengthType Length);

#endif /* APP_H_ */
