#include "S32K144.h"
#include "../bsp/can/board_can.h"
#include "../bsp/LED.h"
#include "../drivers/can/can_driver/Can.h"
#include "../drivers/can/canif/CanIf.h"
#include "../drivers/can/canif/CanIf_Cfg.h"
#include "../drivers/can/pdur/PduR.h"
#include "../drivers/uart/Driver_UART.h"
#include "../drivers/systick/Driver_SysTick.h"
#include "system_S32K144.h"
#include "can_loopback_test.h"
#include <stddef.h>

/* Build one image per board: 0=existing loopback, 1=TC-003 Tx, 2=TC-003 Rx.
 * Set -DBOARD_MODE=1 or -DBOARD_MODE=2 in the build configuration. */
#define BOARD_MODE_LOOPBACK (0U)
#define BOARD_MODE_TC003_TX (1U)
#define BOARD_MODE_TC003_RX (2U)
#ifndef BOARD_MODE
#define BOARD_MODE BOARD_MODE_TC003_RX
#endif
#if ((BOARD_MODE != BOARD_MODE_LOOPBACK) && \
     (BOARD_MODE != BOARD_MODE_TC003_TX) && (BOARD_MODE != BOARD_MODE_TC003_RX))
#error BOARD_MODE_must_be_0_1_or_2
#endif

#if (BOARD_MODE != BOARD_MODE_LOOPBACK)

#define BOARD_DEMO_CAN_ID       (0x321U)
#define BOARD_DEMO_MAGIC        (0xCAU)
#define BOARD_DEMO_UART_BAUD    (115200U)
#define BOARD_DEMO_FRAME_LENGTH (3U)

typedef enum
{
    BOARD_DEMO_STARTING = 0,
    BOARD_DEMO_RUNNING,
    BOARD_DEMO_INIT_FAILED
} BoardDemo_StatusType;

typedef enum
{
    BOARD_DEMO_ERROR_NONE = 0,
    BOARD_DEMO_ERROR_CONFIG,
    BOARD_DEMO_ERROR_UART,
    BOARD_DEMO_ERROR_CAN,
    BOARD_DEMO_ERROR_CANIF,
    BOARD_DEMO_ERROR_SYSTICK
} BoardDemo_ErrorType;

typedef struct
{
    BoardDemo_StatusType status;
    BoardDemo_ErrorType error;
    uint32_t txAccepted;
    uint32_t txConfirmations;
    uint32_t txRejected;
    uint32_t txTimeouts;
    uint32_t rxFrames;
    uint32_t rxInvalid;
    uint32_t rxDuplicates;
    uint32_t uartErrors;
    uint8_t lastFrame[BOARD_DEMO_FRAME_LENGTH];
    uint8_t lastCommand;
    uint8_t lastSequence;
} BoardDemo_ResultType;

volatile BoardDemo_ResultType g_BoardDemoResult;

#if (!defined(BOARD_DEMO_UNIT_TEST) || (BOARD_MODE == BOARD_MODE_TC003_RX))
/** Send one short diagnostic line; retain UART failures for the debugger. */
static void BoardDemo_Log(const char *line)
{
    if (LPUART1_SendString_Blocking(line) != UART_STATUS_OK)
    { g_BoardDemoResult.uartErrors++; }
}

/** Print the exact three CAN payload bytes on the board's UART port. */
static void BoardDemo_LogFrame(char direction, const uint8_t *frame)
{
    static const char digits[] = "0123456789ABCDEF";
    char line[] = "TX CA 00 00\r\n";
    uint8_t i;
    line[0] = direction;
    for (i = 0U; i < BOARD_DEMO_FRAME_LENGTH; i++)
    {
        uint8_t offset = (uint8_t)(3U + (3U * i));
        line[offset] = digits[frame[i] >> 4U];
        line[offset + 1U] = digits[frame[i] & 0x0FU];
    }
    BoardDemo_Log(line);
}
#endif

