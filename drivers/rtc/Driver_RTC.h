#ifndef DRIVER_RTC_H
#define DRIVER_RTC_H

#include <stdint.h>
#include <stdbool.h>
#include "S32K144.h"

#define RTC_STATUS_OK                  (0U)
#define RTC_STATUS_ERROR               (1U)
#define RTC_STATUS_INVALID_PARAMETER   (2U)

#define RTC_ALARM_NOT_OCCURRED         (0U)
#define RTC_ALARM_OCCURRED             (1U)

typedef void (*RTC_Callback_t)(void);

typedef struct
{
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} RTC_Time_t;

typedef struct
{
    uint32_t currentSeconds;
    uint32_t alarmSeconds;

    uint32_t secondsInterruptCount;
    uint32_t alarmInterruptCount;
    uint32_t overflowInterruptCount;

    uint8_t alarmOccurred;
} RTC_INFOR_t;

extern volatile RTC_INFOR_t g_rtcInfo;

uint32_t RTC_Init(uint32_t initialSeconds);
void RTC_DeInit(void);

void RTC_Start(void);
void RTC_Stop(void);
bool RTC_IsRunning(void);

uint32_t RTC_SetSeconds(uint32_t seconds);
uint32_t RTC_GetSeconds(void);
void RTC_ResetSeconds(void);

void RTC_GetTime(RTC_Time_t *time);

uint32_t RTC_SetAlarmAbsolute(uint32_t alarmSeconds);
uint32_t RTC_SetAlarmAfter(uint32_t delaySeconds);

void RTC_DisableAlarm(void);

uint32_t RTC_GetAlarmSeconds(void);
uint8_t RTC_HasAlarmOccurred(void);
void RTC_ClearAlarmStatus(void);

void RTC_EnableSecondsInterrupt(bool enable);
void RTC_EnableAlarmInterrupt(bool enable);
void RTC_EnableOverflowInterrupt(bool enable);

void RTC_RegisterSecondsCallback(RTC_Callback_t callback);
void RTC_RegisterAlarmCallback(RTC_Callback_t callback);
void RTC_RegisterOverflowCallback(RTC_Callback_t callback);

void RTC_Seconds_IRQHandler(void);
void RTC_IRQHandler(void);

#endif /* DRIVER_RTC_H */
