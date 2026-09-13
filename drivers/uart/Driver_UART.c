#include "S32K144.h"
#include "../uart/Driver_UART.h"
#include "../nvic/Driver_NVIC.h"
#include "S32K144_features.h"

#define FIRC_TIMEOUT_CNT            (100000U)
#define UART_POLL_TIMEOUT_CNT       (500000U)
#define UART_BAUD_OSR_MIN           (4U)
#define UART_BAUD_OSR_MAX           (32U)
#define UART_BAUD_BOTHEDGE_MAX      (7U)
#define UART_BAUD_SBR_MAX           (8191U)
#define UART_BAUD_MAX_ERROR_PPM     (30000UL)
#define UART_STAT_W1C_MASK          (LPUART_STAT_LBKDIF_MASK  | \
                                     LPUART_STAT_RXEDGIF_MASK | \
                                     LPUART_STAT_IDLE_MASK    | \
                                     LPUART_STAT_OR_MASK      | \
                                     LPUART_STAT_NF_MASK      | \
                                     LPUART_STAT_FE_MASK      | \
                                     LPUART_STAT_PF_MASK      | \
                                     LPUART_STAT_MA1F_MASK    | \
                                     LPUART_STAT_MA2F_MASK)

typedef struct
{
    uint8_t oversamplingRatio;
    uint16_t baudRateDivisor;
    uint32_t actualBaudRate;
    uint32_t errorPpm;
} UART_BaudConfig_t;

/* Find the valid OSR/SBR pair with the smallest baud-rate error. */
static bool LPUART1_CalculateBaud(uint32_t sourceClockHz,
                                 uint32_t requestedBaudRate,
                                 UART_BaudConfig_t *config)
{
    UART_BaudConfig_t bestConfig = {0U, 0U, 0U, UINT32_MAX};
    uint32_t oversamplingRatio;

    if ((sourceClockHz == 0U) || (requestedBaudRate == 0U) ||
        (config == NULL))
    {
        return false;
    }

    for (oversamplingRatio = UART_BAUD_OSR_MIN;
         oversamplingRatio <= UART_BAUD_OSR_MAX;
         oversamplingRatio++)
    {
        uint64_t requestedDivider =
            (uint64_t)requestedBaudRate * oversamplingRatio;
        uint64_t baudRateDivisor =
            ((uint64_t)sourceClockHz + (requestedDivider / 2U)) /
            requestedDivider;
        uint64_t totalDivider;
        uint64_t targetClock;
        uint64_t clockDifference;
        uint32_t errorPpm;

        if ((baudRateDivisor == 0U) ||
            (baudRateDivisor > UART_BAUD_SBR_MAX))
        {
            continue;
        }

        totalDivider = (uint64_t)oversamplingRatio * baudRateDivisor;
        targetClock = (uint64_t)requestedBaudRate * totalDivider;
        clockDifference = ((uint64_t)sourceClockHz >= targetClock)
            ? ((uint64_t)sourceClockHz - targetClock)
            : (targetClock - (uint64_t)sourceClockHz);
        errorPpm = (uint32_t)(((clockDifference * 1000000ULL) +
                               (targetClock / 2U)) / targetClock);

        /* For equal error, prefer the higher OSR for better noise tolerance. */
        if (errorPpm <= bestConfig.errorPpm)
        {
            bestConfig.oversamplingRatio = (uint8_t)oversamplingRatio;
            bestConfig.baudRateDivisor = (uint16_t)baudRateDivisor;
            bestConfig.actualBaudRate =
                (uint32_t)(((uint64_t)sourceClockHz + (totalDivider / 2U)) /
                           totalDivider);
            bestConfig.errorPpm = errorPpm;
        }
    }

    if ((bestConfig.oversamplingRatio == 0U) ||
        (bestConfig.errorPpm > UART_BAUD_MAX_ERROR_PPM))
    {
        return false;
    }

    *config = bestConfig;
    return true;
}

/* Calculate and apply the closest valid baud configuration. */
static UART_Status_t LPUART1_SetBaudRate(uint32_t requestedBaudRate,
                                        uint32_t *actualBaudRate)
{
    UART_BaudConfig_t config;
    uint32_t baudRegister;

    if ((actualBaudRate == NULL) ||
        !LPUART1_CalculateBaud(FEATURE_SCG_FIRC_FREQ0,
                              requestedBaudRate,
                              &config))
    {
        return UART_STATUS_INVALID_PARAM;
    }

    baudRegister = LPUART1->BAUD;
    baudRegister &= ~(LPUART_BAUD_OSR_MASK | LPUART_BAUD_SBR_MASK |
                      LPUART_BAUD_BOTHEDGE_MASK | LPUART_BAUD_SBNS_MASK |
                      LPUART_BAUD_M10_MASK);
    baudRegister |=
        LPUART_BAUD_OSR((uint32_t)config.oversamplingRatio - 1U) |
        LPUART_BAUD_SBR(config.baudRateDivisor);

    if (config.oversamplingRatio <= UART_BAUD_BOTHEDGE_MAX)
    {
        baudRegister |= LPUART_BAUD_BOTHEDGE_MASK;
    }

    LPUART1->BAUD = baudRegister;
    *actualBaudRate = config.actualBaudRate;
    return UART_STATUS_OK;
}

