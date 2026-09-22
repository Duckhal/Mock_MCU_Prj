#include "app.h"
#include "node_app.h"
#include "../bsp/LED.h"
#include "../drivers/adc/Driver_ADC.h"
#include "../drivers/can/com/Com.h"
#include "../drivers/uart/Driver_UART.h"
#include "../middlewares/ring_buffer.h"
#include <stddef.h>
#include <string.h>

#define APP_UART_BAUD                (115200U)
#define APP_UART_RX_RING_CAPACITY    (128U)
#define APP_UART_MESSAGE_GAP_TICKS   (20U)
#define APP_CANTP_TX_TIMEOUT_TICKS   (500U)
#define APP_COM_UPDATE_TICKS         (10U)
#define APP_ADC_OFF_MAX              (999U)
#define APP_ADC_BLINK_500_MAX        (1999U)
#define APP_ADC_BLINK_1000_MAX       (2999U)
#define APP_ADC_BLINK_2000_MAX       (3999U)

volatile App_InitErrorType g_AppInitError;
volatile App_RuntimeStatusType g_AppRuntimeStatus;
volatile uint8_t g_AppModeTx;
volatile uint32_t g_AppMainFunctionCount;
volatile uint32_t g_AppTxSignalUpdates;
volatile uint32_t g_AppRxCommands;
volatile uint32_t g_AppInvalidRxCommands;
volatile uint32_t g_AppAdcConversions;
volatile uint16_t g_AppAdcValue;
volatile uint8_t g_AppLedMode;
volatile uint8_t g_AppLedState;
volatile uint32_t g_AppUartErrors;
volatile uint32_t g_AppCanTpTxRequests;
volatile uint32_t g_AppCanTpTxRejects;
volatile uint32_t g_AppCanTpRxMessages;
volatile uint32_t g_AppCanTpRxErrors;
volatile uint32_t g_AppUartRxBytes;
volatile uint32_t g_AppUartRxOverflows;
volatile uint32_t g_AppCanTpTxCompleted;
volatile uint32_t g_AppCanTpTxFailures;
volatile uint32_t g_AppCanTpRxUartDeliveries;
volatile uint32_t g_AppCanTpRxIgnored;
volatile uint8_t g_AppCanTpTxPending;
volatile uint8_t g_AppCanTpInternalLoopback;
volatile uint32_t g_AppStateCorruptionCount;
volatile uint32_t g_AppStateErrorMask;
volatile PduLengthType g_AppLastCanTpRxLength;
uint8_t g_AppLastCanTpRxData[APP_MAX_LARGE_MESSAGE_LENGTH];

typedef char App_NodeLengthMustMatch[
    (APP_MAX_LARGE_MESSAGE_LENGTH == NODE_APP_MAX_NSDU_LENGTH) ? 1 : -1];

static uint32_t App_LastRxCount;
static uint8_t App_HardwareReady;
static uint8_t App_Initialized;
static uint8_t App_AdcConversionPending;
static uint8_t App_ComUpdateArmed;
static uint8_t App_BlueLedOn;
static uint32_t App_LastComUpdateTick;
static uint32_t App_LastBlinkToggleTick;
static RingBuffer_t App_UartRxRing;
static uint8_t App_UartRxRingStorage[APP_UART_RX_RING_CAPACITY];
static uint8_t App_UartMessage[APP_MAX_LARGE_MESSAGE_LENGTH];
static PduLengthType App_UartMessageLength;
static uint32_t App_UartLastByteTick;
static uint32_t App_CanTpTxStartedAtTick;
static uint32_t App_CanTpTxConfirmationBaseline;