#ifndef BOARD_DEMO_UNIT_TEST
/** Initialize normal CAN bus mode, UART logging and a 1 ms debounce clock. */
static uint8_t BoardDemo_Init(void)
{
    g_BoardDemoResult.status = BOARD_DEMO_STARTING;
    disable_WDOG();
    init_MCU();
    LED_Init(LED_BLUE);
    LED_Init(LED_RED);
    LED_Init(LED_GREEN);
    LED_Off(LED_BLUE);
    LED_Off(LED_RED);
    LED_Off(LED_GREEN);
    if ((CANIF_NUM_TX_PDUS != 1U) || (CANIF_NUM_RX_PDUS != 1U) ||
        (CanIf_TxPduConfig[0].canId != BOARD_DEMO_CAN_ID) ||
        (CanIf_RxPduConfig[0].canId != BOARD_DEMO_CAN_ID))
    { g_BoardDemoResult.error = BOARD_DEMO_ERROR_CONFIG; goto failed; }
    if (LPUART1_Init(BOARD_DEMO_UART_BAUD) != UART_STATUS_OK)
    { g_BoardDemoResult.error = BOARD_DEMO_ERROR_UART; goto failed; }
    if (Can_Init() != CAN_OK)
    { g_BoardDemoResult.error = BOARD_DEMO_ERROR_CAN; goto failed; }
    if (CanIf_Init() != E_OK)
    { g_BoardDemoResult.error = BOARD_DEMO_ERROR_CANIF; goto failed; }
    SystemCoreClockUpdate();
    if (Driver_SysTick_Init(1000U, NULL) != 0U)
    { g_BoardDemoResult.error = BOARD_DEMO_ERROR_SYSTICK; goto failed; }
    g_BoardDemoResult.status = BOARD_DEMO_RUNNING;
    return 1U;
failed:
    g_BoardDemoResult.status = BOARD_DEMO_INIT_FAILED;
    if (g_BoardDemoResult.error != BOARD_DEMO_ERROR_UART)
    { BoardDemo_Log("INIT FAIL\r\n"); }
    return 0U;
}
#endif

#if (BOARD_MODE == BOARD_MODE_TC003_TX)

typedef struct
{
    uint8_t raw;
    uint8_t stable;
    uint32_t changedAt;
} BoardDemo_ButtonStateType;

/** Report one press only after SW2 has stayed low for 20 ms. */
static uint8_t BoardDemo_ButtonPressed(BoardDemo_ButtonStateType *button,
                                       uint8_t sample, uint32_t now)
{
    if (sample != button->raw)
    { button->raw = sample; button->changedAt = now; }
    if ((sample != button->stable) &&
        ((uint32_t)(now - button->changedAt) >= 20U))
    {
        button->stable = sample;
        return (sample == 0U) ? 1U : 0U;
    }
    return 0U;
}

/** Cycle blue, red, green, off and encode TC-003 magic plus sequence. */
static void BoardDemo_BuildNextFrame(uint8_t *frame, uint8_t *command,
                                     uint8_t *sequence)
{
    *command = (uint8_t)((*command + 1U) & 3U);
    (*sequence)++;
    frame[0] = BOARD_DEMO_MAGIC;
    frame[1] = *command;
    frame[2] = *sequence;
}

#ifndef BOARD_DEMO_UNIT_TEST
/** Configure SW2/PTC12 as an active-low input with its internal pull-up. */
static void BoardDemo_InitButton(void)
{
    PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    PORTC->PCR[12U] = PORT_PCR_MUX(1U) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PTC->PDDR &= ~(1UL << 12U);
}

/** Poll CAN completion and queue one LED command per debounced SW2 press. */
static void BoardDemo_RunTx(void)
{
    uint8_t queued[BOARD_DEMO_FRAME_LENGTH] = {0U};
    uint8_t hasQueued = 0U;
    uint8_t inFlight = 0U;
    uint8_t timeoutReported = 0U;
    uint8_t nextCommand = 0U;
    uint8_t sequence = 0U;
    uint32_t startTick = Driver_SysTick_GetTicks();
    uint32_t attemptTick = startTick - 1U;
    uint32_t acceptedTick = startTick;
    uint32_t confirmations = PduR_TxConfirmationCount;
    BoardDemo_ButtonStateType button = {1U, 1U, startTick};
    PduInfoType info;

    BoardDemo_InitButton();
    BoardDemo_Log("TC003 TX READY CAN=321 UART=115200\r\n");
    for (;;)
    {
        uint32_t now;
        uint8_t sample;
        Can_MainFunction_Write();
        Can_MainFunction_Read();
        now = Driver_SysTick_GetTicks();
        sample = ((PTC->PDIR & (1UL << 12U)) != 0U) ? 1U : 0U;
        if (BoardDemo_ButtonPressed(&button, sample, now) != 0U)
        {
            BoardDemo_BuildNextFrame(queued, &nextCommand, &sequence);
            hasQueued = 1U; /* The most recent press wins while Tx is busy. */
            BoardDemo_LogFrame('T', queued);
        }
        if (PduR_TxConfirmationCount != confirmations)
        {
            confirmations = PduR_TxConfirmationCount;
            if (inFlight != 0U)
            {
                inFlight = 0U;
                g_BoardDemoResult.txConfirmations++;
                BoardDemo_Log("TX DONE\r\n");
            }
        }
        if ((inFlight != 0U) && (timeoutReported == 0U) &&
            ((uint32_t)(now - acceptedTick) >= 1000U))
        {
            timeoutReported = 1U;
            g_BoardDemoResult.txTimeouts++;
            BoardDemo_Log("TX TIMEOUT\r\n");
        }
        if ((hasQueued != 0U) && (inFlight == 0U) && (now != attemptTick))
        {
            attemptTick = now;
            info.SduDataPtr = queued;
            info.SduLength = BOARD_DEMO_FRAME_LENGTH;
            if (CanIf_Transmit(CANIF_TX_PDU_VEHICLE_STATUS, &info) == E_OK)
            {
                uint8_t i;
                for (i = 0U; i < BOARD_DEMO_FRAME_LENGTH; i++)
                { g_BoardDemoResult.lastFrame[i] = queued[i]; }
                g_BoardDemoResult.lastCommand = queued[1];
                g_BoardDemoResult.lastSequence = queued[2];
                g_BoardDemoResult.txAccepted++;
                hasQueued = 0U;
                inFlight = 1U;
                timeoutReported = 0U;
                acceptedTick = now;
                BoardDemo_Log("TX ACCEPTED\r\n");
            }
            else
            { g_BoardDemoResult.txRejected++; }
        }
    }
}
#endif