static UART_RxCallback_t s_rxCallback = NULL;
static UART_TxCallback_t s_txCallback = NULL;
static volatile uint32_t s_rxByteCount = 0U;
static volatile uint32_t s_txByteCount = 0U;
static volatile uint32_t s_errorCount = 0U;
static uint32_t s_configuredBaudRate = 0U;
static uint32_t s_actualBaudRate = 0U;

/* Initialize LPUART1 module with a caller-selected baud rate and 8N1 framing */
UART_Status_t LPUART1_Init(uint32_t baudRate)
{
    uint32_t timeout = FIRC_TIMEOUT_CNT;
    uint32_t actualBaudRate;
    UART_Status_t status;

    /* Stop any interrupt left active by an earlier debug/run session. */
    NVIC_DisableIRQ(LPUART1_RxTx_IRQn);

    /* Enable FIRC if disabled */
    if ((SCG->FIRCCSR & SCG_FIRCCSR_FIRCEN_MASK) == 0U)
    {
        SCG->FIRCCSR |= SCG_FIRCCSR_FIRCEN_MASK;
    }

    /* Wait for FIRC clock to stabilize */
    while (((SCG->FIRCCSR & SCG_FIRCCSR_FIRCVLD_MASK) == 0U) && (timeout > 0U))
    {
        timeout--;
    }

    /* Return timeout if clock is unstable */
    if (timeout == 0U)
    {
        return UART_STATUS_TIMEOUT;
    }

    /* Enable clock for PORTC */
    PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Route PTC6 pin to LPUART1_RX */
    PORTC->PCR[6U] = (PORTC->PCR[6U] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(2U);

    /* Route PTC7 pin to LPUART1_TX */
    PORTC->PCR[7U] = (PORTC->PCR[7U] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(2U);

    /* Configure FIRC divider 2 with ratio 1 */
    SCG->FIRCDIV = (SCG->FIRCDIV & ~SCG_FIRCDIV_FIRCDIV2_MASK) |
                   SCG_FIRCDIV_FIRCDIV2(1U);

    /* Disable LPUART1 clock before selecting source */
    PCC->PCCn[PCC_LPUART1_INDEX] &= ~PCC_PCCn_CGC_MASK;

    /* Select FIRC as LPUART1 clock source */
    PCC->PCCn[PCC_LPUART1_INDEX] = (PCC->PCCn[PCC_LPUART1_INDEX] & ~PCC_PCCn_PCS_MASK) | PCC_PCCn_PCS(3U);

    /* Enable clock for LPUART1 */
    PCC->PCCn[PCC_LPUART1_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Reset all prior TX/RX, DMA, break, FIFO, and interrupt state. */
    LPUART1->GLOBAL |= LPUART_GLOBAL_RST_MASK;
    LPUART1->GLOBAL &= ~LPUART_GLOBAL_RST_MASK;
    LPUART1->CTRL = 0U;
    LPUART1->FIFO = LPUART_FIFO_TXFLUSH_MASK | LPUART_FIFO_RXFLUSH_MASK;
    LPUART1->WATER = 0U;
    LPUART1->STAT = UART_STAT_W1C_MASK;
    LPUART1->FIFO |= LPUART_FIFO_TXOF_MASK | LPUART_FIFO_RXUF_MASK;

    status = LPUART1_SetBaudRate(baudRate, &actualBaudRate);
    if (status != UART_STATUS_OK)
    {
        PCC->PCCn[PCC_LPUART1_INDEX] &= ~PCC_PCCn_CGC_MASK;
        return status;
    }

    /* Reset defaults plus RIE give 8 data bits, no parity, and one stop bit. */
    LPUART1->CTRL = LPUART_CTRL_RIE_MASK;

    /* Set interrupt priority and enable IRQ in NVIC */
    NVIC_ClearPendingIRQ(LPUART1_RxTx_IRQn);
    NVIC_SetPriority(LPUART1_RxTx_IRQn, 8U);
    NVIC_EnableIRQ(LPUART1_RxTx_IRQn);

    /* Enable transmitter and receiver */
    LPUART1->CTRL |= (LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);

    s_configuredBaudRate = baudRate;
    s_actualBaudRate = actualBaudRate;

    /* Return success */
    return UART_STATUS_OK;
}

/* Deinitialize LPUART1 peripheral and disable its interrupt and clock */
void LPUART1_Deinit(void)
{
    NVIC_DisableIRQ(LPUART1_RxTx_IRQn);
    NVIC_ClearPendingIRQ(LPUART1_RxTx_IRQn);
    LPUART1->CTRL = 0U;
    LPUART1->GLOBAL |= LPUART_GLOBAL_RST_MASK;
    LPUART1->GLOBAL &= ~LPUART_GLOBAL_RST_MASK;
    PCC->PCCn[PCC_LPUART1_INDEX] &= ~PCC_PCCn_CGC_MASK;
    s_rxCallback = NULL;
    s_txCallback = NULL;
    s_configuredBaudRate = 0U;
    s_actualBaudRate = 0U;
}

/* Transmit one character using polling blocking mode */
UART_Status_t LPUART1_SendChar_Blocking(char data)
{
    uint32_t timeout = UART_POLL_TIMEOUT_CNT;

    /* Wait until transmit data register is empty */
    while (((LPUART1->STAT & LPUART_STAT_TDRE_MASK) == 0U) && (timeout > 0U))
    {
        timeout--;
    }

    /* Return timeout error if register did not clear */
    if (timeout == 0U)
    {
        return UART_STATUS_TIMEOUT;
    }

    /* Write character to data register */
    LPUART1->DATA = (uint32_t)data;
    s_txByteCount++;

    /* Return success */
    return UART_STATUS_OK;
}

/* Transmit a null-terminated string using polling blocking mode */
UART_Status_t LPUART1_SendString_Blocking(const char *str)
{
    UART_Status_t status = UART_STATUS_OK;

    /* Validate input pointer */
    if (str == NULL)
    {
        return UART_STATUS_INVALID_PARAM;
    }

    /* Transmit each character sequentially */
    while (*str != '\0')
    {
        status = LPUART1_SendChar_Blocking(*str);

        /* Abort if transmission fails */
        if (status != UART_STATUS_OK)
        {
            return status;
        }

        str++;
    }

    /* Return success */
    return UART_STATUS_OK;
}

/* Register callback functions for interrupt handling */
void LPUART1_RegisterCallbacks(UART_RxCallback_t rxCb, UART_TxCallback_t txCb)
{
    s_rxCallback = rxCb;
    s_txCallback = txCb;
}

/* Enable transmitter interrupt to start sending data */
void LPUART1_EnableTxInterrupt(void)
{
    LPUART1->CTRL |= LPUART_CTRL_TIE_MASK;
}

/* Disable transmitter interrupt when buffer is empty */
static void LPUART1_DisableTxInterrupt(void)
{
    LPUART1->CTRL &= ~LPUART_CTRL_TIE_MASK;
}

/* Get total number of received bytes */
uint32_t LPUART1_GetRxCount(void)
{
    return s_rxByteCount;
}

/* Get total number of transmitted bytes */
uint32_t LPUART1_GetTxCount(void)
{
    return s_txByteCount;
}

uint32_t LPUART1_GetConfiguredBaudRate(void)
{
    return s_configuredBaudRate;
}

uint32_t LPUART1_GetActualBaudRate(void)
{
    return s_actualBaudRate;
}

/* Get total number of detected receive errors */
uint32_t LPUART1_GetErrorCount(void)
{
    return s_errorCount;
}

/* Reset both transmission and reception byte counters */
void LPUART1_ResetStats(void)
{
    s_rxByteCount = 0U;
    s_txByteCount = 0U;
    s_errorCount = 0U;
}

/* LPUART1 interrupt service routine for RX and TX events */
void LPUART1_RxTx_IRQHandler(void)
{
    uint32_t status = LPUART1->STAT;
    uint32_t errorFlags = status & (LPUART_STAT_OR_MASK | LPUART_STAT_NF_MASK |
                                    LPUART_STAT_FE_MASK | LPUART_STAT_PF_MASK);

    if (errorFlags != 0U)
    {
        s_errorCount++;
        LPUART1->STAT = errorFlags;
    }

    /* Handle RX data register full interrupt */
    if (((LPUART1->CTRL & LPUART_CTRL_RIE_MASK) != 0U) &&
        ((status & LPUART_STAT_RDRF_MASK) != 0U))
    {
        uint8_t val = (uint8_t)(LPUART1->DATA & 0xFFU);
        s_rxByteCount++;
        if (s_rxCallback != NULL)
        {
            s_rxCallback(val);
        }
    }

    /* Handle TX data register empty interrupt */
    if (((LPUART1->CTRL & LPUART_CTRL_TIE_MASK) != 0U) && ((LPUART1->STAT & LPUART_STAT_TDRE_MASK) != 0U))
    {
        uint8_t txByte = 0U;
        if (s_txCallback != NULL)
        {
            bool hasData = s_txCallback(&txByte);
            if (hasData)
            {
                LPUART1->DATA = (uint32_t)txByte;
                s_txByteCount++;
            }
            else
            {
                LPUART1_DisableTxInterrupt();
            }
        }
        else
        {
            LPUART1_DisableTxInterrupt();
        }
    }
}
