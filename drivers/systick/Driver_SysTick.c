#include "Driver_SysTick.h"

#define DRIVER_SYSTICK_RELOAD_MASK (0x00FFFFFFUL)
#define DRIVER_SYSTICK_CTRL_ENABLE (S32_SysTick_CSR_ENABLE_MASK)
#define DRIVER_SYSTICK_CTRL_TICKINT (S32_SysTick_CSR_TICKINT_MASK)
#define DRIVER_SYSTICK_CTRL_CLKSOURCE (S32_SysTick_CSR_CLKSOURCE_MASK)

static volatile uint32_t driver_systick_ticks;
static Driver_SysTick_Callback_t driver_systick_callback;

static uint32_t driver_systick_config(uint32_t ticks)
{
    if ((ticks == 0U) || ((ticks - 1U) > DRIVER_SYSTICK_RELOAD_MASK))
    {
        return 1U;
    }

    S32_SysTick->RVR = ticks - 1U;
    S32_SysTick->CVR = 0U;
    S32_SysTick->CSR = DRIVER_SYSTICK_CTRL_CLKSOURCE | DRIVER_SYSTICK_CTRL_TICKINT | DRIVER_SYSTICK_CTRL_ENABLE;

    return 0U;
}

uint32_t Driver_SysTick_Init(uint32_t tick_hz, Driver_SysTick_Callback_t callback)
{
    uint32_t ticks;

    if ((tick_hz == 0U) || (SystemCoreClock < tick_hz))
    {
        return 1U;
    }

    ticks = SystemCoreClock / tick_hz;

    driver_systick_ticks = 0U;
    driver_systick_callback = callback;

    return driver_systick_config(ticks);
}

uint32_t Driver_SysTick_GetTicks(void)
{
    return driver_systick_ticks;
}

void Driver_SysTick_Handler(void)
{
    driver_systick_ticks++;

    if (driver_systick_callback != (Driver_SysTick_Callback_t)0)
    {
        driver_systick_callback();
    }
}

void SysTick_Handler(void)
{
    Driver_SysTick_Handler();
}
