#include "S32K144.h"
#include "../app/app.h"
#include "../bsp/can/board_can.h"
#include "../drivers/can/can_driver/Can.h"
#include "../drivers/can/canif/CanIf.h"
#include "../drivers/can/cantp/Cantp.h"
#include "../drivers/can/com/Com.h"
#include "../drivers/can/pdur/PduR.h"
#include "../drivers/systick/Driver_SysTick.h"
#include "system_S32K144.h"
#include "can_loopback_test.h"
#include <stddef.h>

#ifndef SYSTEM_RUN_CANTP_LOOPBACK_TEST
#define SYSTEM_RUN_CANTP_LOOPBACK_TEST (0U)
#endif

#ifndef SYSTEM_ENABLE_UART_CANTP_LOOPBACK
#define SYSTEM_ENABLE_UART_CANTP_LOOPBACK (0U)
#endif

typedef enum
{
    SYSTEM_STARTING = 0,
    SYSTEM_RUNNING,
    SYSTEM_APP_HARDWARE_FAILED,
    SYSTEM_CAN_FAILED,
    SYSTEM_CANIF_FAILED,
    SYSTEM_CANTP_FAILED,
    SYSTEM_PDUR_FAILED,
    SYSTEM_COM_FAILED,
    SYSTEM_APP_INIT_FAILED,
    SYSTEM_SYSTICK_FAILED,
    SYSTEM_CANTP_LOOPBACK_FAILED,
    SYSTEM_APP_RUNTIME_FAILED
} System_StatusType;

volatile System_StatusType g_SystemStatus;
volatile uint32_t g_SystemProcessedTicks;

/** Latch a system-level failure for the debugger and stop scheduling. */
static void System_Fail(System_StatusType Status)
{
    g_SystemStatus = Status;
    for (;;)
    {
    }
}

/** Initialize hardware, the communication stack, application, and scheduler. */
int main(void)
{
    uint32_t lastTick;

    g_SystemStatus = SYSTEM_STARTING;
    g_SystemProcessedTicks = 0U;

    disable_WDOG();
    init_MCU();
    if (App_HardwareInit() != E_OK)
    {
        System_Fail(SYSTEM_APP_HARDWARE_FAILED);
    }

    if (Can_Init() != CAN_OK)
    {
        System_Fail(SYSTEM_CAN_FAILED);
    }
    if (CanIf_Init() != E_OK)
    {
        System_Fail(SYSTEM_CANIF_FAILED);
    }
    if (PduR_Init() != E_OK)
    {
        System_Fail(SYSTEM_PDUR_FAILED);
    }
    if (CanTp_Init() != E_OK)
    {
        System_Fail(SYSTEM_CANTP_FAILED);
    }
    if (Com_Init() != E_OK)
    {
        System_Fail(SYSTEM_COM_FAILED);
    }
    if (App_Init() != E_OK)
    {
        System_Fail(SYSTEM_APP_INIT_FAILED);
    }

    SystemCoreClockUpdate();
    if (Driver_SysTick_Init(1000U, NULL) != 0U)
    {
        System_Fail(SYSTEM_SYSTICK_FAILED);
    }

#if SYSTEM_RUN_CANTP_LOOPBACK_TEST != 0U
    if (CanTpLoopbackTest_Run() != E_OK)
    {
        System_Fail(SYSTEM_CANTP_LOOPBACK_FAILED);
    }
#endif

#if SYSTEM_ENABLE_UART_CANTP_LOOPBACK != 0U
    if (CanTpLoopbackTest_SetEnabled(1U) != E_OK)
    {
        System_Fail(SYSTEM_CANTP_LOOPBACK_FAILED);
    }
    if (App_SetCanTpLoopbackMode(1U) != E_OK)
    {
        System_Fail(SYSTEM_CANTP_LOOPBACK_FAILED);
    }
#endif

    lastTick = Driver_SysTick_GetTicks();
    g_SystemStatus = SYSTEM_RUNNING;

    for (;;)
    {
        uint32_t now = Driver_SysTick_GetTicks();
        while ((uint32_t)(now - lastTick) > 0U)
        {
            lastTick++;
            Can_MainFunction_Write();
            Can_MainFunction_Read();
            CanTp_MainFunction();
            if (App_MainFunction(lastTick) != E_OK)
            {
                System_Fail(SYSTEM_APP_RUNTIME_FAILED);
            }
            if (App_IsComTxEnabled() != 0U)
            {
                Com_MainFunctionTx();
            }
            g_SystemProcessedTicks++;
        }
    }
}
