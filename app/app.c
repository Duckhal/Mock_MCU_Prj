#include "app.h"
#include "node_app.h"
#include "../bsp/LED.h"
#include "../drivers/can/com/Com.h"
#include "../drivers/gpio/Driver_GPIO.h"
#include "../drivers/uart/Driver_UART.h"
#include <stddef.h>
#include <string.h>

#define APP_UART_BAUD       (115200U)
#define APP_DEBOUNCE_TICKS  (20U)
#define APP_LED_COMMAND_MAX (3U)

volatile App_InitErrorType g_AppInitError;
volatile App_RuntimeStatusType g_AppRuntimeStatus;
volatile uint8_t g_AppModeTx;
volatile uint32_t g_AppMainFunctionCount;
volatile uint32_t g_AppTxSignalUpdates;
volatile uint32_t g_AppRxCommands;
volatile uint32_t g_AppInvalidRxCommands;
volatile uint32_t g_AppUartErrors;
volatile uint32_t g_AppCanTpTxRequests;
volatile uint32_t g_AppCanTpTxRejects;
volatile uint32_t g_AppCanTpRxMessages;
volatile uint32_t g_AppCanTpRxErrors;
volatile uint32_t g_AppStateCorruptionCount;
volatile uint32_t g_AppStateErrorMask;
volatile uint32_t g_AppLastInvalidTxCommand;
volatile PduLengthType g_AppLastCanTpRxLength;
uint8_t g_AppLastCanTpRxData[APP_MAX_LARGE_MESSAGE_LENGTH];

typedef char App_NodeLengthMustMatch[
    (APP_MAX_LARGE_MESSAGE_LENGTH == NODE_APP_MAX_NSDU_LENGTH) ? 1 : -1];

static uint32_t App_LastRxCount;
static uint32_t App_Sw2ChangedAt;
static uint32_t App_Sw3ChangedAt;
static uint32_t App_NextCommand;
static uint8_t App_Sw2Raw;
static uint8_t App_Sw2Stable;
static uint8_t App_Sw3Raw;
static uint8_t App_Sw3Stable;
static uint8_t App_HardwareReady;
static uint8_t App_Initialized;

static const char *const App_TxMessages[4] =
{
    "TX LED OFF\r\n",
    "TX LED GREEN\r\n",
    "TX LED BLUE\r\n",
    "TX LED GREEN+BLUE\r\n"
};

static const char *const App_RxMessages[4] =
{
    "RX LED OFF\r\n",
    "RX LED GREEN\r\n",
    "RX LED BLUE\r\n",
    "RX LED GREEN+BLUE\r\n"
};

