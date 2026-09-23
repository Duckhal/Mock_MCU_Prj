#ifndef APP_HOST_FIXTURE_H_
#define APP_HOST_FIXTURE_H_

#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#define main App_Entry
#include "../../src/main.c"
#undef main
#include "../../app/app.c"
#include "../../middlewares/ring_buffer.c"

#define TEST_NODE_TX_HISTORY_SIZE (32U)
#define TEST_UART_CAPTURE_SIZE    (8192U)
#if defined(__GNUC__)
#define TEST_UNUSED __attribute__((unused))
#else
#define TEST_UNUSED
#endif

static jmp_buf Test_StopPoint;
static uint32_t Test_Tick;
static uint32_t Test_StopAt;
static uint32_t Test_JumpAt;
static uint8_t Test_LedState[3];
static uint32_t Test_SchedulerCount;
static uint32_t Test_WriteCount;
static uint32_t Test_ReadCount;
static uint32_t Test_CanTpCount;
static uint32_t Test_CanTpLoopbackCount;
static uint32_t Test_CanTpLoopbackEnableCount;
static uint32_t Test_PduRInitCount;
static char Test_JumpEvents[16];
static uint8_t Test_JumpEventCount;
static UART_RxCallback_t Test_UartRxCallback;
static UART_TxCallback_t Test_UartTxCallback;
static uint8_t Test_UartRawTx[TEST_UART_CAPTURE_SIZE];
static uint16_t Test_UartRawTxLength;
static uint16_t Test_AdcValue;
static uint32_t Test_AdcStartCount;
static uint32_t Test_ComSignalValue[COM_NUM_SIGNALS];
static uint8_t Test_ComSignalValid[COM_NUM_SIGNALS];
static uint32_t Test_ComSendCount[COM_NUM_SIGNALS];
static uint32_t Test_ComRxCount[COM_NUM_IPDUS];
static uint8_t Test_ComTxEnabled[COM_NUM_IPDUS];
static uint8_t Test_CanTpDataRxEnabled;
static uint32_t Test_NodeReadyCount;
static PduLengthType Test_NodeRxLength;
static uint8_t Test_NodeRxData[NODE_APP_MAX_NSDU_LENGTH];
static uint32_t Test_NodeTransmitCount;
static uint8_t Test_NodeTransmitAccept;
static PduLengthType Test_NodeTxLength[TEST_NODE_TX_HISTORY_SIZE];
static uint8_t Test_NodeTxData[TEST_NODE_TX_HISTORY_SIZE]
                                  [NODE_APP_MAX_NSDU_LENGTH];

volatile uint32_t Com_RxIndicationCount;
volatile uint32_t NodeApp_TxConfirmationCount;
volatile Std_ReturnType NodeApp_LastTxResult;

uint32_t Com_GetRxIndicationCount(void)
{
    return Com_RxIndicationCount;
}

static void Test_RecordEvent(char Event)
{
    if ((Test_JumpAt != 0U) && (Test_Tick == Test_JumpAt + 2U))
    {
        assert(Test_JumpEventCount < sizeof(Test_JumpEvents));
        Test_JumpEvents[Test_JumpEventCount++] = Event;
    }
}

static unsigned Test_LedIndex(ARM_GPIO_Pin_t Pin)
{
    if (Pin == LED_BLUE) { return 0U; }
    if (Pin == LED_RED) { return 1U; }
    assert(Pin == LED_GREEN);
    return 2U;
}

void LED_Init(ARM_GPIO_Pin_t Pin)
{
    Test_LedState[Test_LedIndex(Pin)] = 0U;
}

void LED_On(ARM_GPIO_Pin_t Pin)
{
    Test_LedState[Test_LedIndex(Pin)] = 1U;
}

void LED_Off(ARM_GPIO_Pin_t Pin)
{
    Test_LedState[Test_LedIndex(Pin)] = 0U;
}

void LED_Write(ARM_GPIO_Pin_t Pin, led_state_t State)
{
    Test_LedState[Test_LedIndex(Pin)] = (uint8_t)(State == LED_STATE_ON);
}

uint32_t ADC_Init(uint8_t Mode)
{
    assert(Mode == ADC_MODE_SW_TRIGGER);
    return ADC_STATUS_OK;
}

uint32_t ADC_SetChannel(uint8_t Channel)
{
    assert(Channel == ADC_CHANNEL_12);
    return ADC_STATUS_OK;
}

uint32_t ADC_StartConversion(void)
{
    Test_AdcStartCount++;
    return ADC_STATUS_OK;
}

uint32_t ADC_IsConversionComplete(void) { return 1U; }
uint16_t ADC_GetResult(void) { return Test_AdcValue; }

void disable_WDOG(void) {}
void init_MCU(void) {}
void SystemCoreClockUpdate(void) {}
Can_ReturnType Can_Init(void) { return CAN_OK; }
Std_ReturnType CanIf_Init(void) { return E_OK; }
Std_ReturnType CanTp_Init(void) { return E_OK; }