/* Validate application state before it alters mode or indexes runtime policy. */
static Std_ReturnType App_ValidateState(void)
{
    uint32_t errorMask = 0U;

    if ((g_AppLedMode > COM_LED_MODE_MAX) ||
        (g_AppLedState > COM_LED_STATE_ON) ||
        ((g_AppLedState == COM_LED_STATE_OFF) &&
         (g_AppLedMode != COM_LED_MODE_STEADY)))
    {
        errorMask |= APP_STATE_ERROR_LED_COMMAND;
    }
    if ((App_HardwareReady > 1U) || (App_Initialized > 1U) ||
        (g_AppModeTx > 1U) || (g_AppCanTpTxPending > 1U) ||
        (g_AppCanTpInternalLoopback > 1U))
    {
        errorMask |= APP_STATE_ERROR_LIFECYCLE;
    }
    if (errorMask != 0U)
    {
        g_AppStateErrorMask = errorMask;
        g_AppStateCorruptionCount++;
        g_AppRuntimeStatus = APP_RUNTIME_STATE_CORRUPTION;
        return E_NOT_OK;
    }
    return E_OK;
}

/* Queue one received UART byte without running application logic in the ISR. */
static void App_UartRxCallback(uint8_t RxData)
{
    if (RingBuffer_Push(&App_UartRxRing, RxData) == RING_BUFFER_OK)
    {
        g_AppUartRxBytes++;
    }
    else
    {
        g_AppUartRxOverflows++;
    }
}

/* Discard queued UART input and the incomplete message during a mode change. */
static void App_DiscardUartInput(void)
{
    uint8_t byte;
    while (RingBuffer_Pop(&App_UartRxRing, &byte) == RING_BUFFER_OK)
    {
    }
    App_UartMessageLength = 0U;
    App_UartLastByteTick = 0U;
}

/* Send one complete received CanTp N-SDU to the PC without framing bytes. */
static Std_ReturnType App_SendUartBytes(const uint8_t *DataPtr,
                                       PduLengthType Length)
{
    PduLengthType index;
    for (index = 0U; index < Length; index++)
    {
        if (LPUART1_SendChar_Blocking((char)DataPtr[index]) != UART_STATUS_OK)
        {
            g_AppUartErrors++;
            g_AppRuntimeStatus = APP_RUNTIME_UART_TX_ERROR;
            return E_NOT_OK;
        }
    }
    return E_OK;
}

/* Turn off every user LED before applying a new application indication. */
static void App_ClearLeds(void)
{
    LED_Off(LED_BLUE);
    LED_Off(LED_RED);
    LED_Off(LED_GREEN);
}

/* Apply the compile-time Tx/Rx role without writing COM traffic to UART. */
static void App_ApplyConfiguredRole(void)
{
    App_DiscardUartInput();
    App_ClearLeds();
    App_ComUpdateArmed = 0U;
    App_BlueLedOn = 0U;
    g_AppLedMode = COM_LED_MODE_STEADY;
    g_AppLedState = COM_LED_STATE_OFF;
    if (g_AppModeTx != 0U)
    {
        LED_On(LED_RED);
    }
    else
    {
        App_LastRxCount = Com_GetRxIndicationCount();
    }
}

/* Map one 12-bit potentiometer sample to the LED mode/state command. */
static void App_MapAdcToLedCommand(uint16_t AdcValue)
{
    if (AdcValue <= APP_ADC_OFF_MAX)
    {
        g_AppLedMode = COM_LED_MODE_STEADY;
        g_AppLedState = COM_LED_STATE_OFF;
    }
    else if (AdcValue <= APP_ADC_BLINK_500_MAX)
    {
        g_AppLedMode = COM_LED_MODE_BLINK_500_MS;
        g_AppLedState = COM_LED_STATE_ON;
    }
    else if (AdcValue <= APP_ADC_BLINK_1000_MAX)
    {
        g_AppLedMode = COM_LED_MODE_BLINK_1000_MS;
        g_AppLedState = COM_LED_STATE_ON;
    }
    else if (AdcValue <= APP_ADC_BLINK_2000_MAX)
    {
        g_AppLedMode = COM_LED_MODE_BLINK_2000_MS;
        g_AppLedState = COM_LED_STATE_ON;
    }
    else
    {
        g_AppLedMode = COM_LED_MODE_STEADY;
        g_AppLedState = COM_LED_STATE_ON;
    }
}