/** Validate state before it can select a UART message or alter application mode. */
static Std_ReturnType App_ValidateState(void)
{
    uint32_t errorMask = 0U;

    if (App_NextCommand > APP_LED_COMMAND_MAX)
    {
        errorMask |= APP_STATE_ERROR_TX_COMMAND;
        g_AppLastInvalidTxCommand = App_NextCommand;
    }
    if ((App_Sw2Raw > 1U) || (App_Sw2Stable > 1U) ||
        (App_Sw3Raw > 1U) || (App_Sw3Stable > 1U))
    {
        errorMask |= APP_STATE_ERROR_SWITCH;
    }
    if ((App_HardwareReady > 1U) || (App_Initialized > 1U) ||
        (g_AppModeTx > 1U))
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

/** Send one application message and retain an observable error counter. */
static void App_ReportUart(const char *MessagePtr)
{
    if (LPUART1_SendString_Blocking(MessagePtr) != UART_STATUS_OK)
    {
        g_AppUartErrors++;
    }
}

/** Turn off every user LED before applying a new application indication. */
static void App_ClearLeds(void)
{
    LED_Off(LED_BLUE);
    LED_Off(LED_RED);
    LED_Off(LED_GREEN);
}

/** Apply the selected Tx/Rx mode to LEDs, UART, and COM receive tracking. */
static void App_ApplyMode(void)
{
    App_ClearLeds();
    if (g_AppModeTx != 0U)
    {
        LED_On(LED_RED);
        App_ReportUart("MODE TX SW3=UPDATE\r\n");
    }
    else
    {
        App_LastRxCount = Com_GetRxIndicationCount();
        App_ReportUart("MODE RX WAIT CAN\r\n");
    }
}

/** Debounce SW2 and update the application Tx/Rx mode on a press. */
static void App_ProcessModeSwitch(uint32_t Tick, uint8_t SwitchLevel)
{
    if (SwitchLevel != App_Sw2Raw)
    {
        App_Sw2Raw = SwitchLevel;
        App_Sw2ChangedAt = Tick;
    }
    if ((SwitchLevel != App_Sw2Stable) &&
        ((uint32_t)(Tick - App_Sw2ChangedAt) >= APP_DEBOUNCE_TICKS))
    {
        App_Sw2Stable = SwitchLevel;
        if (SwitchLevel == 0U)
        {
            g_AppModeTx ^= 1U;
            App_ApplyMode();
        }
    }
}

/** Debounce SW3 and update the next COM LED command while in Tx mode. */
static Std_ReturnType App_ProcessCommandSwitch(uint32_t Tick,
                                                uint8_t SwitchLevel,
                                                uint8_t WasTx)
{
    if (SwitchLevel != App_Sw3Raw)
    {
        App_Sw3Raw = SwitchLevel;
        App_Sw3ChangedAt = Tick;
    }
    if ((SwitchLevel != App_Sw3Stable) &&
        ((uint32_t)(Tick - App_Sw3ChangedAt) >= APP_DEBOUNCE_TICKS))
    {
        App_Sw3Stable = SwitchLevel;
        if ((SwitchLevel == 0U) && (WasTx != 0U) &&
            (g_AppModeTx != 0U))
        {
            if (App_ValidateState() != E_OK)
            {
                return E_NOT_OK;
            }
            if (Com_SendSignal(COM_SIGNAL_LED_COMMAND, &App_NextCommand) !=
                E_OK)
            {
                g_AppRuntimeStatus = APP_RUNTIME_COM_SEND_ERROR;
                return E_NOT_OK;
            }
            g_AppTxSignalUpdates++;
            App_ReportUart(App_TxMessages[App_NextCommand]);
            App_NextCommand =
                (App_NextCommand + 1U) & APP_LED_COMMAND_MAX;
        }
    }
    return E_OK;
}

/** Consume a new COM LED command and apply it only while in Rx mode. */
static Std_ReturnType App_ProcessComRx(void)
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
        App_LastRxCount = indicationCount;
        if (Com_ReceiveSignal(COM_SIGNAL_RX_LED_COMMAND, &command) != E_OK)
        {
            g_AppRuntimeStatus = APP_RUNTIME_COM_RECEIVE_ERROR;
            return E_NOT_OK;
        }
        if (command <= APP_LED_COMMAND_MAX)
        {
            App_ClearLeds();
            if ((command & 1U) != 0U)
            {
                LED_On(LED_GREEN);
            }
            if ((command & 2U) != 0U)
            {
                LED_On(LED_BLUE);
            }
            g_AppRxCommands++;
            App_ReportUart(App_RxMessages[command]);
        }
        else
        {
            g_AppInvalidRxCommands++;
        }
    }
    return E_OK;
}

/** Drain complete CanTp N-SDUs from NodeApp into application-owned storage. */
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
    }
    return E_OK;
}

/** Initialize GPIO, LEDs, and UART without exposing them to system main. */
Std_ReturnType App_HardwareInit(void)
{
    g_AppInitError = APP_INIT_ERROR_NONE;
    App_HardwareReady = 0U;
    App_Initialized = 0U;

    LED_Init(LED_BLUE);
    LED_Init(LED_RED);
    LED_Init(LED_GREEN);
    App_ClearLeds();
    if ((Driver_GPIO0.Setup(GPIO_C12, NULL) != ARM_DRIVER_OK) ||
        (Driver_GPIO0.Setup(GPIO_C13, NULL) != ARM_DRIVER_OK) ||
        (Driver_GPIO0.SetDirection(GPIO_C12, ARM_GPIO_INPUT) != ARM_DRIVER_OK) ||
        (Driver_GPIO0.SetDirection(GPIO_C13, ARM_GPIO_INPUT) != ARM_DRIVER_OK) ||
        (Driver_GPIO0.SetPullResistor(GPIO_C12, ARM_GPIO_PULL_UP) != ARM_DRIVER_OK) ||
        (Driver_GPIO0.SetPullResistor(GPIO_C13, ARM_GPIO_PULL_UP) != ARM_DRIVER_OK))
    {
        g_AppInitError = APP_INIT_ERROR_GPIO;
        return E_NOT_OK;
    }
    if (LPUART1_Init(APP_UART_BAUD) != UART_STATUS_OK)
    {
        g_AppInitError = APP_INIT_ERROR_UART;
        return E_NOT_OK;
    }
    App_HardwareReady = 1U;
    return E_OK;
}

