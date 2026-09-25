#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <stdint.h>

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

extern volatile System_StatusType g_SystemStatus;
extern volatile uint32_t g_SystemProcessedTicks;

/* Initialize hardware, the communication stack, application, and SysTick. */
void System_Init(void);

/* Process every pending 1 ms scheduler tick in the required stack order. */
void System_RunTask(void);

#endif /* SYSTEM_H_ */
