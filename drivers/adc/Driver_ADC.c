#include "Driver_ADC.h"
#include "../nvic/Driver_NVIC.h"
#include "../systick/Driver_SysTick.h"
#include "system_S32K144.h"

#define ADC_CALIBRATION_TIMEOUT    (0x100000UL)

volatile ADC_INFOR_t  g_adcInfo;
volatile uint8_t      g_adcMode = ADC_MODE_SW_TRIGGER;
volatile float        g_adcVoltageBuffer[ADC_RAM_BUFFER_SIZE];
volatile uint32_t     g_adcBufferIdx = 0U;

static uint8_t adc_channel_index = ADC_CHANNEL_12;

static uint32_t ADC_ConfigClock(void)
{
    /* Enable FIRC divider 2 */
    SCG->FIRCDIV = (SCG->FIRCDIV & ~SCG_FIRCDIV_FIRCDIV2_MASK) | SCG_FIRCDIV_FIRCDIV2(1U);

    /* Clock ADC0 from FIRC */
    PCC->PCCn[PCC_ADC0_INDEX] &= ~PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_ADC0_INDEX] = (PCC->PCCn[PCC_ADC0_INDEX] & ~PCC_PCCn_PCS_MASK) | PCC_PCCn_PCS(3U);
    PCC->PCCn[PCC_ADC0_INDEX] |= PCC_PCCn_CGC_MASK;

    if ((PCC->PCCn[PCC_ADC0_INDEX] & PCC_PCCn_CGC_MASK) == 0U)
    {
        return ADC_STATUS_ERROR;
    }

    return ADC_STATUS_OK;
}

static void ADC_ConfigHardwareTrigger(void)
{
    /* Enable Clocks for TRGMUX and LPIT0 */
    PCC->PCCn[PCC_LPIT_INDEX] &= ~PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_LPIT_INDEX] = (PCC->PCCn[PCC_LPIT_INDEX] & ~PCC_PCCn_PCS_MASK) | PCC_PCCn_PCS(3U); /* FIRC */
    PCC->PCCn[PCC_LPIT_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Route LPIT0 Channel 0 output (Source 17) to ADC0 Trigger */
    TRGMUX->TRGMUXn[TRGMUX_ADC0_INDEX] = TRGMUX_TRGMUXn_SEL0(17U);

    /* Configure SIM ADC Options: PDB / TRGMUX pre-trigger */
    SIM->ADCOPT &= ~(SIM_ADCOPT_ADC0TRGSEL_MASK | SIM_ADCOPT_ADC0PRETRGSEL_MASK | SIM_ADCOPT_ADC0SWPRETRG_MASK);
    SIM->ADCOPT |= SIM_ADCOPT_ADC0TRGSEL(1U) | SIM_ADCOPT_ADC0PRETRGSEL(1U);

    /* Set bit ADTRG to receive hardware trigger signal */
    ADC0->SC2 |= (1U << 6);

    /* Configure LPIT0: Reset, enable module, periodic mode on CH0 (~100ms interval) */
    LPIT0->MCR = LPIT_MCR_M_CEN_MASK;
    LPIT0->TMR[0].TVAL = 4800000U - 1U; /* 48 MHz FIRC / 10 = 100 ms trigger period */
    LPIT0->TMR[0].TCTRL = LPIT_TMR_TCTRL_MODE(0U) | LPIT_TMR_TCTRL_T_EN_MASK;
}

uint32_t ADC_Init(uint8_t adcMode)
{
    if ((adcMode != ADC_MODE_SW_TRIGGER) && (adcMode != ADC_MODE_HW_TRIGGER))
    {
        return ADC_STATUS_ERROR;
    }

    if (ADC_ConfigClock() != ADC_STATUS_OK)
    {
        return ADC_STATUS_ERROR;
    }

    /* Configure Potentiometer Pin (PTC14 / ADC0_SE12) as Analog input */
    PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    PORTC->PCR[14] &= ~PORT_PCR_MUX_MASK;

    ADC0->SC1[0] = ADC_SC1_ADCH(ADC_CHANNEL_DISABLED);
    ADC0->CFG1 = ADC_CFG1_MODE(1U) | ADC_CFG1_ADICLK(0U) | ADC_CFG1_ADIV(1U); /* 12-bit mode */
    ADC0->CFG2 = ADC_CFG2_SMPLTS(12U);

    if (ADC_Calibrate() != ADC_STATUS_OK)
    {
        return ADC_STATUS_ERROR;
    }

    g_adcMode = adcMode;
    g_adcInfo.readCount = 0U;
    g_adcInfo.lastValue = 0U;
    g_adcInfo.lastVoltage = 0.0f;
    g_adcInfo.averageValue = 0U;
    g_adcInfo.averageVoltage = 0.0f;
    g_adcInfo.channel = ADC_CHANNEL_12;
    g_adcInfo.timestamp.hour = 0U;
    g_adcInfo.timestamp.minute = 0U;
    g_adcInfo.timestamp.second = 0U;
    g_adcInfo.compareResult = 0U;
    g_adcBufferIdx = 0U;

    adc_channel_index = ADC_CHANNEL_12;

    if (g_adcMode == ADC_MODE_SW_TRIGGER)
    {
        /* Mode 2: Hardware Average 4 samples (AVGE=1, AVGS=0), Standard Polling (ACFE=0) */
        ADC0->SC3 = ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(0U);
        ADC0->SC2 = ADC_SC2_REFSEL(0U);
        ADC0->SC1[0] = ADC_SC1_ADCH(ADC_CHANNEL_DISABLED);

        /* Disable NVIC IRQ for Polling mode to avoid COCO auto-clear contention */
        NVIC_DisableIRQ(ADC0_IRQn);
    }
    else
    {
        /* Mode 1: Hardware Trigger via LPIT_CH0 */
        ADC_ConfigHardwareTrigger();

        ADC0->SC2 = ADC_SC2_REFSEL(0U) | ADC_SC2_ADTRG_MASK;
        ADC0->SC3 = 0U;
        ADC0->SC1[0] = ADC_SC1_ADCH(ADC_CHANNEL_12) | ADC_SC1_AIEN_MASK;

        /* Enable NVIC IRQ only for Hardware Trigger mode */
        NVIC_ClearPendingIRQ(ADC0_IRQn);
        NVIC_SetPriority(ADC0_IRQn, 2U);
        NVIC_EnableIRQ(ADC0_IRQn);
    }

    return ADC_STATUS_OK;
}

