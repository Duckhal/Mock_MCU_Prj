#include "S32K144.h"
#include "../bsp/can/board_can.h"
#include "../bsp/LED.h"
#include "../drivers/can/can_driver/Can.h"
#include "../drivers/can/canif/CanIf.h"
#include "../drivers/can/cantp/Cantp.h"
#include "../drivers/can/com/Com.h"
#include "../app/node_app.h"
#include "../drivers/gpio/Driver_GPIO.h"
#include "../drivers/systick/Driver_SysTick.h"
#include "../drivers/uart/Driver_UART.h"
#include "system_S32K144.h"
#include "can_loopback_test.h"
#include <stddef.h>

#define APP_UART_BAUD        (115200U)
#define APP_DEBOUNCE_TICKS   (20U)

typedef enum
{
    APP_STARTING = 0,
    APP_RUNNING,
    APP_GPIO_FAILED,
    APP_UART_FAILED,
    APP_CAN_FAILED,
    APP_CANIF_FAILED,
    APP_NODE_FAILED,
    APP_CANTP_FAILED,
    APP_CANTP_LOOPBACK_FAILED,
    APP_COM_FAILED,
    APP_SYSTICK_FAILED,
    APP_SIGNAL_FAILED
} App_StatusType;

volatile App_StatusType g_AppStatus;
volatile uint8_t g_AppModeTx;
volatile uint32_t g_AppProcessedTicks;
volatile uint32_t g_AppTxSignalUpdates;
volatile uint32_t g_AppRxCommands;
volatile uint32_t g_AppInvalidRxCommands;
volatile uint32_t g_AppUartErrors;

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

