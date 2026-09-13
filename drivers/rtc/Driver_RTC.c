#include "Driver_RTC.h"
#include "../nvic/Driver_NVIC.h"
#include "S32K144.h"

/* Time conversion definitions. */
#define RTC_SECONDS_PER_MINUTE          (60UL)
#define RTC_SECONDS_PER_HOUR            (3600UL)
#define RTC_SECONDS_PER_DAY             (86400UL)

/* RTC clock and interrupt priority definitions */
#define RTC_CLOCK_SELECT_LPO            (1U)
#define RTC_MAIN_IRQ_PRIORITY           (3U)
#define RTC_SECONDS_IRQ_PRIORITY        (4U)

/* Prototypes */
static void RTC_ConfigClock(void);
static void RTC_ResetInformation(uint32_t initialSeconds);

/* Variables */
volatile RTC_INFOR_t g_rtcInfo;
static RTC_Callback_t rtcSecondsCallback = (RTC_Callback_t)0;
static RTC_Callback_t rtcAlarmCallback = (RTC_Callback_t)0;
static RTC_Callback_t rtcOverflowCallback = (RTC_Callback_t)0;

static void RTC_ConfigClock(void)
{
    /* Read current LPO configuration to preserve unrelated fields */
    uint32_t lpoClockConfig;

    /* Clear existing RTC clock selection */
    lpoClockConfig = SIM->LPOCLKS;

    /* Enable LPO 1kHz and route LPO to RTC */
    lpoClockConfig &= ~SIM_LPOCLKS_RTCCLKSEL_MASK;

    /* Enable LPO 1kHz and route LPO to RTC */
    lpoClockConfig |= SIM_LPOCLKS_LPO1KCLKEN_MASK | SIM_LPOCLKS_RTCCLKSEL(RTC_CLOCK_SELECT_LPO);
    SIM->LPOCLKS = lpoClockConfig;

    /* Enable bus interface clock for RTC */
    PCC->PCCn[PCC_RTC_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Stop RTC before configuring control register */
    RTC->SR &= ~RTC_SR_TCE_MASK;

    /* Select 1kHz LPO input for RTC prescaler */
    RTC->CR |= RTC_CR_LPOS_MASK;
}

static void RTC_ResetInformation(uint32_t initialSeconds)
{
    g_rtcInfo.currentSeconds = initialSeconds;
    g_rtcInfo.alarmSeconds = 0U;
    g_rtcInfo.secondsInterruptCount = 0U;
    g_rtcInfo.alarmInterruptCount = 0U;
    g_rtcInfo.overflowInterruptCount = 0U;

    g_rtcInfo.alarmOccurred = RTC_ALARM_NOT_OCCURRED;
}

uint32_t RTC_Init(uint32_t initialSeconds)
{
    /* Avoid initializing seconds counter to 0 */
    if (initialSeconds == 0U)
    {
        initialSeconds = 1U;
    }

    /* Configure clock and reset state registers */
    RTC_ConfigClock();
    RTC->SR &= ~RTC_SR_TCE_MASK;
    RTC->IER = 0U;
    RTC->TPR = 0U;
    RTC->TAR = 0U;
    RTC->TSR = initialSeconds;

    /* Reset software context tracking */
    RTC_ResetInformation(initialSeconds);

    /* Configure NVIC for RTC seconds interrupt */
    NVIC_DisableIRQ(RTC_Seconds_IRQn);
    NVIC_ClearPendingIRQ(RTC_Seconds_IRQn);
    NVIC_SetPriority(RTC_Seconds_IRQn, RTC_SECONDS_IRQ_PRIORITY);

    /* Configure NVIC for RTC alarm interrupt */
    NVIC_DisableIRQ(RTC_IRQn);
    NVIC_ClearPendingIRQ(RTC_IRQn);
    NVIC_SetPriority(RTC_IRQn, RTC_MAIN_IRQ_PRIORITY);

    return RTC_STATUS_OK;
}

void RTC_DeInit(void)
{
    /* Stop counter and clear interrupts */
    RTC_Stop();
    RTC->IER = 0U;
    NVIC_DisableIRQ(RTC_Seconds_IRQn);
    NVIC_ClearPendingIRQ(RTC_Seconds_IRQn);
    NVIC_DisableIRQ(RTC_IRQn);
    NVIC_ClearPendingIRQ(RTC_IRQn);

    /* Clear hardware registers and software tracking */
    RTC->TAR = 0U;
    RTC->TPR = 0U;
    RTC_ResetInformation(0U);

    /* Reset registered user callbacks */
    rtcSecondsCallback = (RTC_Callback_t)0;
    rtcAlarmCallback = (RTC_Callback_t)0;
    rtcOverflowCallback = (RTC_Callback_t)0;

    /* Disable RTC peripheral clock gate */
    PCC->PCCn[PCC_RTC_INDEX] &= ~PCC_PCCn_CGC_MASK;
}

void RTC_Start(void)
{
    /* Enable time counter */
    RTC->SR |= RTC_SR_TCE_MASK;
}

void RTC_Stop(void)
{
    /* Disable time counter */
    RTC->SR &= ~RTC_SR_TCE_MASK;
}

bool RTC_IsRunning(void)
{
    /* Check if time counter is active */
    return ((RTC->SR & RTC_SR_TCE_MASK) != 0U);
}

uint32_t RTC_SetSeconds(uint32_t seconds)
{
    if (seconds == 0U)
    { 
        seconds = 1U; 
    }
    bool wasRunning = RTC_IsRunning();

    /* Disable counter, reset prescaler, and set new time */
    RTC_Stop();
    RTC->TPR = 0U;
    RTC->TSR = seconds;
    g_rtcInfo.currentSeconds = seconds;

    /* Restore counter run state */
    if (wasRunning) 
    { 
        RTC_Start(); 
    }

    return RTC_STATUS_OK;
}

uint32_t RTC_GetSeconds(void)
{
    uint32_t seconds;
    seconds = RTC->TSR;
    g_rtcInfo.currentSeconds = seconds;

    return seconds;
}

void RTC_ResetSeconds(void)
{
    /* Reset Timer */
    (void)RTC_SetSeconds(1U);
}

void RTC_GetTime(RTC_Time_t *time)
{
    if (time == (RTC_Time_t *)0)
    {
        return;
    }

    /* Decompose total seconds into hours, minutes, and seconds */
    uint32_t totalSeconds = RTC_GetSeconds();
    uint32_t secondsInDay = totalSeconds % RTC_SECONDS_PER_DAY;

    time->hour = (uint8_t)(secondsInDay / RTC_SECONDS_PER_HOUR);
    time->minute = (uint8_t)((secondsInDay % RTC_SECONDS_PER_HOUR) / RTC_SECONDS_PER_MINUTE);
    time->second = (uint8_t)(secondsInDay % RTC_SECONDS_PER_MINUTE);
}

uint32_t RTC_SetAlarmAbsolute(uint32_t alarmSeconds)
{
    uint32_t currentSeconds = RTC_GetSeconds();

    if (alarmSeconds <= currentSeconds)
    {
        return RTC_STATUS_INVALID_PARAMETER;
    }

    /* Set target alarm match time */
    g_rtcInfo.alarmOccurred = RTC_ALARM_NOT_OCCURRED;
    RTC->TAR = alarmSeconds;
    g_rtcInfo.alarmSeconds = alarmSeconds;

    return RTC_STATUS_OK;
}

uint32_t RTC_SetAlarmAfter(uint32_t delaySeconds)
{
    if (delaySeconds == 0U)
    {
        return RTC_STATUS_INVALID_PARAMETER;
    }

    uint32_t currentSeconds = RTC_GetSeconds();

    /* Check addition overflow before configuring alarm */
    if (currentSeconds > (UINT32_MAX - delaySeconds)) 
    { 
        return RTC_STATUS_ERROR; 
    }

    return RTC_SetAlarmAbsolute(currentSeconds + delaySeconds);
}

void RTC_DisableAlarm(void)
{
    /* Clear alarm enable bit and reset target register */
    RTC->IER &= ~RTC_IER_TAIE_MASK;
    RTC->TAR = 0U;
    g_rtcInfo.alarmSeconds = 0U;
    NVIC_ClearPendingIRQ(RTC_IRQn);
}

uint32_t RTC_GetAlarmSeconds(void)
{
    return g_rtcInfo.alarmSeconds;
}

uint8_t RTC_HasAlarmOccurred(void)
{
    return g_rtcInfo.alarmOccurred;
}

void RTC_ClearAlarmStatus(void)
{
    /* Reset software alarm occurrence flag */
    g_rtcInfo.alarmOccurred = RTC_ALARM_NOT_OCCURRED;
}

void RTC_EnableSecondsInterrupt(bool enable)
{
    if (enable)
    {
        /* Enable seconds interrupt in peripheral and NVIC */
        RTC->IER = (RTC->IER & ~(RTC_IER_TSIC_MASK | RTC_IER_TSIE_MASK)) | RTC_IER_TSIC(0U) | RTC_IER_TSIE_MASK;
        NVIC_ClearPendingIRQ(RTC_Seconds_IRQn);
        NVIC_EnableIRQ(RTC_Seconds_IRQn);
    }
    else
    {
        /* Disable seconds interrupt in peripheral and NVIC */
        RTC->IER &= ~RTC_IER_TSIE_MASK;
        NVIC_DisableIRQ(RTC_Seconds_IRQn);
        NVIC_ClearPendingIRQ(RTC_Seconds_IRQn);
    }
}

void RTC_EnableAlarmInterrupt(bool enable)
{
    if (enable)
    {
        /* Enable alarm interrupt in peripheral and NVIC */
        RTC->IER |= RTC_IER_TAIE_MASK;
        NVIC_ClearPendingIRQ(RTC_IRQn);
        NVIC_EnableIRQ(RTC_IRQn);
    }
    else
    {
        /* Disable alarm interrupt in peripheral */
        RTC->IER &= ~RTC_IER_TAIE_MASK;
        NVIC_ClearPendingIRQ(RTC_IRQn);
    }
}

void RTC_EnableOverflowInterrupt(bool enable)
{
    if (enable)
    {
        /* Enable overflow interrupt in peripheral and NVIC */
        RTC->IER |= RTC_IER_TOIE_MASK;
        NVIC_ClearPendingIRQ(RTC_IRQn);
        NVIC_EnableIRQ(RTC_IRQn);
    }
    else
    {
        /* Disable overflow interrupt in peripheral */
        RTC->IER &= ~RTC_IER_TOIE_MASK;
        if ((RTC->IER & RTC_IER_TAIE_MASK) == 0U) { NVIC_ClearPendingIRQ(RTC_IRQn); }
    }
}

void RTC_RegisterSecondsCallback(RTC_Callback_t callback)
{
    /* Assign user callback for 1-second ticks */
    rtcSecondsCallback = callback;
}

void RTC_RegisterAlarmCallback(RTC_Callback_t callback)
{
    /* Assign user callback for alarm match events */
    rtcAlarmCallback = callback;
}

void RTC_RegisterOverflowCallback(RTC_Callback_t callback)
{
    /* Assign user callback for counter overflow */
    rtcOverflowCallback = callback;
}

void RTC_Seconds_IRQHandler(void)
{
    /* Exit immediately if seconds interrupt was disabled by alarm */
    if ((RTC->IER & RTC_IER_TSIE_MASK) == 0U) 
    { 
        return; 
    }

    /* Update runtime seconds and invoke callback */
    g_rtcInfo.currentSeconds = RTC->TSR;
    g_rtcInfo.secondsInterruptCount++;
    if (rtcSecondsCallback != (RTC_Callback_t)0) 
    { 
        rtcSecondsCallback(); 
    }
}

void RTC_IRQHandler(void)
{
    uint32_t rtcStatus = RTC->SR;

    /* Process alarm match event */
    if ((rtcStatus & RTC_SR_TAF_MASK) != 0U)
    {
        /* Save alarm execution info */
        g_rtcInfo.currentSeconds = RTC->TSR;
        g_rtcInfo.alarmSeconds = RTC->TAR;
        g_rtcInfo.alarmInterruptCount++;
        g_rtcInfo.alarmOccurred = RTC_ALARM_OCCURRED;

        /* Disable RTC interrupts and stop counter */
        RTC->IER = 0U;
        NVIC_DisableIRQ(RTC_Seconds_IRQn);
        NVIC_ClearPendingIRQ(RTC_Seconds_IRQn);
        RTC->TAR = 0U;
        RTC_Stop();

        /* Execute alarm callback */
        if (rtcAlarmCallback != (RTC_Callback_t)0) { rtcAlarmCallback(); }
        return;
    }

    /* Process counter overflow event */
    if ((rtcStatus & RTC_SR_TOF_MASK) != 0U)
    {
        g_rtcInfo.overflowInterruptCount++;
        RTC->IER &= ~RTC_IER_TOIE_MASK;
        if (rtcOverflowCallback != (RTC_Callback_t)0) { rtcOverflowCallback(); }
    }
}
