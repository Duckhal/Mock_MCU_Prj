#include "app.h"
#include "node_app.h"
#include "../bsp/LED.h"
#include "../drivers/adc/Driver_ADC.h"
#include "../drivers/can/cantp/Cantp.h"
#include "../drivers/can/com/Com.h"
#include "../drivers/uart/Driver_UART.h"
#include "../middlewares/ring_buffer.h"
#include <stddef.h>
#include <string.h>

#define APP_UART_BAUD             (115200U)
#define APP_UART_RX_CAPACITY      (17U * 1024U)
#define APP_UART_TX_CAPACITY      (256U)
#define APP_NETWORK_TASK_TICKS    (10U)

volatile App_InitErrorType g_AppInitError;
volatile App_RuntimeStatusType g_AppRuntimeStatus;
volatile uint8_t g_AppRole;
volatile uint32_t g_AppMainFunctionCount;
volatile uint32_t g_AppAdcConversions;
volatile uint16_t g_AppAdcValue;
volatile uint8_t g_AppAliveCounter;
volatile uint8_t g_AppKeepAliveRateLevel;
volatile uint32_t g_AppKeepAliveEvents;
volatile uint8_t g_AppLastAliveCounter;
volatile uint32_t g_AppLastAliveTick;
volatile uint8_t g_AppSlaveStatus;
volatile uint8_t g_AppSlaveOnline[2];
volatile uint8_t g_AppSlaveReportedStatus[2];
volatile uint8_t g_AppOnlineSlaveCount;
volatile uint32_t g_AppUartErrors;
volatile uint32_t g_AppUartRxBytes;
volatile uint32_t g_AppUartRxOverflows;
volatile uint32_t g_AppUartTxOverflows;
volatile uint32_t g_AppCanTpTxRequests;
volatile uint32_t g_AppCanTpTxRejects;
volatile uint32_t g_AppCanTpTxCompleted;
volatile uint32_t g_AppCanTpTxFailures;
volatile uint32_t g_AppCanTpRxMessages;
volatile uint32_t g_AppCanTpRxErrors;
volatile uint32_t g_AppCanTpRxUartDeliveries;
volatile uint32_t g_AppCanTpRxIgnored;
volatile uint8_t g_AppCanTpTxPending;
volatile uint8_t g_AppCanTpInternalLoopback;
volatile uint16_t g_AppImageLength;
volatile uint16_t g_AppImageBytesQueued;
volatile uint32_t g_AppImageChunksCompleted;
volatile uint32_t g_AppImageChunksDropped;
volatile uint32_t g_AppImageRetryCount;
volatile uint32_t g_AppStateCorruptionCount;
volatile uint32_t g_AppStateErrorMask;
volatile PduLengthType g_AppLastCanTpRxLength;
uint8_t g_AppLastCanTpRxData[APP_MAX_LARGE_MESSAGE_LENGTH];

typedef char App_NodeLengthMustMatch[
    (APP_MAX_LARGE_MESSAGE_LENGTH == NODE_APP_MAX_NSDU_LENGTH) ? 1 : -1];

static const uint16_t App_KeepAlivePeriods[APP_KEEPALIVE_LEVEL_COUNT] =
{
    500U, 200U, 100U, 50U, 20U, 10U, 5U
};
static const uint16_t App_LedFullCyclePeriods[APP_KEEPALIVE_LEVEL_COUNT] =
{
    1500U, 1000U, 800U, 600U, 400U, 300U, 200U
};

static uint8_t App_HardwareReady;
static uint8_t App_Initialized;
static uint8_t App_AdcConversionPending;
static uint16_t App_KeepAliveCountdown;
static uint32_t App_LastKeepAliveRxCount;
static uint8_t App_HasAliveCounter;
static uint8_t App_BlueLedOn;
static uint32_t App_LastLedToggleTick;
static uint32_t App_LastNetworkMonitorTick;
static uint32_t App_LastStatusRxCount[2];
static uint32_t App_LastStatusTick[2];
static RingBuffer_t App_UartRxRing;
static RingBuffer_t App_UartTxRing;
static uint8_t App_UartRxStorage[APP_UART_RX_CAPACITY];
static uint8_t App_UartTxStorage[APP_UART_TX_CAPACITY];
static uint8_t App_ImageHeader[2];
static uint8_t App_ImageHeaderLength;
static uint8_t App_ImageActive;
static uint16_t App_ImageRemaining;
static uint8_t App_ImageChunk[APP_MAX_LARGE_MESSAGE_LENGTH];
static PduLengthType App_ImageChunkLength;
static uint8_t App_ImageChunkValid;
static uint8_t App_ImageChunkRetryCount;
static uint32_t App_TxConfirmationBaseline;

