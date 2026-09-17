#include <assert.h>
#include <stdio.h>
#include <string.h>

#define BOARD_MODE 2
#define BOARD_DEMO_UNIT_TEST
#include "../../src/main.c"

static unsigned ledOn[3];
static unsigned ledOff[3];
static char lastUart[64];

static unsigned Test_LedIndex(ARM_GPIO_Pin_t pin)
{
    if (pin == LED_BLUE) { return 0U; }
    if (pin == LED_RED) { return 1U; }
    assert(pin == LED_GREEN);
    return 2U;
}

void LED_On(ARM_GPIO_Pin_t pin) { ledOn[Test_LedIndex(pin)]++; }
void LED_Off(ARM_GPIO_Pin_t pin) { ledOff[Test_LedIndex(pin)]++; }

UART_Status_t LPUART1_SendString_Blocking(const char *line)
{
    size_t length = strlen(line);
    assert(length < sizeof(lastUart));
    memcpy(lastUart, line, length + 1U);
    return UART_STATUS_OK;
}

int main(void)
{
    uint8_t seen = 0U;
    uint8_t frame[3] = {0xCAU, 1U, 5U};
    assert(BoardDemo_DecodeFrame(NULL, 3U) == 0U);
    BoardDemo_ProcessRx(CANIF_RX_PDU_VEHICLE_STATUS, 2U, NULL, &seen);
    assert(g_BoardDemoResult.rxInvalid == 1U && seen == 0U);
    BoardDemo_ProcessRx(CANIF_RX_PDU_VEHICLE_STATUS, 3U, frame, &seen);
    assert(g_BoardDemoResult.rxFrames == 1U && ledOn[0] == 1U);
    assert(g_BoardDemoResult.lastCommand == 1U && g_BoardDemoResult.lastSequence == 5U);
    BoardDemo_ProcessRx(CANIF_RX_PDU_VEHICLE_STATUS, 3U, frame, &seen);
    assert(g_BoardDemoResult.rxDuplicates == 1U && ledOn[0] == 1U);
    frame[1] = 2U; frame[2] = 6U;
    BoardDemo_ProcessRx(CANIF_RX_PDU_VEHICLE_STATUS, 3U, frame, &seen);
    assert(g_BoardDemoResult.rxFrames == 2U && ledOn[1] == 1U);
    frame[1] = 3U; frame[2] = 7U;
    BoardDemo_ProcessRx(CANIF_RX_PDU_VEHICLE_STATUS, 3U, frame, &seen);
    assert(g_BoardDemoResult.rxFrames == 3U && ledOn[2] == 1U);
    frame[1] = 0U; frame[2] = 8U;
    BoardDemo_ProcessRx(CANIF_RX_PDU_VEHICLE_STATUS, 3U, frame, &seen);
    assert(g_BoardDemoResult.rxFrames == 4U && ledOff[0] == 4U &&
           ledOff[1] == 4U && ledOff[2] == 4U);
    frame[0] = 0x00U;
    BoardDemo_ProcessRx(CANIF_RX_PDU_VEHICLE_STATUS, 3U, frame, &seen);
    frame[0] = 0xCAU; frame[1] = 4U;
    BoardDemo_ProcessRx(CANIF_RX_PDU_VEHICLE_STATUS, 3U, frame, &seen);
    assert(g_BoardDemoResult.rxInvalid == 3U);
    assert(strcmp(lastUart, "RX INVALID LENGTH/ID/DATA\r\n") == 0);
    puts("PASS: TC-003 Rx validation, duplicate suppression, UART and RGB LEDs.");
    return 0;
}