/* Poll one non-blocking ADC conversion and immediately start the next sample. */
static Std_ReturnType App_ProcessAdc(void)
{
    if ((App_AdcConversionPending != 0U) &&
        (ADC_IsConversionComplete() != 0U))
    {
        g_AppAdcValue = ADC_GetResult();
        g_AppAdcConversions++;
        App_AdcConversionPending = 0U;
        if (g_AppModeTx != 0U)
        {
            App_MapAdcToLedCommand(g_AppAdcValue);
        }
    }
    if (App_AdcConversionPending == 0U)
    {
        if (ADC_StartConversion() != ADC_STATUS_OK)
        {
            g_AppRuntimeStatus = APP_RUNTIME_ADC_ERROR;
            return E_NOT_OK;
        }
        App_AdcConversionPending = 1U;
    }
    return E_OK;
}

/* Update the logical LED Signal from ADC data exactly once per 10 ms. */
static Std_ReturnType App_ProcessComTx(uint32_t Tick)
{
    uint32_t command;
    if (g_AppModeTx != 0U)
    {
        if ((App_ComUpdateArmed == 0U) ||
            ((uint32_t)(Tick - App_LastComUpdateTick) >= APP_COM_UPDATE_TICKS))
        {
            command = COM_LED_COMMAND_ENCODE(g_AppLedMode, g_AppLedState);
            if (Com_SendSignal(COM_SIGNAL_LED_COMMAND, &command) != E_OK)
            {
                g_AppRuntimeStatus = APP_RUNTIME_COM_SEND_ERROR;
                return E_NOT_OK;
            }
            App_LastComUpdateTick = Tick;
            App_ComUpdateArmed = 1U;
            g_AppTxSignalUpdates++;
        }
    }
    return E_OK;
}

/* Convert a received LED mode to its ON and OFF interval in milliseconds. */
static uint32_t App_GetBlinkInterval(uint8_t Mode)
{
    static const uint16_t intervals[4] = {0U, 500U, 1000U, 2000U};
    return intervals[Mode];
}

/* Advance the receiver's blue LED without resetting on repeated 10 ms frames. */
static void App_UpdateRxLed(uint32_t Tick)
{
    uint32_t interval;
    uint32_t elapsed;
    uint32_t transitions;

    if ((g_AppModeTx != 0U) || (g_AppLedState == COM_LED_STATE_OFF))
    {
        return;
    }
    interval = App_GetBlinkInterval(g_AppLedMode);
    if (interval == 0U)
    {
        return;
    }
    elapsed = (uint32_t)(Tick - App_LastBlinkToggleTick);
    transitions = elapsed / interval;
    if (transitions > 0U)
    {
        App_LastBlinkToggleTick += transitions * interval;
        if ((transitions & 1U) != 0U)
        {
            App_BlueLedOn ^= 1U;
            if (App_BlueLedOn != 0U)
            {
                LED_On(LED_BLUE);
            }
            else
            {
                LED_Off(LED_BLUE);
            }
        }
    }
}

