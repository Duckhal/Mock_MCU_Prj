#ifndef APP_HOST_FIXTURE_H_
#define APP_HOST_FIXTURE_H_

#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#define main App_Entry
#include "../../src/main.c"
#undef main

static jmp_buf Test_StopPoint;
static uint32_t Test_Tick;
static uint32_t Test_StopAt;
static uint32_t Test_RxAt;
static uint32_t Test_RxValue;
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
static char Test_JumpEvents[16];
static uint8_t Test_JumpEventCount;
static char Test_LastUart[64];

volatile uint32_t Com_RxIndicationCount;

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
    { return ((Test_Tick >= 5U && Test_Tick <= 30U) ||
              (Test_Tick >= 110U && Test_Tick <= 135U)) ? 0U : 1U; }
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
Std_ReturnType CanTpLoopbackTest_Run(void)
{ Test_CanTpLoopbackCount++; return E_OK; }
Std_ReturnType Com_Init(void) { Com_RxIndicationCount = 0U; return E_OK; }

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
    }
}

UART_Status_t LPUART1_Init(uint32_t baud)
{ assert(baud == 115200U); return UART_STATUS_OK; }

UART_Status_t LPUART1_SendString_Blocking(const char *line)
{
    size_t length = strlen(line);
    assert(length < sizeof(Test_LastUart));
    memcpy(Test_LastUart, line, length + 1U);
    return UART_STATUS_OK;
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