Std_ReturnType NodeApp_Init(void)
{
    Test_NodeReadyCount = 0U;
    Test_NodeTransmitCount = 0U;
    Test_NodeTransmitAccept = 1U;
    NodeApp_TxConfirmationCount = 0U;
    NodeApp_LastTxResult = E_NOT_OK;
    return E_OK;
}

Std_ReturnType PduR_Init(void)
{
    Test_PduRInitCount++;
    return E_OK;
}

Std_ReturnType CanTpLoopbackTest_Run(void)
{
    Test_CanTpLoopbackCount++;
    return E_OK;
}

Std_ReturnType CanTpLoopbackTest_SetEnabled(uint8_t Enable)
{
    assert(Enable <= 1U);
    Test_CanTpLoopbackEnableCount++;
    return E_OK;
}

Std_ReturnType Com_Init(void)
{
    memset(Test_ComSignalValue, 0, sizeof(Test_ComSignalValue));
    memset(Test_ComSignalValid, 0, sizeof(Test_ComSignalValid));
    memset(Test_ComSendCount, 0, sizeof(Test_ComSendCount));
    memset(Test_ComRxCount, 0, sizeof(Test_ComRxCount));
    memset(Test_ComTxEnabled, 0, sizeof(Test_ComTxEnabled));
    Com_RxIndicationCount = 0U;
    return E_OK;
}

Std_ReturnType Com_SetTxIPduEnabled(PduIdType IPduId, uint8_t Enabled)
{
    if ((IPduId >= COM_NUM_IPDUS) || (Enabled > 1U))
    {
        return E_NOT_OK;
    }
    Test_ComTxEnabled[IPduId] = Enabled;
    return E_OK;
}

uint32_t Com_GetRxIPduIndicationCount(PduIdType IPduId)
{
    return (IPduId < COM_NUM_IPDUS) ? Test_ComRxCount[IPduId] : 0U;
}

Std_ReturnType Com_SendSignal(PduIdType SignalId, const void *ValuePtr)
{
    if ((SignalId >= COM_NUM_SIGNALS) || (ValuePtr == NULL))
    {
        return E_NOT_OK;
    }
    memcpy(&Test_ComSignalValue[SignalId], ValuePtr, sizeof(uint32_t));
    Test_ComSignalValid[SignalId] = 1U;
    Test_ComSendCount[SignalId]++;
    return E_OK;
}

Std_ReturnType Com_ReceiveSignal(PduIdType SignalId, void *ValuePtr)
{
    if ((SignalId >= COM_NUM_SIGNALS) || (ValuePtr == NULL) ||
        (Test_ComSignalValid[SignalId] == 0U))
    {
        return E_NOT_OK;
    }
    memcpy(ValuePtr, &Test_ComSignalValue[SignalId], sizeof(uint32_t));
    return E_OK;
}

static TEST_UNUSED void Test_InjectKeepAlive(uint8_t Alive, uint8_t Level)
{
    Test_ComSignalValue[COM_SIGNAL_RX_ALIVE_COUNTER] = Alive;
    Test_ComSignalValue[COM_SIGNAL_RX_KEEPALIVE_RATE] = Level;
    Test_ComSignalValid[COM_SIGNAL_RX_ALIVE_COUNTER] = 1U;
    Test_ComSignalValid[COM_SIGNAL_RX_KEEPALIVE_RATE] = 1U;
    Test_ComRxCount[COM_IPDU_RX_KEEPALIVE]++;
    Com_RxIndicationCount++;
}

static TEST_UNUSED void Test_InjectSlaveStatus(uint8_t SlaveIndex,
                                                uint8_t Status)
{
    PduIdType signalId = (SlaveIndex == 0U) ?
        COM_SIGNAL_RX_SLAVE1_STATUS : COM_SIGNAL_RX_SLAVE2_STATUS;
    PduIdType ipduId = (SlaveIndex == 0U) ?
        COM_IPDU_RX_SLAVE1_STATUS : COM_IPDU_RX_SLAVE2_STATUS;
    assert(SlaveIndex < 2U);
    Test_ComSignalValue[signalId] = Status;
    Test_ComSignalValid[signalId] = 1U;
    Test_ComRxCount[ipduId]++;
    Com_RxIndicationCount++;
}

Std_ReturnType CanTp_SetDataRxEnabled(uint8_t Enabled)
{
    if (Enabled > 1U)
    {
        return E_NOT_OK;
    }
    Test_CanTpDataRxEnabled = Enabled;
    return E_OK;
}

uint8_t NodeApp_GetReadyCount(void)
{
    return (uint8_t)Test_NodeReadyCount;
}

