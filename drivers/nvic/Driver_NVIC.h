#ifndef DRIVER_NVIC_H_
#define DRIVER_NVIC_H_

#include "S32K144.h"
#include <stdint.h>

/* Function prototypes for NVIC control */
void NVIC_EnableIRQ(IRQn_Type IRQn);
void NVIC_DisableIRQ(IRQn_Type IRQn);
void NVIC_SetPendingIRQ(IRQn_Type IRQn);
void NVIC_ClearPendingIRQ(IRQn_Type IRQn);
uint32_t NVIC_GetActive(IRQn_Type IRQn);
void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority);
uint32_t NVIC_GetPriority(IRQn_Type IRQn);

#endif /* DRIVER_NVIC_H_ */