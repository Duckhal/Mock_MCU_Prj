#include "Driver_NVIC.h"
#include <stdint.h>
#include <S32K144_features.h>

#define NVIC_REGISTER_INDEX(irq) (((uint32_t)(irq)) >> 5U)
#define NVIC_BIT_MASK(irq) (1UL << (((uint32_t)(irq)) & 0x1FU))

#define NVIC_PRIORITY_BITS FEATURE_NVIC_PRIO_BITS
#define NVIC_PRIORITY_SHIFT (8U - NVIC_PRIORITY_BITS)
#define NVIC_PRIORITY_MASK ((1UL << NVIC_PRIORITY_BITS) - 1UL)

static uint8_t encode_priority(uint32_t priority)
{
    priority &= NVIC_PRIORITY_MASK;

    return (uint8_t)(priority << NVIC_PRIORITY_SHIFT);
}

void NVIC_EnableIRQ(IRQn_Type IRQn)
{
    if ((int32_t)IRQn >= 0)
    {
        S32_NVIC->ISER[NVIC_REGISTER_INDEX(IRQn)] = NVIC_BIT_MASK(IRQn);
    }
}

void NVIC_DisableIRQ(IRQn_Type IRQn)
{
    if ((int32_t)IRQn >= 0)
    {
        S32_NVIC->ICER[NVIC_REGISTER_INDEX(IRQn)] = NVIC_BIT_MASK(IRQn);
    }
}

void NVIC_SetPendingIRQ(IRQn_Type IRQn)
{
    if ((int32_t)IRQn >= 0)
    {
        S32_NVIC->ISPR[NVIC_REGISTER_INDEX(IRQn)] = NVIC_BIT_MASK(IRQn);
    }
}

void NVIC_ClearPendingIRQ(IRQn_Type IRQn)
{
    if ((int32_t)IRQn >= 0)
    {
        S32_NVIC->ICPR[NVIC_REGISTER_INDEX(IRQn)] = NVIC_BIT_MASK(IRQn);
    }
}

uint32_t NVIC_GetActive(IRQn_Type IRQn)
{
    if ((int32_t)IRQn < 0)
    {
        return 0U;
    }

    return (S32_NVIC->IABR[NVIC_REGISTER_INDEX(IRQn)] & NVIC_BIT_MASK(IRQn)) != 0U;
}

void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority)
{
    uint8_t encoded_priority = encode_priority(priority);

    if ((int32_t)IRQn >= 0)
    {
        if ((uint32_t)IRQn < S32_NVIC_IP_COUNT)
        {
            S32_NVIC->IP[(uint32_t)IRQn] = encoded_priority;
        }
    }
    else
    {
        int32_t exception_number = (int32_t)IRQn + 16;

        if ((exception_number >= 4) && (exception_number <= 15))
        {
            volatile uint8_t* system_handler_priority = (volatile uint8_t*)&S32_SCB->SHPR1;

            system_handler_priority[exception_number - 4] = encoded_priority;
        }
    }
}

uint32_t NVIC_GetPriority(IRQn_Type IRQn)
{
    uint8_t priority_value;

    if ((int32_t)IRQn >= 0)
    {
        if ((uint32_t)IRQn >= S32_NVIC_IP_COUNT)
        {
            return 0U;
        }

        priority_value = S32_NVIC->IP[(uint32_t)IRQn];
    }
    else
    {
        int32_t exception_number = (int32_t)IRQn + 16;

        if ((exception_number < 4) || (exception_number > 15))
        {
            return 0U;
        }
        else
        {
            volatile uint8_t* system_handler_priority = (volatile uint8_t*)&S32_SCB->SHPR1;

            priority_value = system_handler_priority[exception_number - 4];
        }
    }

    return ((uint32_t)priority_value >> NVIC_PRIORITY_SHIFT) & NVIC_PRIORITY_MASK;
}