Std_ReturnType NodeApp_Receive(uint8_t *DataPtr, PduLengthType Capacity,
                               PduLengthType *LengthPtr)
{
    if ((DataPtr == NULL) || (LengthPtr == NULL) ||
        (Test_NodeReadyCount == 0U) || (Capacity < Test_NodeRxLength))
    {
        return E_NOT_OK;
    }
    memcpy(DataPtr, Test_NodeRxData, Test_NodeRxLength);
    *LengthPtr = Test_NodeRxLength;
    Test_NodeReadyCount--;
    return E_OK;
}

Std_ReturnType NodeApp_Transmit(const uint8_t *DataPtr, PduLengthType Length)
{
    uint32_t index = Test_NodeTransmitCount;
    if ((DataPtr == NULL) || (Length == 0U) ||
        (Length > NODE_APP_MAX_NSDU_LENGTH) ||
        (index >= TEST_NODE_TX_HISTORY_SIZE) ||
        (Test_NodeTransmitAccept == 0U))
    {
        return E_NOT_OK;
    }
    memcpy(Test_NodeTxData[index], DataPtr, Length);
    Test_NodeTxLength[index] = Length;
    Test_NodeTransmitCount++;
    return E_OK;
}

static TEST_UNUSED void Test_ConfirmNodeTx(Std_ReturnType Result)
{
    NodeApp_LastTxResult = Result;
    NodeApp_TxConfirmationCount++;
}

static TEST_UNUSED void Test_QueueNodeRx(const uint8_t *DataPtr,
                                         PduLengthType Length)
{
    assert(DataPtr != NULL && Length > 0U &&
           Length <= NODE_APP_MAX_NSDU_LENGTH);
    memcpy(Test_NodeRxData, DataPtr, Length);
    Test_NodeRxLength = Length;
    Test_NodeReadyCount = 1U;
}

void Com_MainFunctionTx(void)
{
    Test_RecordEvent('C');
    Test_SchedulerCount++;
}

void CanTp_MainFunction(void)
{
    Test_RecordEvent('T');
    Test_CanTpCount++;
}

void Can_MainFunction_Write(void)
{
    Test_RecordEvent('W');
    Test_WriteCount++;
}

void Can_MainFunction_Read(void)
{
    Test_RecordEvent('R');
    Test_ReadCount++;
}

UART_Status_t LPUART1_Init(uint32_t Baud)
{
    assert(Baud == 115200U);
    return UART_STATUS_OK;
}

void LPUART1_RegisterCallbacks(UART_RxCallback_t RxCallback,
                               UART_TxCallback_t TxCallback)
{
    Test_UartRxCallback = RxCallback;
    Test_UartTxCallback = TxCallback;
}

void LPUART1_EnableTxInterrupt(void)
{
    uint8_t value;
    assert(Test_UartTxCallback != NULL);
    while (Test_UartTxCallback(&value))
    {
        assert(Test_UartRawTxLength < sizeof(Test_UartRawTx));
        Test_UartRawTx[Test_UartRawTxLength++] = value;
    }
}

UART_Status_t LPUART1_SendChar_Blocking(char Value)
{
    assert(Test_UartRawTxLength < sizeof(Test_UartRawTx));
    Test_UartRawTx[Test_UartRawTxLength++] = (uint8_t)Value;
    return UART_STATUS_OK;
}

UART_Status_t LPUART1_SendString_Blocking(const char *Text)
{
    size_t length = strlen(Text);
    assert((size_t)Test_UartRawTxLength + length <= sizeof(Test_UartRawTx));
    memcpy(&Test_UartRawTx[Test_UartRawTxLength], Text, length);
    Test_UartRawTxLength = (uint16_t)(Test_UartRawTxLength + length);
    return UART_STATUS_OK;
}

static TEST_UNUSED void Test_InjectUart(const uint8_t *DataPtr,
                                        uint16_t Length)
{
    uint16_t index;
    assert(DataPtr != NULL && Test_UartRxCallback != NULL);
    for (index = 0U; index < Length; index++)
    {
        Test_UartRxCallback(DataPtr[index]);
    }
}

uint32_t Driver_SysTick_Init(uint32_t Frequency,
                             Driver_SysTick_Callback_t Callback)
{
    assert(Frequency == 1000U && Callback == NULL);
    Test_Tick = 0U;
    return 0U;
}

uint32_t Driver_SysTick_GetTicks(void)
{
    if (Test_Tick >= Test_StopAt)
    {
        longjmp(Test_StopPoint, 1);
    }
    Test_Tick++;
    if (Test_Tick == Test_JumpAt)
    {
        Test_Tick += 2U;
    }
    return Test_Tick;
}

static TEST_UNUSED void Test_RunApp(uint32_t StopAt)
{
    Test_StopAt = StopAt;
    if (setjmp(Test_StopPoint) == 0)
    {
        (void)App_Entry();
        assert(0 && "App_Entry unexpectedly returned");
    }
}

#endif /* APP_HOST_FIXTURE_H_ */
