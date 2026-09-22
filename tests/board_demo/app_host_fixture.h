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

static jmp_buf Test_StopPoint;
static uint32_t Test_Tick;
static uint32_t Test_StopAt;
static uint32_t Test_RxAt;
static uint32_t Test_RxValue;
static uint32_t Test_RxSignalUpdateCount;
static uint32_t Test_JumpAt;
static uint8_t Test_RxInjected;
static uint8_t Test_RxValid;
static uint8_t Test_LedState[3];
static uint32_t Test_SentCommands[8];
static uint32_t Test_SendCount;
static uint32_t Test_ReceiveCount;
static uint32_t Test_SchedulerCount;
static uint32_t Test_WriteCount;
static uint32_t Test_ReadCount;
static uint32_t Test_CanTpCount;
static uint32_t Test_CanTpLoopbackCount;
static uint32_t Test_CanTpLoopbackEnableCount;
static uint32_t Test_PduRInitCount;
static uint32_t Test_NodeReadyCount;
static uint32_t Test_NodeTransmitCount;
static PduLengthType Test_NodeRxLength;
static uint8_t Test_NodeRxData[NODE_APP_MAX_NSDU_LENGTH];
static PduLengthType Test_NodeTxLength;
static uint8_t Test_NodeTxData[NODE_APP_MAX_NSDU_LENGTH];
static char Test_JumpEvents[16];
static uint8_t Test_JumpEventCount;
static char Test_LastUart[64];
static uint32_t Test_UartStringCount;
static UART_RxCallback_t Test_UartRxCallback;
static uint8_t Test_UartRawTx[256];
static uint16_t Test_UartRawTxLength;

volatile uint32_t Com_RxIndicationCount;
volatile uint32_t NodeApp_TxConfirmationCount;
volatile Std_ReturnType NodeApp_LastTxResult;

uint32_t Com_GetRxIndicationCount(void)
{ return Com_RxIndicationCount; }

/** Record poll ordering during the one simulated three-tick backlog. */
static void Test_RecordEvent(char event)
{
    if ((Test_JumpAt != 0U) && (Test_Tick == Test_JumpAt + 2U))
    {
        assert(Test_JumpEventCount < sizeof(Test_JumpEvents));
        Test_JumpEvents[Test_JumpEventCount++] = event;
    }
}

/** Return active-low switch levels for two button presses per switch. */
static uint32_t Test_GpioInput(ARM_GPIO_Pin_t pin)
{
    if (pin == GPIO_C12)
    {
        Test_RecordEvent('A');
        return ((Test_Tick >= 5U && Test_Tick <= 30U) ||
                (Test_Tick >= 110U && Test_Tick <= 135U)) ? 0U : 1U;
    }
    assert(pin == GPIO_C13);
    return ((Test_Tick >= 40U && Test_Tick <= 65U) ||
            (Test_Tick >= 90U && Test_Tick <= 115U)) ? 0U : 1U;
}

static int32_t Test_GpioSetup(ARM_GPIO_Pin_t pin, ARM_GPIO_SignalEvent_t cb)
{ assert((pin == GPIO_C12 || pin == GPIO_C13) && cb == NULL); return ARM_DRIVER_OK; }

static int32_t Test_GpioDirection(ARM_GPIO_Pin_t pin, ARM_GPIO_DIRECTION direction)
{ assert((pin == GPIO_C12 || pin == GPIO_C13) && direction == ARM_GPIO_INPUT); return ARM_DRIVER_OK; }

static int32_t Test_GpioPull(ARM_GPIO_Pin_t pin, ARM_GPIO_PULL_RESISTOR pull)
{ assert((pin == GPIO_C12 || pin == GPIO_C13) && pull == ARM_GPIO_PULL_UP); return ARM_DRIVER_OK; }

ARM_DRIVER_GPIO Driver_GPIO0 =
{
    .Setup = Test_GpioSetup,
    .SetDirection = Test_GpioDirection,
    .SetPullResistor = Test_GpioPull,
    .GetInput = Test_GpioInput
};

static unsigned Test_LedIndex(ARM_GPIO_Pin_t pin)
{
    if (pin == LED_BLUE) { return 0U; }
    if (pin == LED_RED) { return 1U; }
    assert(pin == LED_GREEN);
    return 2U;
}

void LED_Init(ARM_GPIO_Pin_t pin) { Test_LedState[Test_LedIndex(pin)] = 0U; }
void LED_On(ARM_GPIO_Pin_t pin) { Test_LedState[Test_LedIndex(pin)] = 1U; }
void LED_Off(ARM_GPIO_Pin_t pin) { Test_LedState[Test_LedIndex(pin)] = 0U; }

void disable_WDOG(void) {}
void init_MCU(void) {}
void SystemCoreClockUpdate(void) {}
Can_ReturnType Can_Init(void) { return CAN_OK; }
Std_ReturnType CanIf_Init(void) { return E_OK; }
Std_ReturnType NodeApp_Init(void) { return E_OK; }
Std_ReturnType CanTp_Init(void) { return E_OK; }
Std_ReturnType PduR_Init(void)
{ Test_PduRInitCount++; return E_OK; }
Std_ReturnType CanTpLoopbackTest_Run(void)
{ Test_CanTpLoopbackCount++; return E_OK; }
Std_ReturnType CanTpLoopbackTest_SetEnabled(uint8_t enable)
{
    assert(enable <= 1U);
    Test_CanTpLoopbackEnableCount++;
    return E_OK;
}
Std_ReturnType Com_Init(void)
{
    Com_RxIndicationCount = 0U;
    Test_RxSignalUpdateCount = 0U;
    return E_OK;
}

