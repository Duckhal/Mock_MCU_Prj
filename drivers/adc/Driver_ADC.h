#ifndef DRIVER_ADC_H_
#define DRIVER_ADC_H_

#include <stdint.h>
#include "S32K144.h"
#include "../rtc/Driver_RTC.h"

/* ADC operating modes */
#define ADC_MODE_HW_TRIGGER          (1U)
#define ADC_MODE_SW_TRIGGER          (2U)

/* ADC channels */
#define ADC_CHANNEL_12               (12U)
#define ADC_CHANNEL_DISABLED         (31U)

/* ADC comparison values */
#define ADC_COMPARE_LOW              (300U)
#define ADC_COMPARE_HIGH             (4000U)

/*
 * ADC < 2048  : Red LED
 * ADC >= 2048 : Green LED
 */
#define ADC_TEST_THRESHOLD           (2048U)

/* ADC conversion constants */
#define ADC_MAX_VALUE                (4095U)
#define ADC_REF_VOLTAGE              (3.3f)

/* ADC return status */
#define ADC_STATUS_OK                (0U)
#define ADC_STATUS_ERROR             (1U)

/* ADC Buffer */
#define ADC_RAM_BUFFER_SIZE          (64U)

/* ADC information structure */
typedef struct
{
    uint32_t          readCount;
    uint16_t          lastValue;
    float             lastVoltage;
    uint32_t          averageValue;
    float             averageVoltage;
    uint8_t           channel;
    RTC_Time_t        timestamp;      /* RTC Time: Hour, Minute, Second */
    uint8_t           compareResult;
} ADC_INFOR_t;

uint32_t ADC_Init(uint8_t adcMode);
uint32_t ADC_Calibrate(void);
uint32_t ADC_SetChannel(uint8_t channel);
uint32_t ADC_StartConversion(void);
uint16_t ADC_GetResult(void);
float ADC_GetVoltage(uint16_t adcValue);
uint32_t ADC_IsConversionComplete(void);
uint8_t ADC_GetChannelIndex(void);
void ADC_Deinit(void);

extern volatile ADC_INFOR_t g_adcInfo;
extern volatile float       g_adcVoltageBuffer[ADC_RAM_BUFFER_SIZE];
extern volatile uint32_t    g_adcBufferIdx;

#endif /* DRIVER_ADC_H_ */