/* Restart the blue-LED indication when the active KeepAlive rate changes. */
static void App_StartRateLed(uint32_t Tick)
{
    App_LastLedToggleTick = Tick;
    App_BlueLedOn = 1U;
    LED_On(LED_BLUE);
}

/* Advance the blue LED at half of the selected full-cycle period. */
static void App_UpdateRateLed(uint32_t Tick)
{
    uint32_t halfPeriod;
    uint32_t transitions;
    if ((g_AppRole != APP_ROLE_MASTER) &&
        (g_AppSlaveStatus == COM_SLAVE_STATUS_MASTER_LOST))
    {
        return;
    }
    halfPeriod = App_LedFullCyclePeriods[g_AppKeepAliveRateLevel] / 2U;
    transitions = (uint32_t)(Tick - App_LastLedToggleTick) / halfPeriod;
    if (transitions > 0U)
    {
        App_LastLedToggleTick += transitions * halfPeriod;
        if ((transitions & 1U) != 0U)
        {
            App_BlueLedOn ^= 1U;
            LED_Write(LED_BLUE, (App_BlueLedOn != 0U) ?
                      LED_STATE_ON : LED_STATE_OFF);
        }
    }
}

/* Validate every index and lifecycle flag used by role-specific policies. */
static Std_ReturnType App_ValidateState(void)
{
    uint32_t errorMask = 0U;
    if ((App_HardwareReady > 1U) || (App_Initialized > 1U) ||
        (g_AppRole > APP_ROLE_SLAVE2) ||
        (g_AppRole != (uint8_t)APP_BOARD_ROLE) ||
        (g_AppCanTpTxPending > 1U) ||
        (g_AppCanTpInternalLoopback > 1U) ||
        (App_ImageActive > 1U) || (App_ImageChunkValid > 1U))
    {
        errorMask |= APP_STATE_ERROR_LIFECYCLE;
    }
    if (g_AppKeepAliveRateLevel >= APP_KEEPALIVE_LEVEL_COUNT)
    {
        errorMask |= APP_STATE_ERROR_KEEPALIVE_LEVEL;
    }
    if (g_AppSlaveStatus > COM_SLAVE_STATUS_MASTER_LOST)
    {
        errorMask |= APP_STATE_ERROR_SLAVE_STATUS;
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

/* Push one PC byte only when this image accepts an image input stream. */
static void App_UartRxCallback(uint8_t Data)
{
    if ((g_AppRole != APP_ROLE_MASTER) &&
        (g_AppCanTpInternalLoopback == 0U))
    {
        return;
    }
    if (RingBuffer_Push(&App_UartRxRing, Data) == RING_BUFFER_OK)
    {
        g_AppUartRxBytes++;
    }
    else
    {
        g_AppUartRxOverflows++;
    }
}

/* Supply one queued output byte to the UART Tx interrupt. */
static bool App_UartTxCallback(uint8_t *DataPtr)
{
    return (bool)((DataPtr != NULL) &&
                  (RingBuffer_Pop(&App_UartTxRing, DataPtr) ==
                   RING_BUFFER_OK));
}

/* Queue an atomic UART block without overwriting bytes not yet transmitted. */
static Std_ReturnType App_QueueUartBytes(const uint8_t *DataPtr,
                                        PduLengthType Length)
{
    PduLengthType index;
    if ((DataPtr == NULL) || (Length == 0U) ||
        (RingBuffer_GetFree(&App_UartTxRing) < Length))
    {
        g_AppUartTxOverflows++;
        return E_NOT_OK;
    }
    for (index = 0U; index < Length; index++)
    {
        if (RingBuffer_Push(&App_UartTxRing, DataPtr[index]) != RING_BUFFER_OK)
        {
            g_AppUartErrors++;
            return E_NOT_OK;
        }
    }
    LPUART1_EnableTxInterrupt();
    return E_OK;
}

/* Queue one fixed diagnostic line without using formatted-output libraries. */
static void App_QueueUartString(const char *Text)
{
    size_t length;
    if (Text == NULL)
    {
        return;
    }
    length = strlen(Text);
    if ((length > 0U) && (length <= UINT16_MAX))
    {
        (void)App_QueueUartBytes((const uint8_t *)Text,
                                 (PduLengthType)length);
    }
}

/* Turn every user LED off before applying a role-specific indication. */
static void App_ClearLeds(void)
{
    LED_Off(LED_BLUE);
    LED_Off(LED_RED);
    LED_Off(LED_GREEN);
}

/* Map the complete 12-bit ADC range evenly onto levels zero through six. */
static uint8_t App_MapAdcToRateLevel(uint16_t AdcValue)
{
    uint32_t level = ((uint32_t)AdcValue * APP_KEEPALIVE_LEVEL_COUNT) /
                     ((uint32_t)ADC_MAX_VALUE + 1U);
    if (level >= APP_KEEPALIVE_LEVEL_COUNT)
    {
        level = APP_KEEPALIVE_LEVEL_COUNT - 1U;
    }
    return (uint8_t)level;
}

/* Poll the Master ADC and restart rate timing when its level changes. */
static Std_ReturnType App_ProcessAdc(uint32_t Tick)
{
    uint8_t level;
    if (g_AppRole != APP_ROLE_MASTER)
    {
        return E_OK;
    }
    if ((App_AdcConversionPending != 0U) &&
        (ADC_IsConversionComplete() != 0U))
    {
        g_AppAdcValue = ADC_GetResult();
        g_AppAdcConversions++;
        App_AdcConversionPending = 0U;
        level = App_MapAdcToRateLevel(g_AppAdcValue);
        if (level != g_AppKeepAliveRateLevel)
        {
            g_AppKeepAliveRateLevel = level;
            App_KeepAliveCountdown = App_KeepAlivePeriods[level];
            App_StartRateLed(Tick);
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

/* Update both Master KeepAlive Signals at the ADC-selected application rate. */
static Std_ReturnType App_ProcessMasterKeepAlive(void)
{
    uint32_t value;
    if (g_AppRole != APP_ROLE_MASTER)
    {
        return E_OK;
    }
    if (App_KeepAliveCountdown > 0U)
    {
        App_KeepAliveCountdown--;
    }
    if (App_KeepAliveCountdown != 0U)
    {
        return E_OK;
    }
    if (g_AppAliveCounter >= COM_ALIVE_COUNTER_MAX_VALUE)
    {
        g_AppAliveCounter = 0U;
    }
    else
    {
        g_AppAliveCounter++;
    }
    value = g_AppAliveCounter;
    if (Com_SendSignal(COM_SIGNAL_TX_ALIVE_COUNTER, &value) != E_OK)
    {
        g_AppRuntimeStatus = APP_RUNTIME_COM_SEND_ERROR;
        return E_NOT_OK;
    }
    value = g_AppKeepAliveRateLevel;
    if (Com_SendSignal(COM_SIGNAL_TX_KEEPALIVE_RATE, &value) != E_OK)
    {
        g_AppRuntimeStatus = APP_RUNTIME_COM_SEND_ERROR;
        return E_NOT_OK;
    }
    g_AppKeepAliveEvents++;
    App_KeepAliveCountdown =
        App_KeepAlivePeriods[g_AppKeepAliveRateLevel];
    return E_OK;
}

/* Write the current Slave status into the role-owned periodic COM Signal. */
static Std_ReturnType App_WriteSlaveStatus(void)
{
    uint32_t value = g_AppSlaveStatus;
    PduIdType signalId = (g_AppRole == APP_ROLE_SLAVE1) ?
        COM_SIGNAL_TX_SLAVE1_STATUS : COM_SIGNAL_TX_SLAVE2_STATUS;
    if (Com_SendSignal(signalId, &value) != E_OK)
    {
        g_AppRuntimeStatus = APP_RUNTIME_COM_SEND_ERROR;
        return E_NOT_OK;
    }
    return E_OK;
}

/* Consume KeepAlive events and derive Slave NORMAL/MASTER_LOST state. */
static Std_ReturnType App_ProcessSlaveKeepAlive(uint32_t Tick)
{
    uint32_t count;
    if (g_AppRole == APP_ROLE_MASTER)
    {
        return E_OK;
    }
    count = Com_GetRxIPduIndicationCount(COM_IPDU_RX_KEEPALIVE);
    if (count != App_LastKeepAliveRxCount)
    {
        uint32_t alive = 0U;
        uint32_t level = 0U;
        App_LastKeepAliveRxCount = count;
        if ((Com_ReceiveSignal(COM_SIGNAL_RX_ALIVE_COUNTER, &alive) != E_OK) ||
            (Com_ReceiveSignal(COM_SIGNAL_RX_KEEPALIVE_RATE, &level) != E_OK))
        {
            g_AppRuntimeStatus = APP_RUNTIME_COM_RECEIVE_ERROR;
            return E_NOT_OK;
        }
        if ((alive > COM_ALIVE_COUNTER_MAX_VALUE) ||
            (level >= APP_KEEPALIVE_LEVEL_COUNT))
        {
            g_AppRuntimeStatus = APP_RUNTIME_COM_RECEIVE_ERROR;
            return E_NOT_OK;
        }
        if ((App_HasAliveCounter == 0U) ||
            ((uint8_t)alive != g_AppLastAliveCounter))
        {
            uint8_t firstAlive = (uint8_t)(App_HasAliveCounter == 0U);
            uint8_t rateChanged =
                (uint8_t)((uint8_t)level != g_AppKeepAliveRateLevel);
            g_AppLastAliveCounter = (uint8_t)alive;
            g_AppLastAliveTick = Tick;
            g_AppKeepAliveRateLevel = (uint8_t)level;
            App_HasAliveCounter = 1U;
            if ((g_AppSlaveStatus == COM_SLAVE_STATUS_MASTER_LOST) ||
                (rateChanged != 0U) || (firstAlive != 0U))
            {
                g_AppSlaveStatus = COM_SLAVE_STATUS_NORMAL;
                if (App_WriteSlaveStatus() != E_OK)
                {
                    return E_NOT_OK;
                }
                App_StartRateLed(Tick);
            }
        }
    }
    if ((g_AppSlaveStatus == COM_SLAVE_STATUS_NORMAL) &&
        ((uint32_t)(Tick - g_AppLastAliveTick) >=
         APP_MASTER_LOST_TIMEOUT_TICKS))
    {
        g_AppSlaveStatus = COM_SLAVE_STATUS_MASTER_LOST;
        App_BlueLedOn = 0U;
        LED_Off(LED_BLUE);
        if (App_WriteSlaveStatus() != E_OK)
        {
            return E_NOT_OK;
        }
    }
    return E_OK;
}

/* Queue a transition and current online count for the Master terminal. */
static void App_ReportSlaveOnlineState(uint8_t SlaveIndex, uint8_t Online)
{
    if (SlaveIndex == 0U)
    {
        App_QueueUartString((Online != 0U) ?
            "[MASTER] Slave 1 ONLINE\r\n" :
            "[MASTER] Slave 1 OFFLINE\r\n");
    }
    else
    {
        App_QueueUartString((Online != 0U) ?
            "[MASTER] Slave 2 ONLINE\r\n" :
            "[MASTER] Slave 2 OFFLINE\r\n");
    }
}

/* Recompute and report the aggregate online count only when it changes. */
static void App_UpdateOnlineCount(void)
{
    uint8_t count = (uint8_t)(g_AppSlaveOnline[0] + g_AppSlaveOnline[1]);
    if (count != g_AppOnlineSlaveCount)
    {
        g_AppOnlineSlaveCount = count;
        if (count == 0U)
        {
            App_QueueUartString("[MASTER] Online Slaves: 0/2\r\n");
        }
        else if (count == 1U)
        {
            App_QueueUartString("[MASTER] Online Slaves: 1/2\r\n");
        }
        else
        {
            App_QueueUartString("[MASTER] Online Slaves: 2/2\r\n");
        }
    }
}

/* Observe one Slave Status route and refresh its independent online timer. */
static Std_ReturnType App_ProcessOneSlaveStatus(uint8_t SlaveIndex,
                                                uint32_t Tick)
{
    PduIdType ipduId = (SlaveIndex == 0U) ?
        COM_IPDU_RX_SLAVE1_STATUS : COM_IPDU_RX_SLAVE2_STATUS;
    PduIdType signalId = (SlaveIndex == 0U) ?
        COM_SIGNAL_RX_SLAVE1_STATUS : COM_SIGNAL_RX_SLAVE2_STATUS;
    uint32_t count = Com_GetRxIPduIndicationCount(ipduId);
    if (count != App_LastStatusRxCount[SlaveIndex])
    {
        uint32_t value = 0U;
        App_LastStatusRxCount[SlaveIndex] = count;
        if ((Com_ReceiveSignal(signalId, &value) != E_OK) ||
            (value > COM_SLAVE_STATUS_MASTER_LOST))
        {
            g_AppRuntimeStatus = APP_RUNTIME_COM_RECEIVE_ERROR;
            return E_NOT_OK;
        }
        g_AppSlaveReportedStatus[SlaveIndex] = (uint8_t)value;
        App_LastStatusTick[SlaveIndex] = Tick;
        if (g_AppSlaveOnline[SlaveIndex] == 0U)
        {
            g_AppSlaveOnline[SlaveIndex] = 1U;
            App_ReportSlaveOnlineState(SlaveIndex, 1U);
        }
    }
    return E_OK;
}

/* Run the Master's 10 ms status-reception and offline timeout task. */
static Std_ReturnType App_ProcessMasterNetworkMonitor(uint32_t Tick)
{
    uint8_t index;
    if (g_AppRole != APP_ROLE_MASTER)
    {
        return E_OK;
    }
    if ((App_ProcessOneSlaveStatus(0U, Tick) != E_OK) ||
        (App_ProcessOneSlaveStatus(1U, Tick) != E_OK))
    {
        return E_NOT_OK;
    }
    if ((uint32_t)(Tick - App_LastNetworkMonitorTick) < APP_NETWORK_TASK_TICKS)
    {
        App_UpdateOnlineCount();
        return E_OK;
    }
    App_LastNetworkMonitorTick = Tick;
    for (index = 0U; index < 2U; index++)
    {
        if ((g_AppSlaveOnline[index] != 0U) &&
            ((uint32_t)(Tick - App_LastStatusTick[index]) >=
             APP_SLAVE_OFFLINE_TIMEOUT_TICKS))
        {
            g_AppSlaveOnline[index] = 0U;
            App_ReportSlaveOnlineState(index, 0U);
        }
    }
    App_UpdateOnlineCount();
    return E_OK;
}

/* Release a successful image chunk or retain it for one bounded retry. */
static void App_ResolveImageChunk(Std_ReturnType Result)
{
    if (Result == E_OK)
    {
        g_AppCanTpTxCompleted++;
        g_AppImageChunksCompleted++;
        App_ImageChunkValid = 0U;
        App_ImageChunkLength = 0U;
        App_ImageChunkRetryCount = 0U;
    }
    else
    {
        g_AppCanTpTxFailures++;
        if (App_ImageChunkRetryCount < APP_IMAGE_MAX_RETRIES)
        {
            App_ImageChunkRetryCount++;
            g_AppImageRetryCount++;
        }
        else
        {
            g_AppImageChunksDropped++;
            App_ImageChunkValid = 0U;
            App_ImageChunkLength = 0U;
            App_ImageChunkRetryCount = 0U;
        }
    }
    if ((App_ImageRemaining == 0U) && (App_ImageChunkValid == 0U))
    {
        App_ImageActive = 0U;
    }
}

/* Parse the uint16-LE image length and pop one complete <=62-byte chunk. */
static void App_PrepareImageChunk(void)
{
    uint8_t byte;
    PduLengthType target;
    PduLengthType index;
    if ((App_ImageChunkValid != 0U) || (g_AppCanTpTxPending != 0U))
    {
        return;
    }
    while ((App_ImageActive == 0U) && (App_ImageHeaderLength < 2U) &&
           (RingBuffer_Pop(&App_UartRxRing, &byte) == RING_BUFFER_OK))
    {
        App_ImageHeader[App_ImageHeaderLength++] = byte;
    }
    if ((App_ImageActive == 0U) && (App_ImageHeaderLength == 2U))
    {
        g_AppImageLength = (uint16_t)((uint16_t)App_ImageHeader[0] |
                           ((uint16_t)App_ImageHeader[1] << 8U));
        App_ImageHeaderLength = 0U;
        g_AppImageBytesQueued = 0U;
        if (g_AppImageLength == 0U)
        {
            g_AppRuntimeStatus = APP_RUNTIME_IMAGE_FORMAT_ERROR;
            return;
        }
        App_ImageRemaining = g_AppImageLength;
        App_ImageActive = 1U;
        g_AppRuntimeStatus = APP_RUNTIME_OK;
    }
    if (App_ImageActive == 0U)
    {
        return;
    }
    target = (App_ImageRemaining < APP_MAX_LARGE_MESSAGE_LENGTH) ?
        (PduLengthType)App_ImageRemaining : APP_MAX_LARGE_MESSAGE_LENGTH;
    if (RingBuffer_GetCount(&App_UartRxRing) < target)
    {
        return;
    }
    for (index = 0U; index < target; index++)
    {
        if (RingBuffer_Pop(&App_UartRxRing, &App_ImageChunk[index]) !=
            RING_BUFFER_OK)
        {
            g_AppRuntimeStatus = APP_RUNTIME_IMAGE_FORMAT_ERROR;
            return;
        }
    }
    App_ImageChunkLength = target;
    App_ImageChunkValid = 1U;
    App_ImageChunkRetryCount = 0U;
    App_ImageRemaining = (uint16_t)(App_ImageRemaining - target);
    g_AppImageBytesQueued = (uint16_t)(g_AppImageBytesQueued + target);
}

/* Process one image Tx completion and at most one application attempt per tick. */
static void App_ProcessMasterImageTx(void)
{
    if ((g_AppRole != APP_ROLE_MASTER) &&
        (g_AppCanTpInternalLoopback == 0U))
    {
        RingBuffer_Clear(&App_UartRxRing);
        return;
    }
    if (g_AppCanTpTxPending != 0U)
    {
        if (NodeApp_TxConfirmationCount == App_TxConfirmationBaseline)
        {
            return;
        }
        g_AppCanTpTxPending = 0U;
        App_ResolveImageChunk(NodeApp_LastTxResult);
    }
    App_PrepareImageChunk();
    if (App_ImageChunkValid == 0U)
    {
        return;
    }
    App_TxConfirmationBaseline = NodeApp_TxConfirmationCount;
    if (App_SendLargeMessage(App_ImageChunk, App_ImageChunkLength) == E_OK)
    {
        g_AppCanTpTxPending = 1U;
    }
    else
    {
        App_ResolveImageChunk(E_NOT_OK);
    }
}

/* Deliver complete image chunks only from Slave 1 to its PC terminal. */
static Std_ReturnType App_ProcessCanTpRx(void)
{
    while (NodeApp_GetReadyCount() > 0U)
    {
        PduLengthType length = 0U;
        if (((g_AppRole == APP_ROLE_SLAVE1) ||
             (g_AppCanTpInternalLoopback != 0U)) &&
            (RingBuffer_GetFree(&App_UartTxRing) <
             APP_MAX_LARGE_MESSAGE_LENGTH))
        {
            return E_OK;
        }
        if (NodeApp_Receive(g_AppLastCanTpRxData,
                            sizeof(g_AppLastCanTpRxData), &length) != E_OK)
        {
            g_AppCanTpRxErrors++;
            g_AppRuntimeStatus = APP_RUNTIME_CANTP_RECEIVE_ERROR;
            return E_NOT_OK;
        }
        g_AppLastCanTpRxLength = length;
        g_AppCanTpRxMessages++;
        if ((g_AppRole == APP_ROLE_SLAVE1) ||
            (g_AppCanTpInternalLoopback != 0U))
        {
            if (App_QueueUartBytes(g_AppLastCanTpRxData, length) != E_OK)
            {
                g_AppRuntimeStatus = APP_RUNTIME_UART_TX_ERROR;
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

/* Enable exactly one periodic COM Tx I-PDU for the compiled ECU role. */
static Std_ReturnType App_ConfigureCommunicationProfile(void)
{
    PduIdType enabledIpdu;
    if ((Com_SetTxIPduEnabled(COM_IPDU_TX_KEEPALIVE, 0U) != E_OK) ||
        (Com_SetTxIPduEnabled(COM_IPDU_TX_SLAVE1_STATUS, 0U) != E_OK) ||
        (Com_SetTxIPduEnabled(COM_IPDU_TX_SLAVE2_STATUS, 0U) != E_OK))
    {
        return E_NOT_OK;
    }
    if (g_AppRole == APP_ROLE_MASTER)
    {
        enabledIpdu = COM_IPDU_TX_KEEPALIVE;
    }
    else if (g_AppRole == APP_ROLE_SLAVE1)
    {
        enabledIpdu = COM_IPDU_TX_SLAVE1_STATUS;
    }
    else
    {
        enabledIpdu = COM_IPDU_TX_SLAVE2_STATUS;
    }
    if (Com_SetTxIPduEnabled(enabledIpdu, 1U) != E_OK)
    {
        return E_NOT_OK;
    }
    return CanTp_SetDataRxEnabled(
        (uint8_t)(g_AppRole != APP_ROLE_SLAVE2));
}

/* Initialize LEDs, role-owned ADC hardware and interrupt-driven UART. */
Std_ReturnType App_HardwareInit(void)
{
    g_AppInitError = APP_INIT_ERROR_NONE;
    App_HardwareReady = 0U;
    App_Initialized = 0U;
    LED_Init(LED_BLUE);
    LED_Init(LED_RED);
    LED_Init(LED_GREEN);
    App_ClearLeds();
#if APP_BOARD_ROLE == APP_ROLE_MASTER
    if ((ADC_Init(ADC_MODE_SW_TRIGGER) != ADC_STATUS_OK) ||
        (ADC_SetChannel(ADC_CHANNEL_12) != ADC_STATUS_OK))
    {
        g_AppInitError = APP_INIT_ERROR_ADC;
        return E_NOT_OK;
    }
#endif
    if (LPUART1_Init(APP_UART_BAUD) != UART_STATUS_OK)
    {
        g_AppInitError = APP_INIT_ERROR_UART;
        return E_NOT_OK;
    }
    LPUART1_RegisterCallbacks(NULL, NULL);
    App_HardwareReady = 1U;
    return E_OK;
}

/* Initialize role state, bounded queues and communication-stack profile. */
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
    g_AppRole = (uint8_t)APP_BOARD_ROLE;
    g_AppMainFunctionCount = 0U;
    g_AppAdcConversions = 0U;
    g_AppAdcValue = 0U;
    g_AppAliveCounter = 0U;
    g_AppKeepAliveRateLevel = 0U;
    g_AppKeepAliveEvents = 0U;
    g_AppLastAliveCounter = 0U;
    g_AppLastAliveTick = 0U;
    g_AppSlaveStatus = COM_SLAVE_STATUS_NORMAL;
    memset((void *)g_AppSlaveOnline, 0, sizeof(g_AppSlaveOnline));
    memset((void *)g_AppSlaveReportedStatus, 0,
           sizeof(g_AppSlaveReportedStatus));
    g_AppOnlineSlaveCount = 0U;
    g_AppUartErrors = 0U;
    g_AppUartRxBytes = 0U;
    g_AppUartRxOverflows = 0U;
    g_AppUartTxOverflows = 0U;
    g_AppCanTpTxRequests = 0U;
    g_AppCanTpTxRejects = 0U;
    g_AppCanTpTxCompleted = 0U;
    g_AppCanTpTxFailures = 0U;
    g_AppCanTpRxMessages = 0U;
    g_AppCanTpRxErrors = 0U;
    g_AppCanTpRxUartDeliveries = 0U;
    g_AppCanTpRxIgnored = 0U;
    g_AppCanTpTxPending = 0U;
    g_AppCanTpInternalLoopback = 0U;
    g_AppImageLength = 0U;
    g_AppImageBytesQueued = 0U;
    g_AppImageChunksCompleted = 0U;
    g_AppImageChunksDropped = 0U;
    g_AppImageRetryCount = 0U;
    g_AppStateCorruptionCount = 0U;
    g_AppStateErrorMask = 0U;
    g_AppLastCanTpRxLength = 0U;
    memset(g_AppLastCanTpRxData, 0, sizeof(g_AppLastCanTpRxData));
    memset(App_ImageHeader, 0, sizeof(App_ImageHeader));
    memset(App_ImageChunk, 0, sizeof(App_ImageChunk));
    App_AdcConversionPending = 0U;
    App_KeepAliveCountdown = App_KeepAlivePeriods[0];
    App_LastKeepAliveRxCount = 0U;
    App_HasAliveCounter = 0U;
    App_BlueLedOn = 0U;
    App_LastLedToggleTick = 0U;
    App_LastNetworkMonitorTick = 0U;
    memset(App_LastStatusRxCount, 0, sizeof(App_LastStatusRxCount));
    memset(App_LastStatusTick, 0, sizeof(App_LastStatusTick));
    App_ImageHeaderLength = 0U;
    App_ImageActive = 0U;
    App_ImageRemaining = 0U;
    App_ImageChunkLength = 0U;
    App_ImageChunkValid = 0U;
    App_ImageChunkRetryCount = 0U;
    App_TxConfirmationBaseline = 0U;

    if (RingBuffer_Init(&App_UartRxRing, App_UartRxStorage,
                        APP_UART_RX_CAPACITY) != RING_BUFFER_OK)
    {
        g_AppInitError = APP_INIT_ERROR_UART_RX_BUFFER;
        return E_NOT_OK;
    }
    if (RingBuffer_Init(&App_UartTxRing, App_UartTxStorage,
                        APP_UART_TX_CAPACITY) != RING_BUFFER_OK)
    {
        g_AppInitError = APP_INIT_ERROR_UART_TX_BUFFER;
        return E_NOT_OK;
    }
    LPUART1_RegisterCallbacks(App_UartRxCallback, App_UartTxCallback);
    if (NodeApp_Init() != E_OK)
    {
        g_AppInitError = APP_INIT_ERROR_NODE;
        return E_NOT_OK;
    }
    if (App_ConfigureCommunicationProfile() != E_OK)
    {
        g_AppInitError = APP_INIT_ERROR_COM_PROFILE;
        return E_NOT_OK;
    }
    if (g_AppRole == APP_ROLE_MASTER)
    {
        App_StartRateLed(0U);
        if (ADC_StartConversion() != ADC_STATUS_OK)
        {
            g_AppInitError = APP_INIT_ERROR_ADC;
            return E_NOT_OK;
        }
        App_AdcConversionPending = 1U;
        App_LastStatusRxCount[0] =
            Com_GetRxIPduIndicationCount(COM_IPDU_RX_SLAVE1_STATUS);
        App_LastStatusRxCount[1] =
            Com_GetRxIPduIndicationCount(COM_IPDU_RX_SLAVE2_STATUS);
        App_QueueUartString("[MASTER] Online Slaves: 0/2\r\n");
    }
    else
    {
        App_LastKeepAliveRxCount =
            Com_GetRxIPduIndicationCount(COM_IPDU_RX_KEEPALIVE);
        if (App_WriteSlaveStatus() != E_OK)
        {
            return E_NOT_OK;
        }
    }
    App_Initialized = 1U;
    return E_OK;
}

/* Execute all role-specific application functions once per scheduler tick. */
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
    if ((App_ProcessAdc(Tick) != E_OK) ||
        (App_ProcessMasterKeepAlive() != E_OK) ||
        (App_ProcessSlaveKeepAlive(Tick) != E_OK) ||
        (App_ProcessMasterNetworkMonitor(Tick) != E_OK) ||
        (App_ProcessCanTpRx() != E_OK))
    {
        return E_NOT_OK;
    }
    App_UpdateRateLed(Tick);
    App_ProcessMasterImageTx();
    return E_OK;
}

/* Every fixed ECU role owns one periodic COM transmit I-PDU. */
uint8_t App_IsComTxEnabled(void)
{
    return App_Initialized;
}

/* Enable Master self-echo while preserving compile-time role selection. */
Std_ReturnType App_SetCanTpLoopbackMode(uint8_t Enabled)
{
    if ((App_Initialized == 0U) || (g_AppRole != APP_ROLE_MASTER) ||
        (Enabled > 1U) || (g_AppCanTpTxPending != 0U))
    {
        return E_NOT_OK;
    }
    if (CanTp_SetDataRxEnabled(1U) != E_OK)
    {
        return E_NOT_OK;
    }
    RingBuffer_Clear(&App_UartRxRing);
    g_AppCanTpInternalLoopback = Enabled;
    return E_OK;
}

/* Forward one stable Master image chunk into NodeApp/CanTp ownership. */
Std_ReturnType App_SendLargeMessage(const uint8_t *DataPtr,
                                    PduLengthType Length)
{
    g_AppCanTpTxRequests++;
    if ((App_Initialized == 0U) ||
        ((g_AppRole != APP_ROLE_MASTER) &&
         (g_AppCanTpInternalLoopback == 0U)) ||
        (DataPtr == NULL) || (Length == 0U) ||
        (Length > APP_MAX_LARGE_MESSAGE_LENGTH) ||
        (NodeApp_Transmit(DataPtr, Length) != E_OK))
    {
        g_AppCanTpTxRejects++;
        return E_NOT_OK;
    }
    return E_OK;
}