uint32_t Com_GetRxSignalUpdateCount(PduIdType signalId)
{
    assert(signalId == COM_SIGNAL_RX_LED_COMMAND);
    return Test_RxSignalUpdateCount;
}

uint8_t NodeApp_GetReadyCount(void)
{ return (uint8_t)Test_NodeReadyCount; }

Std_ReturnType NodeApp_Receive(uint8_t *data, PduLengthType capacity,
                               PduLengthType *length)
{
    if ((data == NULL) || (length == NULL) || (Test_NodeReadyCount == 0U) ||
        (capacity < Test_NodeRxLength))
    {
        return E_NOT_OK;
    }
    memcpy(data, Test_NodeRxData, Test_NodeRxLength);
    *length = Test_NodeRxLength;
    Test_NodeReadyCount--;
    return E_OK;
}

Std_ReturnType NodeApp_Transmit(const uint8_t *data, PduLengthType length)
{
    if ((data == NULL) || (length == 0U) ||
        (length > NODE_APP_MAX_NSDU_LENGTH))
    {
        return E_NOT_OK;
    }
    memcpy(Test_NodeTxData, data, length);
    Test_NodeTxLength = length;
    Test_NodeTransmitCount++;
    return E_OK;
}

Std_ReturnType Com_SendSignal(PduIdType signalId, const void *value)
{
    assert(signalId == COM_SIGNAL_LED_COMMAND && value != NULL);
    assert(Test_SendCount < 8U);
    memcpy(&Test_SentCommands[Test_SendCount], value, sizeof(uint32_t));
    Test_SendCount++;
    return E_OK;
}

Std_ReturnType Com_ReceiveSignal(PduIdType signalId, void *value)
{
    assert(signalId == COM_SIGNAL_RX_LED_COMMAND && value != NULL);
    if (Test_RxValid == 0U) { return E_NOT_OK; }
    memcpy(value, &Test_RxValue, sizeof(uint32_t));
    Test_ReceiveCount++;
    return E_OK;
}

void Com_MainFunctionTx(void)
{ Test_RecordEvent('C'); Test_SchedulerCount++; }

void CanTp_MainFunction(void)
{ Test_RecordEvent('T'); Test_CanTpCount++; }

void Can_MainFunction_Write(void)
{ Test_RecordEvent('W'); Test_WriteCount++; }

void Can_MainFunction_Read(void)
{
    Test_RecordEvent('R');
    Test_ReadCount++;
    if ((Test_RxAt != 0U) && (Test_Tick == Test_RxAt) &&
        (Test_RxInjected == 0U))
    {
        Test_RxInjected = 1U;
        Test_RxValid = 1U;
        Com_RxIndicationCount++;
        Test_RxSignalUpdateCount++;
    }
}

UART_Status_t LPUART1_Init(uint32_t baud)
{ assert(baud == 115200U); return UART_STATUS_OK; }

void LPUART1_RegisterCallbacks(UART_RxCallback_t rxCb,
                               UART_TxCallback_t txCb)
{
    assert(txCb == NULL);
    Test_UartRxCallback = rxCb;
}

UART_Status_t LPUART1_SendChar_Blocking(char value)
{
    assert(Test_UartRawTxLength < sizeof(Test_UartRawTx));
    Test_UartRawTx[Test_UartRawTxLength++] = (uint8_t)value;
    return UART_STATUS_OK;
}

UART_Status_t LPUART1_SendString_Blocking(const char *line)
{
    size_t length = strlen(line);
    assert(length < sizeof(Test_LastUart));
    memcpy(Test_LastUart, line, length + 1U);
    Test_UartStringCount++;
    return UART_STATUS_OK;
}

/** Inject raw PC bytes through the registered production UART callback. */
void Test_InjectUart(const uint8_t *data, uint16_t length)
{
    uint16_t index;
    assert(data != NULL && Test_UartRxCallback != NULL);
    for (index = 0U; index < length; index++)
    {
        Test_UartRxCallback(data[index]);
    }
}

uint32_t Driver_SysTick_Init(uint32_t frequency, Driver_SysTick_Callback_t callback)
{ assert(frequency == 1000U && callback == NULL); Test_Tick = 0U; return 0U; }

uint32_t Driver_SysTick_GetTicks(void)
{
    if (Test_Tick >= Test_StopAt) { longjmp(Test_StopPoint, 1); }
    Test_Tick++;
    if (Test_Tick == Test_JumpAt) { Test_Tick += 2U; }
    return Test_Tick;
}

/** Run the real application entry with deterministic virtual peripherals. */
static void Test_RunApp(uint32_t stopAt)
{
    Test_StopAt = stopAt;
    if (setjmp(Test_StopPoint) == 0)
    {
        (void)App_Entry();
        assert(0 && "App_Entry unexpectedly returned");
    }
}

#endif /* APP_HOST_FIXTURE_H_ */