uint32_t ADC_Calibrate(void)
{
    uint32_t timeout = ADC_CALIBRATION_TIMEOUT;

    ADC0->SC1[0] = ADC_SC1_ADCH(ADC_CHANNEL_DISABLED);
    ADC0->SC3 = ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(3U) | ADC_SC3_CAL_MASK;

    while (((ADC0->SC3 & ADC_SC3_CAL_MASK) != 0U) && (timeout > 0U))
    {
        timeout--;
    }

    if ((ADC0->SC3 & ADC_SC3_CAL_MASK) != 0U)
    {
        ADC0->SC3 = 0U;
        return ADC_STATUS_ERROR;
    }

    ADC0->SC3 = 0U;
    return ADC_STATUS_OK;
}

uint32_t ADC_SetChannel(uint8_t channel)
{
    if (channel >= ADC_CHANNEL_DISABLED)
    {
        return ADC_STATUS_ERROR;
    }

    if ((g_adcMode == ADC_MODE_HW_TRIGGER) && (channel != ADC_CHANNEL_12))
    {
        return ADC_STATUS_ERROR;
    }

    g_adcInfo.channel = channel;
    adc_channel_index = channel;

    if (g_adcMode == ADC_MODE_HW_TRIGGER)
    {
        ADC0->SC1[0] = ADC_SC1_ADCH(ADC_CHANNEL_12) | ADC_SC1_AIEN_MASK;
    }

    return ADC_STATUS_OK;
}

uint32_t ADC_StartConversion(void)
{
    if (g_adcMode != ADC_MODE_SW_TRIGGER)
    {
        return ADC_STATUS_ERROR;
    }

    if (g_adcInfo.channel >= ADC_CHANNEL_DISABLED)
    {
        return ADC_STATUS_ERROR;
    }

    ADC0->SC1[0] = ADC_SC1_ADCH(g_adcInfo.channel);
    return ADC_STATUS_OK;
}

uint16_t ADC_GetResult(void)
{
    return (uint16_t)(ADC0->R[0] & 0x0FFFU);
}

float ADC_GetVoltage(uint16_t adcValue)
{
    adcValue &= 0x0FFFU;
    return ((float)adcValue / (float)ADC_MAX_VALUE) * ADC_REF_VOLTAGE;
}

uint32_t ADC_IsConversionComplete(void)
{
    return ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) != 0U) ? 1U : 0U;
}

uint8_t ADC_GetChannelIndex(void)
{
    return adc_channel_index;
}

void ADC_Deinit(void)
{
    ADC0->SC1[0] = ADC_SC1_ADCH(ADC_CHANNEL_DISABLED);
    NVIC_DisableIRQ(ADC0_IRQn);
}

void ADC0_IRQHandler(void)
{
    /* Handled for Mode 1 Hardware Trigger */
    uint16_t adcValue = ADC_GetResult();
    float voltage = ADC_GetVoltage(adcValue);

    /* Store converted voltage to RAM Buffer */
    g_adcVoltageBuffer[g_adcBufferIdx] = voltage;
    g_adcBufferIdx = (g_adcBufferIdx + 1U) % ADC_RAM_BUFFER_SIZE;

    /* Tracking information and cumulative average */
    g_adcInfo.readCount++;
    g_adcInfo.lastValue = adcValue;
    g_adcInfo.lastVoltage = voltage;

    if (g_adcInfo.readCount == 1U)
    {
        g_adcInfo.averageValue = adcValue;
    }
    else
    {
        g_adcInfo.averageValue = ((g_adcInfo.averageValue * (g_adcInfo.readCount - 1U)) + adcValue) / g_adcInfo.readCount;
    }

    g_adcInfo.averageVoltage = ADC_GetVoltage((uint16_t)g_adcInfo.averageValue);

    /* Update RTC Timestamp via temporary struct to avoid qualifier warning */
    RTC_Time_t cur_time;
    RTC_GetTime(&cur_time);
    g_adcInfo.timestamp.hour   = cur_time.hour;
    g_adcInfo.timestamp.minute = cur_time.minute;
    g_adcInfo.timestamp.second = cur_time.second;
}
