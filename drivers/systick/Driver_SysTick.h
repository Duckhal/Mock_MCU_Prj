#ifndef DRIVER_SYSTICK_H
#define DRIVER_SYSTICK_H

#include "S32K144.h"
#include "system_S32K144.h"
#include <stdint.h>

typedef void (*Driver_SysTick_Callback_t)(void);

uint32_t Driver_SysTick_Init(uint32_t tick_hz, Driver_SysTick_Callback_t callback);
uint32_t Driver_SysTick_GetTicks(void);
void Driver_SysTick_Handler(void);
void SysTick_Handler(void);

#endif