/* Consume the latest raw mode/state Signal and run the receiver blink policy. */
static Std_ReturnType App_ProcessComRx(uint32_t Tick)
{
    uint32_t indicationCount = Com_GetRxIndicationCount();

    if (g_AppModeTx != 0U)
    {
        App_LastRxCount = indicationCount;
        return E_OK;
    }
    if (indicationCount != App_LastRxCount)
    {
        uint32_t command = 0U;
        uint8_t mode;
        uint8_t state;
        App_LastRxCount = indicationCount;
        if (Com_ReceiveSignal(COM_SIGNAL_RX_LED_COMMAND, &command) != E_OK)
        {
            g_AppRuntimeStatus = APP_RUNTIME_COM_RECEIVE_ERROR;
            return E_NOT_OK;
        }
        mode = COM_LED_COMMAND_GET_MODE(command);
        state = COM_LED_COMMAND_GET_STATE(command);
        if ((mode <= COM_LED_MODE_MAX) && (state <= COM_LED_STATE_ON) &&
            ((state != COM_LED_STATE_OFF) ||
             (mode == COM_LED_MODE_STEADY)))
        {
            if ((mode != g_AppLedMode) || (state != g_AppLedState))
            {
                g_AppLedMode = mode;
                g_AppLedState = state;
                App_LastBlinkToggleTick = Tick;
                App_BlueLedOn = state;
                LED_Off(LED_GREEN);
                if (state != COM_LED_STATE_OFF)
                {
                    LED_On(LED_BLUE);
                }
                else
                {
                    LED_Off(LED_BLUE);
                }
            }
            g_AppRxCommands++;
        }
        else
        {
            g_AppInvalidRxCommands++;
        }
    }
    App_UpdateRxLed(Tick);
    return E_OK;
}

/* Deliver every complete CanTp N-SDU to UART only on a receiver board. */
static Std_ReturnType App_ProcessCanTpRx(void)
{
    while (NodeApp_GetReadyCount() > 0U)
    {
        PduLengthType length = 0U;
        if (NodeApp_Receive(g_AppLastCanTpRxData,
                            sizeof(g_AppLastCanTpRxData), &length) != E_OK)
        {
            g_AppCanTpRxErrors++;
            g_AppRuntimeStatus = APP_RUNTIME_CANTP_RECEIVE_ERROR;
            return E_NOT_OK;
        }
        g_AppLastCanTpRxLength = length;
        g_AppCanTpRxMessages++;
        if ((g_AppModeTx == 0U) || (g_AppCanTpInternalLoopback != 0U))
        {
            if (App_SendUartBytes(g_AppLastCanTpRxData, length) != E_OK)
            {
                return E_NOT_OK;
            }
            g_AppCanTpRxUartDeliveries++;
        }
        else
        {
            g_AppCanTpRxIgnored++;
        }
    }
    return E_OK;
}

/* Submit Tx-board UART chunks and wait for the local CanTp final result. */
static Std_ReturnType App_ProcessUartCanTpTx(uint32_t Tick)
{
    uint8_t byte;

    if (g_AppCanTpTxPending != 0U)
    {
        if ((NodeApp_TxConfirmationCount !=
             App_CanTpTxConfirmationBaseline))
        {
            g_AppCanTpTxPending = 0U;
            if (NodeApp_LastTxResult != E_OK)
            {
                g_AppCanTpTxFailures++;
                g_AppRuntimeStatus = APP_RUNTIME_CANTP_TRANSMIT_ERROR;
                return E_OK;
            }
            g_AppCanTpTxCompleted++;
            if ((g_AppRuntimeStatus == APP_RUNTIME_CANTP_TRANSMIT_ERROR) ||
                (g_AppRuntimeStatus == APP_RUNTIME_CANTP_TX_TIMEOUT))
            {
                g_AppRuntimeStatus = APP_RUNTIME_OK;
            }
        }
        else if ((uint32_t)(Tick - App_CanTpTxStartedAtTick) >=
                 APP_CANTP_TX_TIMEOUT_TICKS)
        {
            g_AppCanTpTxPending = 0U;
            g_AppCanTpTxFailures++;
            g_AppRuntimeStatus = APP_RUNTIME_CANTP_TX_TIMEOUT;
            return E_OK;
        }
        if (g_AppCanTpTxPending != 0U)
        {
            return E_OK;
        }
    }

    if ((g_AppModeTx == 0U) && (g_AppCanTpInternalLoopback == 0U))
    {
        App_DiscardUartInput();
        return E_OK;
    }

    while ((App_UartMessageLength < APP_MAX_LARGE_MESSAGE_LENGTH) &&
           (RingBuffer_Pop(&App_UartRxRing, &byte) == RING_BUFFER_OK))
    {
        App_UartMessage[App_UartMessageLength] = byte;
        App_UartMessageLength++;
        App_UartLastByteTick = Tick;
    }

    if ((App_UartMessageLength == 0U) ||
        ((App_UartMessageLength < APP_MAX_LARGE_MESSAGE_LENGTH) &&
         ((uint32_t)(Tick - App_UartLastByteTick) <
          APP_UART_MESSAGE_GAP_TICKS)))
    {
        return E_OK;
    }

    App_CanTpTxConfirmationBaseline = NodeApp_TxConfirmationCount;
    if (App_SendLargeMessage(App_UartMessage,
                             App_UartMessageLength) != E_OK)
    {
        return E_OK;
    }
    App_UartMessageLength = 0U;
    App_UartLastByteTick = 0U;
    App_CanTpTxStartedAtTick = Tick;
    g_AppCanTpTxPending = 1U;
    return E_OK;
}

