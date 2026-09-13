#ifndef DRIVER_UART_H_
#define DRIVER_UART_H_

#include <stdint.h>
#include <stdbool.h>
#include "../common/Driver_Common.h"

/* Return status codes for UART operations */
typedef enum
{
    UART_STATUS_OK              = ARM_DRIVER_OK,            /* Operation succeeded */
    UART_STATUS_ERROR           = ARM_DRIVER_ERROR,         /* Unspecified general error */
    UART_STATUS_BUSY            = ARM_DRIVER_ERROR_BUSY,    /* Peripheral is currently busy */
    UART_STATUS_TIMEOUT         = ARM_DRIVER_ERROR_TIMEOUT, /* Operation timed out */
    UART_STATUS_INVALID_PARAM   = ARM_DRIVER_ERROR_PARAMETER/* Invalid function parameter */
} UART_Status_t;

/* Callback when a byte is received via RX interrupt */
typedef void (*UART_RxCallback_t)(uint8_t rxData);

/* Callback when TX register is empty and ready for next byte */
typedef bool (*UART_TxCallback_t)(uint8_t *txData);

/* Initialize LPUART1 with the requested baud rate, 8 data bits, no parity, 1 stop bit */
UART_Status_t LPUART1_Init(uint32_t baudRate);

/* Deinitialize LPUART1 peripheral and disable clock */
void LPUART1_Deinit(void);

/* Transmit one character using polling blocking mode */
UART_Status_t LPUART1_SendChar_Blocking(char data);

/* Transmit a null-terminated string using polling blocking mode */
UART_Status_t LPUART1_SendString_Blocking(const char *str);

/* Register callback functions for interrupt handling */
void LPUART1_RegisterCallbacks(UART_RxCallback_t rxCb, UART_TxCallback_t txCb);

/* Enable transmitter interrupt to start sending data */
void LPUART1_EnableTxInterrupt(void);

/* Get total number of received bytes */
uint32_t LPUART1_GetRxCount(void);

/* Get total number of transmitted bytes */
uint32_t LPUART1_GetTxCount(void);

/* Get requested and hardware-achievable baud rates */
uint32_t LPUART1_GetConfiguredBaudRate(void);
uint32_t LPUART1_GetActualBaudRate(void);

/* Get total number of UART framing, parity, noise, and overrun errors */
uint32_t LPUART1_GetErrorCount(void);

/* Reset both transmission and reception byte counters */
void LPUART1_ResetStats(void);

#endif /* DRIVER_UART_H_ */
