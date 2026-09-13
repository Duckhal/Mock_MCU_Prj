#include "Driver_LPIT.h"
#include "../nvic/Driver_NVIC.h"

static Driver_LPIT_Callback_t s_lpit_ch0_cb = (Driver_LPIT_Callback_t)0;
static Driver_LPIT_Callback_t s_lpit_ch3_cb = (Driver_LPIT_Callback_t)0;

uint32_t Driver_LPIT_Init(void)
{
    /* Enable LPIT clock from FIRC (PCS = 3) */
    PCC->PCCn[PCC_LPIT_INDEX] &= ~PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_LPIT_INDEX] = (PCC->PCCn[PCC_LPIT_INDEX] & ~PCC_PCCn_PCS_MASK) | PCC_PCCn_PCS(3U);
    PCC->PCCn[PCC_LPIT_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Software reset LPIT */
    LPIT0->MCR = LPIT_MCR_SW_RST_MASK;
    LPIT0->MCR = 0U;

    /* Enable module with DOZE_EN (keeps timer running in WFI) and DBG_EN */
    LPIT0->MCR = LPIT_MCR_M_CEN_MASK | LPIT_MCR_DOZE_EN_MASK | LPIT_MCR_DBG_EN_MASK;

    return 0U;
}

void Driver_LPIT_ConfigChannel0(uint32_t period_ticks, Driver_LPIT_Callback_t cb)
{
    s_lpit_ch0_cb = cb;

    /* Load timeout period and enable Channel 0 in 32-bit periodic counter mode */
    LPIT0->TMR[0].TVAL = period_ticks - 1U;
    LPIT0->TMR[0].TCTRL = LPIT_TMR_TCTRL_MODE(0U) | LPIT_TMR_TCTRL_T_EN_MASK;
    LPIT0->MIER |= LPIT_MIER_TIE0_MASK;

    /* Configure and enable NVIC interrupt for Channel 0 */
    NVIC_SetPriority(LPIT0_Ch0_IRQn, 2U);
    NVIC_ClearPendingIRQ(LPIT0_Ch0_IRQn);
    NVIC_EnableIRQ(LPIT0_Ch0_IRQn);
}

void Driver_LPIT_ConfigChain_Ch2_Ch3(uint32_t ch2_ticks, uint32_t ch3_ticks, Driver_LPIT_Callback_t ch3_cb)
{
    s_lpit_ch3_cb = ch3_cb;

    /* Stop both channels before reconfiguring */
    LPIT0->CLRTEN |= (LPIT_CLRTEN_CLR_T_EN_2_MASK | LPIT_CLRTEN_CLR_T_EN_3_MASK);

    /* Clear pending interrupt flags */
    LPIT0->MSR = LPIT_MSR_TIF2_MASK | LPIT_MSR_TIF3_MASK;

    /* Load reload values for base prescaler (Ch2) and chained counter (Ch3) */
    LPIT0->TMR[2].TVAL = ch2_ticks - 1U;
    LPIT0->TMR[3].TVAL = ch3_ticks - 1U;

    /* Enable Channel 3 first in chained mode */
    LPIT0->TMR[3].TCTRL = LPIT_TMR_TCTRL_CHAIN_MASK | LPIT_TMR_TCTRL_MODE(0U) | LPIT_TMR_TCTRL_T_EN_MASK;

    /* Enable Channel 2 afterwards to supply prescaler clock pulses to Channel 3 */
    LPIT0->TMR[2].TCTRL = LPIT_TMR_TCTRL_MODE(0U) | LPIT_TMR_TCTRL_T_EN_MASK;

    /* Enable Channel 3 interrupt */
    LPIT0->MIER |= LPIT_MIER_TIE3_MASK;

    /* Configure and enable NVIC interrupt for Channel 3 */
    NVIC_SetPriority(LPIT0_Ch3_IRQn, 2U);
    NVIC_ClearPendingIRQ(LPIT0_Ch3_IRQn);
    NVIC_EnableIRQ(LPIT0_Ch3_IRQn);
}

void Driver_LPIT_StopChannel0(void)
{
    LPIT0->CLRTEN |= LPIT_CLRTEN_CLR_T_EN_0_MASK;
    LPIT0->MIER &= ~LPIT_MIER_TIE0_MASK;
    NVIC_DisableIRQ(LPIT0_Ch0_IRQn);
}

void LPIT0_Ch0_IRQHandler(void)
{
    LPIT0->MSR = LPIT_MSR_TIF0_MASK;
    if (s_lpit_ch0_cb != (Driver_LPIT_Callback_t)0)
    {
        s_lpit_ch0_cb();
    }
}

void LPIT0_Ch3_IRQHandler(void)
{
    LPIT0->MSR = LPIT_MSR_TIF3_MASK;
    if (s_lpit_ch3_cb != (Driver_LPIT_Callback_t)0)
    {
        s_lpit_ch3_cb();
    }
}