/* Initialize ADC, LEDs, and UART without exposing them to system main. */
Std_ReturnType App_HardwareInit(void)
{
    g_AppInitError = APP_INIT_ERROR_NONE;
    App_HardwareReady = 0U;
    App_Initialized = 0U;

    LED_Init(LED_BLUE);
    LED_Init(LED_RED);
    LED_Init(LED_GREEN);
    App_ClearLeds();
    if ((ADC_Init(ADC_MODE_SW_TRIGGER) != ADC_STATUS_OK) ||
        (ADC_SetChannel(ADC_CHANNEL_12) != ADC_STATUS_OK))
    {
        g_AppInitError = APP_INIT_ERROR_ADC;
        return E_NOT_OK;
    }
    if (LPUART1_Init(APP_UART_BAUD) != UART_STATUS_OK)
    {
        g_AppInitError = APP_INIT_ERROR_UART;
        return E_NOT_OK;
    }
    LPUART1_RegisterCallbacks(NULL, NULL);
    App_HardwareReady = 1U;
    return E_OK;
}

/* Initialize all application runtime state and transport-owned buffers. */
Std_ReturnType App_Init(void)
{
    if (App_HardwareReady == 0U)
    {
        g_AppInitError = APP_INIT_ERROR_HARDWARE_NOT_READY;
        return E_NOT_OK;
    }

    g_AppInitError = APP_INIT_ERROR_NONE;
    App_Initialized = 0U;
    g_AppRuntimeStatus = APP_RUNTIME_OK;
    g_AppModeTx = (uint8_t)APP_BOARD_ROLE;
    g_AppMainFunctionCount = 0U;
    g_AppTxSignalUpdates = 0U;
    g_AppRxCommands = 0U;
    g_AppInvalidRxCommands = 0U;
    g_AppAdcConversions = 0U;
    g_AppAdcValue = 0U;
    g_AppLedMode = COM_LED_MODE_STEADY;
    g_AppLedState = COM_LED_STATE_OFF;
    g_AppUartErrors = 0U;
    g_AppCanTpTxRequests = 0U;
    g_AppCanTpTxRejects = 0U;
    g_AppCanTpRxMessages = 0U;
    g_AppCanTpRxErrors = 0U;
    g_AppUartRxBytes = 0U;
    g_AppUartRxOverflows = 0U;
    g_AppCanTpTxCompleted = 0U;
    g_AppCanTpTxFailures = 0U;
    g_AppCanTpRxUartDeliveries = 0U;
    g_AppCanTpRxIgnored = 0U;
    g_AppCanTpTxPending = 0U;
    g_AppCanTpInternalLoopback = 0U;
    g_AppStateCorruptionCount = 0U;
    g_AppStateErrorMask = 0U;
    g_AppLastCanTpRxLength = 0U;
    memset(g_AppLastCanTpRxData, 0, sizeof(g_AppLastCanTpRxData));
    memset(App_UartMessage, 0, sizeof(App_UartMessage));

    App_LastRxCount = 0U;
    App_AdcConversionPending = 0U;
    App_ComUpdateArmed = 0U;
    App_BlueLedOn = 0U;
    App_LastComUpdateTick = 0U;
    App_LastBlinkToggleTick = 0U;
    App_UartMessageLength = 0U;
    App_UartLastByteTick = 0U;
    App_CanTpTxStartedAtTick = 0U;
    App_CanTpTxConfirmationBaseline = 0U;

    if (RingBuffer_Init(&App_UartRxRing, App_UartRxRingStorage,
                        APP_UART_RX_RING_CAPACITY) != RING_BUFFER_OK)
    {
        g_AppInitError = APP_INIT_ERROR_UART_BUFFER;
        return E_NOT_OK;
    }
    LPUART1_RegisterCallbacks(App_UartRxCallback, NULL);

    if (NodeApp_Init() != E_OK)
    {
        g_AppInitError = APP_INIT_ERROR_NODE;
        return E_NOT_OK;
    }

    if (ADC_StartConversion() != ADC_STATUS_OK)
    {
        g_AppInitError = APP_INIT_ERROR_ADC;
        return E_NOT_OK;
    }
    App_AdcConversionPending = 1U;
    App_LastRxCount = Com_GetRxIndicationCount();
    App_ApplyConfiguredRole();
    App_Initialized = 1U;
    return E_OK;
}