#else /* BOARD_MODE_TC003_RX */

/** Decode only the three-byte TC-003 LED command format. */
static uint8_t BoardDemo_DecodeFrame(const uint8_t *frame, PduLengthType length)
{
    return ((frame != NULL) && (length == BOARD_DEMO_FRAME_LENGTH) &&
            (frame[0] == BOARD_DEMO_MAGIC) && (frame[1] <= 3U)) ? 1U : 0U;
}

/** Apply one validated LED command; zero turns every LED off. */
static void BoardDemo_ApplyLed(uint8_t command)
{
    LED_Off(LED_BLUE);
    LED_Off(LED_RED);
    LED_Off(LED_GREEN);
    if (command == 1U) { LED_On(LED_BLUE); }
    else if (command == 2U) { LED_On(LED_RED); }
    else if (command == 3U) { LED_On(LED_GREEN); }
}

/** Validate a PduR snapshot, reject duplicates and apply one new LED command. */
static void BoardDemo_ProcessRx(PduIdType id, PduLengthType length,
                                const uint8_t *frame, uint8_t *haveSequence)
{
    uint8_t i;
    if ((id != CANIF_RX_PDU_VEHICLE_STATUS) ||
        (BoardDemo_DecodeFrame(frame, length) == 0U))
    {
        g_BoardDemoResult.rxInvalid++;
        BoardDemo_Log("RX INVALID LENGTH/ID/DATA\r\n");
        return;
    }
    BoardDemo_LogFrame('R', frame);
    if ((*haveSequence != 0U) && (frame[2] == g_BoardDemoResult.lastSequence))
    {
        g_BoardDemoResult.rxDuplicates++;
        BoardDemo_Log("RX DUPLICATE\r\n");
        return;
    }
    *haveSequence = 1U;
    for (i = 0U; i < BOARD_DEMO_FRAME_LENGTH; i++)
    { g_BoardDemoResult.lastFrame[i] = frame[i]; }
    g_BoardDemoResult.lastCommand = frame[1];
    g_BoardDemoResult.lastSequence = frame[2];
    g_BoardDemoResult.rxFrames++;
    BoardDemo_ApplyLed(frame[1]);
}

#ifndef BOARD_DEMO_UNIT_TEST
/** Consume the synchronous PduR snapshot after each CAN receive poll. */
static void BoardDemo_RunRx(void)
{
    uint32_t indications = PduR_RxIndicationCount;
    uint8_t haveSequence = 0U;
    BoardDemo_Log("TC003 RX READY CAN=321 UART=115200\r\n");
    for (;;)
    {
        uint8_t frame[BOARD_DEMO_FRAME_LENGTH];
        uint8_t i;
        Can_MainFunction_Read();
        if (PduR_RxIndicationCount == indications) { continue; }
        indications = PduR_RxIndicationCount;
        if (PduR_LastRxLength == BOARD_DEMO_FRAME_LENGTH)
        {
            for (i = 0U; i < BOARD_DEMO_FRAME_LENGTH; i++)
            { frame[i] = PduR_LastRxBytes[i]; }
            BoardDemo_ProcessRx(PduR_LastRxPduId, PduR_LastRxLength,
                                frame, &haveSequence);
        }
        else
        {
            BoardDemo_ProcessRx(PduR_LastRxPduId, PduR_LastRxLength,
                                NULL, &haveSequence);
        }
    }
}
#endif

#endif /* TC-003 role */

#ifndef BOARD_DEMO_UNIT_TEST
/** Select the physical transmitter or receiver image at compile time. */
static void BoardDemo_Run(void)
{
    if (BoardDemo_Init() == 0U)
    { for (;;) {} }
#if (BOARD_MODE == BOARD_MODE_TC003_TX)
    BoardDemo_RunTx();
#else
    BoardDemo_RunRx();
#endif
}
#endif

#endif /* BOARD_MODE */

#ifndef BOARD_DEMO_UNIT_TEST
/** Dispatch to the old loopback suite or one physical board role. */
int main(void)
{
#if (BOARD_MODE == BOARD_MODE_LOOPBACK)
    CanLoopbackTest_Run();
#else
    BoardDemo_Run();
#endif
    return 0;
}
#endif