/** Initialize all application runtime state and transport-owned buffers. */
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
    g_AppModeTx = 0U;
    g_AppMainFunctionCount = 0U;
    g_AppTxSignalUpdates = 0U;
    g_AppRxCommands = 0U;
    g_AppInvalidRxCommands = 0U;
    g_AppUartErrors = 0U;
    g_AppCanTpTxRequests = 0U;
    g_AppCanTpTxRejects = 0U;
    g_AppCanTpRxMessages = 0U;
    g_AppCanTpRxErrors = 0U;
    g_AppStateCorruptionCount = 0U;
    g_AppStateErrorMask = 0U;
    g_AppLastInvalidTxCommand = 0U;
    g_AppLastCanTpRxLength = 0U;
    memset(g_AppLastCanTpRxData, 0, sizeof(g_AppLastCanTpRxData));

    App_LastRxCount = 0U;
    App_Sw2ChangedAt = 0U;
    App_Sw3ChangedAt = 0U;
    App_NextCommand = 0U;
    App_Sw2Raw = 1U;
    App_Sw2Stable = 1U;
    App_Sw3Raw = 1U;
    App_Sw3Stable = 1U;

    if (NodeApp_Init() != E_OK)
    {
        g_AppInitError = APP_INIT_ERROR_NODE;
        return E_NOT_OK;
    }

    App_LastRxCount = Com_GetRxIndicationCount();
    App_ApplyMode();
    App_Initialized = 1U;
    return E_OK;
}

/** Run all application use cases once for the supplied scheduler tick. */
Std_ReturnType App_MainFunction(uint32_t Tick)
{
    uint8_t wasTx;
    uint8_t sw2;
    uint8_t sw3;

    if (App_Initialized == 0U)
    {
        g_AppRuntimeStatus = APP_RUNTIME_NOT_INITIALIZED;
        return E_NOT_OK;
    }
    if (App_ValidateState() != E_OK)
    {
        return E_NOT_OK;
    }
    wasTx = g_AppModeTx;
    sw2 = (uint8_t)Driver_GPIO0.GetInput(GPIO_C12);
    sw3 = (uint8_t)Driver_GPIO0.GetInput(GPIO_C13);

    g_AppMainFunctionCount++;
    App_ProcessModeSwitch(Tick, sw2);
    if (App_ProcessCommandSwitch(Tick, sw3, wasTx) != E_OK)
    {
        return E_NOT_OK;
    }
    if (App_ProcessComRx() != E_OK)
    {
        return E_NOT_OK;
    }
    if (App_ProcessCanTpRx() != E_OK)
    {
        return E_NOT_OK;
    }
    return E_OK;
}

/** Expose the application transmission policy to the system scheduler. */
uint8_t App_IsComTxEnabled(void)
{
    return (App_Initialized != 0U) ? g_AppModeTx : 0U;
}

/** Forward a validated application N-SDU to the NodeApp ownership layer. */
Std_ReturnType App_SendLargeMessage(const uint8_t *DataPtr,
                                    PduLengthType Length)
{
    g_AppCanTpTxRequests++;
    if ((App_Initialized == 0U) || (DataPtr == NULL) || (Length == 0U) ||
        (Length > APP_MAX_LARGE_MESSAGE_LENGTH) ||
        (NodeApp_Transmit(DataPtr, Length) != E_OK))
    {
        g_AppCanTpTxRejects++;
        return E_NOT_OK;
    }
    return E_OK;
}