/** Initialize the stack, require CanTp loopback PASS, then run the application. */
int main(void)
{
    uint32_t lastTick = 0U;
    uint32_t lastRxCount;
    uint32_t sw2ChangedAt = 0U;
    uint32_t sw3ChangedAt = 0U;
    uint32_t nextCommand = 0U;
    uint8_t sw2Raw = 1U;
    uint8_t sw2Stable = 1U;
    uint8_t sw3Raw = 1U;
    uint8_t sw3Stable = 1U;
    uint8_t txMode = 0U;

    g_AppStatus = APP_STARTING;
    g_AppModeTx = 0U;
    g_AppProcessedTicks = 0U;
    g_AppTxSignalUpdates = 0U;
    g_AppRxCommands = 0U;
    g_AppInvalidRxCommands = 0U;
    g_AppUartErrors = 0U;

    disable_WDOG();
    init_MCU();
    LED_Init(LED_BLUE);
    LED_Init(LED_RED);
    LED_Init(LED_GREEN);
    LED_Off(LED_BLUE);
    LED_Off(LED_RED);
    LED_Off(LED_GREEN);

    if ((Driver_GPIO0.Setup(GPIO_C12, NULL) != ARM_DRIVER_OK) ||
        (Driver_GPIO0.Setup(GPIO_C13, NULL) != ARM_DRIVER_OK) ||
        (Driver_GPIO0.SetDirection(GPIO_C12, ARM_GPIO_INPUT) != ARM_DRIVER_OK) ||
        (Driver_GPIO0.SetDirection(GPIO_C13, ARM_GPIO_INPUT) != ARM_DRIVER_OK) ||
        (Driver_GPIO0.SetPullResistor(GPIO_C12, ARM_GPIO_PULL_UP) != ARM_DRIVER_OK) ||
        (Driver_GPIO0.SetPullResistor(GPIO_C13, ARM_GPIO_PULL_UP) != ARM_DRIVER_OK))
    {
        g_AppStatus = APP_GPIO_FAILED;
        for (;;)
        {
        }
    }

    if (LPUART1_Init(APP_UART_BAUD) != UART_STATUS_OK)
    {
        g_AppStatus = APP_UART_FAILED;
        for (;;)
        {
        }
    }
    if (Can_Init() != CAN_OK)
    {
        g_AppStatus = APP_CAN_FAILED;
        for (;;)
        {
        }
    }
    if (CanIf_Init() != E_OK)
    {
        g_AppStatus = APP_CANIF_FAILED;
        for (;;)
        {
        }
    }
    if (NodeApp_Init() != E_OK)
    {
        g_AppStatus = APP_NODE_FAILED;
        for (;;)
        {
        }
    }
    if (CanTp_Init() != E_OK)
    {
        g_AppStatus = APP_CANTP_FAILED;
        for (;;)
        {
        }
    }
    if (Com_Init() != E_OK)
    {
        g_AppStatus = APP_COM_FAILED;
        for (;;)
        {
        }
    }

    SystemCoreClockUpdate();
    if (Driver_SysTick_Init(1000U, NULL) != 0U)
    {
        g_AppStatus = APP_SYSTICK_FAILED;
        for (;;)
        {
        }
    }
    if (CanTpLoopbackTest_Run() != E_OK)
    {
        g_AppStatus = APP_CANTP_LOOPBACK_FAILED;
        for (;;)
        {
        }
    }
    lastTick = Driver_SysTick_GetTicks();
    if (LPUART1_SendString_Blocking("CANTP LOOPBACK PASS\r\n") !=
        UART_STATUS_OK)
    {
        g_AppUartErrors++;
    }
    lastRxCount = Com_GetRxIndicationCount();
    g_AppStatus = APP_RUNNING;
    if (LPUART1_SendString_Blocking("MODE RX WAIT CAN\r\n") != UART_STATUS_OK)
    {
        g_AppUartErrors++;
    }

    for (;;)
    {
        uint32_t now = Driver_SysTick_GetTicks();
        while ((uint32_t)(now - lastTick) > 0U)
        {
            lastTick++;
            Can_MainFunction_Write();
            Can_MainFunction_Read();
            CanTp_MainFunction();

            /* Sample switches once at the newest observed tick. */
            if (lastTick == now)
            {
                uint8_t sw2 = (uint8_t)Driver_GPIO0.GetInput(GPIO_C12);
                uint8_t sw3 = (uint8_t)Driver_GPIO0.GetInput(GPIO_C13);
                uint8_t wasTx = txMode;

                if (sw2 != sw2Raw)
                {
                    sw2Raw = sw2;
                    sw2ChangedAt = now;
                }
                if ((sw2 != sw2Stable) &&
                    ((uint32_t)(now - sw2ChangedAt) >= APP_DEBOUNCE_TICKS))
                {
                    sw2Stable = sw2;
                    if (sw2 == 0U)
                    {
                        txMode ^= 1U;
                        g_AppModeTx = txMode;
                        LED_Off(LED_BLUE);
                        LED_Off(LED_RED);
                        LED_Off(LED_GREEN);
                        if (txMode != 0U)
                        {
                            LED_On(LED_RED);
                            if (LPUART1_SendString_Blocking("MODE TX SW3=UPDATE\r\n") != UART_STATUS_OK)
                            {
                                g_AppUartErrors++;
                            }
                        }
                        else
                        {
                            lastRxCount = Com_GetRxIndicationCount();
                            if (LPUART1_SendString_Blocking("MODE RX WAIT CAN\r\n") != UART_STATUS_OK)
                            {
                                g_AppUartErrors++;
                            }
                        }
                    }
                }

                if (sw3 != sw3Raw)
                {
                    sw3Raw = sw3;
                    sw3ChangedAt = now;
                }
                if ((sw3 != sw3Stable) &&
                    ((uint32_t)(now - sw3ChangedAt) >= APP_DEBOUNCE_TICKS))
                {
                    sw3Stable = sw3;
                    if ((sw3 == 0U) && (wasTx != 0U) && (txMode != 0U))
                    {
                        if (Com_SendSignal(COM_SIGNAL_LED_COMMAND, &nextCommand) != E_OK)
                        {
                            g_AppStatus = APP_SIGNAL_FAILED;
                            for (;;)
                            {
                            }
                        }
                        g_AppTxSignalUpdates++;
                        if (LPUART1_SendString_Blocking(App_TxMessages[nextCommand]) != UART_STATUS_OK)
                        {
                            g_AppUartErrors++;
                        }
                        nextCommand = (nextCommand + 1U) & 3U;
                    }
                }

                if ((txMode == 0U) && (Com_GetRxIndicationCount() != lastRxCount))
                {
                    uint32_t command = 0U;
                    lastRxCount = Com_GetRxIndicationCount();
                    if (Com_ReceiveSignal(COM_SIGNAL_RX_LED_COMMAND, &command) != E_OK)
                    {
                        g_AppStatus = APP_SIGNAL_FAILED;
                        for (;;)
                        {
                        }
                    }
                    if (command <= 3U)
                    {
                        LED_Off(LED_BLUE);
                        LED_Off(LED_RED);
                        LED_Off(LED_GREEN);
                        if ((command & 1U) != 0U)
                        {
                            LED_On(LED_GREEN);
                        }
                        if ((command & 2U) != 0U)
                        {
                            LED_On(LED_BLUE);
                        }
                        g_AppRxCommands++;
                        if (LPUART1_SendString_Blocking(App_RxMessages[command]) != UART_STATUS_OK)
                        {
                            g_AppUartErrors++;
                        }
                    }
                    else
                    {
                        g_AppInvalidRxCommands++;
                    }
                }
                else if (txMode != 0U)
                {
                    lastRxCount = Com_GetRxIndicationCount();
                }
            }

            /* COM is periodic in Tx; Rx performs no transmission scheduling. */
            if (txMode != 0U)
            {
                Com_MainFunctionTx();
            }
            g_AppProcessedTicks++;
        }
    }
}
