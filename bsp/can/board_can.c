#include "S32K144.h"
#include "../LED.h"

/* Unlock and disable watchdog timer */
void disable_WDOG(void)
{
    WDOG->CNT = 0xD928C520U;
    while ((WDOG->CS & WDOG_CS_ULK_MASK) == 0U)
    {
    }
    WDOG->TOVAL = 0x0000FFFFU;
    WDOG->CS = (WDOG->CS & ~WDOG_CS_EN_MASK) | WDOG_CS_UPDATE_MASK;
}

/* Initialize external 8 MHz oscillator, CAN pin routing, and board peripherals */
void init_MCU(void)
{
    /* Disable SOSC prior to configuration */
    SCG->SOSCCSR &= ~SCG_SOSCCSR_SOSCEN_MASK;

    /* Set up external 8 MHz crystal oscillator */
    SCG->SOSCCFG = SCG_SOSCCFG_EREFS_MASK | SCG_SOSCCFG_HGO_MASK | SCG_SOSCCFG_RANGE(3);

    /* Output 8 MHz clock onto SOSCDIV2 */
    SCG->SOSCDIV = SCG_SOSCDIV_SOSCDIV1(1) | SCG_SOSCDIV_SOSCDIV2(1);

    /* Enable SOSC and wait for lock */
    SCG->SOSCCSR |= SCG_SOSCCSR_SOSCEN_MASK;
    while ((SCG->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK) == 0U)
    {
    }

    /* Enable clock gates for PORTC, PORTE, and FlexCAN0 */
	PCC->PCCn[PCC_PORTC_INDEX]    |= PCC_PCCn_CGC_MASK;
	PCC->PCCn[PCC_PORTE_INDEX]    |= PCC_PCCn_CGC_MASK;
	PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Mux PORTE[4] as CAN0_RX and PORTE[5] as CAN0_TX (ALT5) */
    PORTE->PCR[4] = PORT_PCR_MUX(5);
    PORTE->PCR[5] = PORT_PCR_MUX(5);

    PORTC->PCR[14] = PORT_PCR_MUX(1);
	PTC->PDDR |= (1UL << 14U);
	PTC->PCOR = (1UL << 14U);  /* Clear = 0 -> Normal Operation (Wake Up) */

	PORTE->PCR[11] = PORT_PCR_MUX(1);
	PTE->PDDR |= (1UL << 11U);
	PTE->PCOR = (1UL << 11U);

    /* Initialize Green LED using BSP driver */
    LED_Init(LED_GREEN);
}