/* Run all application use cases once for the supplied scheduler tick. */
Std_ReturnType App_MainFunction(uint32_t Tick)
{
    if (App_Initialized == 0U)
    {
        g_AppRuntimeStatus = APP_RUNTIME_NOT_INITIALIZED;
        return E_NOT_OK;
    }
    if (App_ValidateState() != E_OK)
    {
        return E_NOT_OK;
    }
    g_AppMainFunctionCount++;
    if (App_ProcessAdc() != E_OK)
    {
        return E_NOT_OK;
    }
    if (App_ProcessComTx(Tick) != E_OK)
    {
        return E_NOT_OK;
    }
    if (App_ProcessComRx(Tick) != E_OK)
    {
        return E_NOT_OK;
    }
    if (App_ProcessCanTpRx() != E_OK)
    {
        return E_NOT_OK;
    }
    if (App_ProcessUartCanTpTx(Tick) != E_OK)
    {
        return E_NOT_OK;
    }
    return E_OK;
}

/* Expose the application transmission policy to the system scheduler. */
uint8_t App_IsComTxEnabled(void)
{
    return (App_Initialized != 0U) ? g_AppModeTx : 0U;
}

/* Select the UART/CanTp self-echo fixture without changing the board role. */
Std_ReturnType App_SetCanTpLoopbackMode(uint8_t Enabled)
{
    if ((App_Initialized == 0U) || (Enabled > 1U) ||
        (g_AppCanTpTxPending != 0U))
    {
        return E_NOT_OK;
    }
    App_DiscardUartInput();
    g_AppCanTpInternalLoopback = Enabled;
    return E_OK;
}

/* Forward a validated application N-SDU to the NodeApp ownership layer. */
Std_ReturnType App_SendLargeMessage(const uint8_t *DataPtr,
                                    PduLengthType Length)
{
    g_AppCanTpTxRequests++;
    if ((App_Initialized == 0U) ||
        ((g_AppModeTx == 0U) && (g_AppCanTpInternalLoopback == 0U)) ||
        (DataPtr == NULL) || (Length == 0U) ||
        (Length > APP_MAX_LARGE_MESSAGE_LENGTH) ||
        (NodeApp_Transmit(DataPtr, Length) != E_OK))
    {
        g_AppCanTpTxRejects++;
        return E_NOT_OK;
    }
    return E_OK;
}
