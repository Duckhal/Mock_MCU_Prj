#ifndef DRIVER_LPIT_H_
#define DRIVER_LPIT_H_

#include <stdint.h>
#include "S32K144.h"

typedef void (*Driver_LPIT_Callback_t)(void);

/**
 * @brief Initialize LPIT module clock (FIRC 48MHz).
 */
uint32_t Driver_LPIT_Init(void);

/**
 * @brief Configure LPIT Channel 0 in Normal 32-bit periodic mode.
 * @param period_ticks Number of clock ticks between interrupts.
 * @param cb Callback executed upon channel timeout.
 */
void Driver_LPIT_ConfigChannel0(uint32_t period_ticks, Driver_LPIT_Callback_t cb);

/**
 * @brief Configure Channel 2 and 3 in Chain mode.
 * @param ch2_ticks Prescaler ticks on Channel 2.
 * @param ch3_ticks Chained timeout ticks on Channel 3.
 * @param ch3_cb Callback executed upon Channel 3 timeout.
 */
void Driver_LPIT_ConfigChain_Ch2_Ch3(uint32_t ch2_ticks, uint32_t ch3_ticks, Driver_LPIT_Callback_t ch3_cb);

/**
 * @brief Stop Channel 0 timer.
 */
void Driver_LPIT_StopChannel0(void);

#endif /* DRIVER_LPIT_H_ */