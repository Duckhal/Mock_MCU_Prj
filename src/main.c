#include "S32K144.h"
#include "../bsp/can/board_can.h"
#include "../bsp/LED.h"
#include "../drivers/can/can_driver/Can.h"
#include "../drivers/can/canif/CanIf.h"
#include "../drivers/can/canif/CanIf_Cfg.h"
#include "../drivers/can/pdur/PduR.h"
#include "../drivers/can/pdur/PduR_Cfg.h"
#include "../drivers/can/com/Com_Cfg.h"
#include "../drivers/can/com/Com.h"
#include "../drivers/can/common/CanStack_Cfg.h"
#include "../drivers/uart/Driver_UART.h"
#include "../drivers/systick/Driver_SysTick.h"
#include "system_S32K144.h"
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

#if (BOARD_MODE == BOARD_MODE_LOOPBACK)

/* BOARD_MODE=0 runs the original internal-loopback stack test; break at its
 * final loop and inspect g_CanLoopbackTestResult. BOARD_MODE=1/2 builds the
 * physical TC-003 transmitter/receiver and exposes g_BoardDemoResult.
 * Real PduR routes Tx, Rx and confirmation; its Rx snapshot serves the
 * three-byte physical test while COM retains its eight-byte loopback test.
 */
/*=========================================================================
 * Shared Test Configuration and Debug Results
 *==========================================================================*/
#define LOOPBACK_FREEZE_LIMIT    (100000UL)
#define LOOPBACK_POLL_LIMIT      (2000000UL) /* Iterations, not milliseconds. */
#define LOOPBACK_EXTRA_POLLS     (32U)
#define LOOPBACK_DATA_LENGTH     (8U)

typedef enum
{
    LOOPBACK_NOT_STARTED = 0,
    LOOPBACK_RUNNING,
    LOOPBACK_PASS,
    LOOPBACK_FAIL
} Loopback_StatusType;

typedef enum
{
    LOOPBACK_STAGE_BSP = 0,
    LOOPBACK_STAGE_DRIVER_INIT,
    LOOPBACK_STAGE_FREEZE_ENTRY,
    LOOPBACK_STAGE_FREEZE_EXIT,
    LOOPBACK_STAGE_CANIF_INIT,
    LOOPBACK_STAGE_DRIVER_TX,
    LOOPBACK_STAGE_CANIF_TX,
    LOOPBACK_STAGE_DONE,
    LOOPBACK_STAGE_PDUR_VALIDATE,
    LOOPBACK_STAGE_PDUR_TX,
    LOOPBACK_STAGE_COM_INIT,
    LOOPBACK_STAGE_COM_TX,
    LOOPBACK_STAGE_COM_RX
} Loopback_StageType;

typedef enum
{
    LOOPBACK_TX_DRIVER = 0,
    LOOPBACK_TX_CANIF,
    LOOPBACK_TX_PDUR
} Loopback_TxPathType;

typedef enum
{
    LOOPBACK_ERROR_NONE = 0,
    LOOPBACK_ERROR_CONFIG,
    LOOPBACK_ERROR_DRIVER_INIT,
    LOOPBACK_ERROR_FREEZE_ENTRY,
    LOOPBACK_ERROR_FREEZE_EXIT,
    LOOPBACK_ERROR_MODE,
    LOOPBACK_ERROR_CANIF_INIT,
    LOOPBACK_ERROR_TRANSMIT,
    LOOPBACK_ERROR_BUSY,
    LOOPBACK_ERROR_TX_CALLBACK,
    LOOPBACK_ERROR_RX_CALLBACK,
    LOOPBACK_ERROR_PAYLOAD,
    LOOPBACK_ERROR_TIMEOUT,
    LOOPBACK_ERROR_PDUR_REJECTION,
    LOOPBACK_ERROR_COM_INIT,
    LOOPBACK_ERROR_COM_SIGNAL,
    LOOPBACK_ERROR_COM_SCHEDULE,
    LOOPBACK_ERROR_COM_RECEIVE
} Loopback_ErrorType;

typedef struct
{
    Loopback_StatusType status;
    Loopback_StageType stage;
    Loopback_ErrorType error;
    uint32_t passedCases;       /* Expected: 27 (three paths, DLC 0..8). */
    uint32_t txConfirmations;   /* Cumulative; expected: 27. */
    uint32_t rxIndications;     /* Cumulative; expected: 27. */
    uint32_t pdurPassedCases;   /* Expected: 9 actual PduR Tx loopback cases. */
    uint32_t pdurRejectedCases; /* Expected: 4 invalid requests rejected. */
    Loopback_TxPathType txPath;
    PduIdType pdurSourcePduId;
    PduIdType pdurDestPduId;
    GlobalPduIdType pdurGlobalPduId; /* Config metadata; never payload bytes. */
    Std_ReturnType pdurRejectionResult;
    uint32_t caseTxCount;       /* Expected: exactly one per case. */
    uint32_t caseRxCount;
    uint32_t pollIterations;
    uint8_t frameActive;
    uint8_t dlc;
    PduIdType lastTxPduId;
    PduIdType lastRxPduId;
    PduLengthType lastRxLength;
    Can_ReturnType driverInitResult;
    Std_ReturnType canIfInitResult;
    Std_ReturnType comInitResult;
    uint32_t comTxConfirmations;
    uint32_t comRxIndications;
    uint32_t comDrops;
    uint32_t comReceivedSpeed;
    uint32_t comReceivedGear;
    uint32_t comReceivedAlive;
    uint8_t comReceivedSpeedU;
    uint8_t comReceivedGearU;
    uint8_t comReceivedAliveU;
    uint32_t transmitResult;
    uint32_t busyResult;
    uint8_t expectedBytes[LOOPBACK_DATA_LENGTH];
    uint8_t receivedBytes[LOOPBACK_DATA_LENGTH];
    uint32_t mcr;              /* Register snapshot at mode setup, PASS or FAIL. */
    uint32_t ctrl1;
    uint32_t esr1;
    uint32_t iflag1;
} Loopback_ResultType;

volatile Loopback_ResultType g_CanLoopbackTestResult;
static uint8_t Loopback_TxBytes[LOOPBACK_DATA_LENGTH];

/*=========================================================================
 * Shared Test Helpers
 *==========================================================================*/
/** Capture controller diagnostics without reading/locking the Rx mailbox CS. */
static void Loopback_Snapshot(void)
{
    g_CanLoopbackTestResult.mcr = CAN0->MCR;
    g_CanLoopbackTestResult.ctrl1 = CAN0->CTRL1;
    g_CanLoopbackTestResult.esr1 = CAN0->ESR1;
    g_CanLoopbackTestResult.iflag1 = CAN0->IFLAG1;
}

/** Latch the first failure and preserve its stage, counters and register snapshot. */
static void Loopback_Fail(Loopback_ErrorType error)
{
    if (g_CanLoopbackTestResult.status != LOOPBACK_FAIL)
    {
        g_CanLoopbackTestResult.error = error;
        Loopback_Snapshot();
        g_CanLoopbackTestResult.status = LOOPBACK_FAIL;
    }
}

/*=========================================================================
 * Can Test - Freeze Handshakes and Internal Loopback Setup
 *==========================================================================*/
/** Wait for a bounded MCR handshake; return zero if the requested bits never match. */
static uint8_t Loopback_WaitMcr(uint32_t mask, uint32_t expected)
{
    uint32_t remaining;

    for (remaining = LOOPBACK_FREEZE_LIMIT; remaining > 0U; remaining--)
    {
        if ((CAN0->MCR & mask) == expected)
        {
            return 1U;
        }
    }
    return 0U;
}

/** Enable internal loopback after driver init, changing protected bits in Freeze. */
static uint8_t Loopback_EnableMode(void)
{
    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_FREEZE_ENTRY;
    CAN0->MCR |= CAN_MCR_FRZ_MASK | CAN_MCR_HALT_MASK;
    if (Loopback_WaitMcr(CAN_MCR_FRZACK_MASK, CAN_MCR_FRZACK_MASK) == 0U)
    {
        Loopback_Fail(LOOPBACK_ERROR_FREEZE_ENTRY);
        return 0U;
    }
    /* S32K1xx RM, CTRL1[LPB]: self reception requires MCR[SRXDIS]=0.
     * Driver init already disables FDCTRL[TDCEN] and all CAN interrupts. */
    CAN0->CTRL1 |= CAN_CTRL1_LPB_MASK;
    CAN0->MCR &= ~CAN_MCR_SRXDIS_MASK;

    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_FREEZE_EXIT;
    CAN0->MCR &= ~(CAN_MCR_HALT_MASK | CAN_MCR_FRZ_MASK);
    if (Loopback_WaitMcr(CAN_MCR_FRZACK_MASK | CAN_MCR_NOTRDY_MASK, 0U) == 0U)
    {
        Loopback_Fail(LOOPBACK_ERROR_FREEZE_EXIT);
        return 0U;
    }
    if (((CAN0->CTRL1 & CAN_CTRL1_LPB_MASK) == 0U) ||
        ((CAN0->MCR & CAN_MCR_SRXDIS_MASK) != 0U))
    {
        Loopback_Fail(LOOPBACK_ERROR_MODE);
        return 0U;
    }
    Loopback_Snapshot();
    return 1U;
}

/*=========================================================================
 * CanIf / PduR Test - Observe Real Routed Callbacks
 *==========================================================================*/
/** Compare PduR callback snapshots with the active frame after polling. */
static void Loopback_ObserveCallbacks(uint32_t txBefore, uint32_t rxBefore)
{
    uint8_t index;
    uint32_t txCount = PduR_TxConfirmationCount - txBefore;
    uint32_t rxCount = PduR_RxIndicationCount - rxBefore;
    g_CanLoopbackTestResult.caseTxCount = txCount;
    g_CanLoopbackTestResult.caseRxCount = rxCount;
    g_CanLoopbackTestResult.txConfirmations = PduR_TxConfirmationCount;
    g_CanLoopbackTestResult.rxIndications = PduR_RxIndicationCount;
    g_CanLoopbackTestResult.lastTxPduId = PduR_LastTxPduId;
    g_CanLoopbackTestResult.lastRxPduId = PduR_LastRxPduId;
    g_CanLoopbackTestResult.lastRxLength = PduR_LastRxLength;
    if ((txCount > 1U) || ((txCount == 1U) &&
        (PduR_LastTxPduId != CANIF_TX_PDU_VEHICLE_STATUS)))
    { Loopback_Fail(LOOPBACK_ERROR_TX_CALLBACK); }
    if ((rxCount > 1U) || ((rxCount == 1U) &&
        ((PduR_LastRxPduId != CANIF_RX_PDU_VEHICLE_STATUS) ||
         (PduR_LastRxLength != g_CanLoopbackTestResult.dlc))))
    { Loopback_Fail(LOOPBACK_ERROR_RX_CALLBACK); }
    if (rxCount == 1U)
    {
        for (index = 0U; index < PduR_LastRxLength; index++)
        {
            g_CanLoopbackTestResult.receivedBytes[index] = PduR_LastRxBytes[index];
            if (PduR_LastRxBytes[index] != g_CanLoopbackTestResult.expectedBytes[index])
            { Loopback_Fail(LOOPBACK_ERROR_PAYLOAD); }
        }
    }
}

/*=========================================================================
 * PduR Test - Invalid Transmit Requests
 *==========================================================================*/
/** Verify invalid PduR requests fail without creating a Tx or Rx callback. */
static uint8_t Loopback_CheckPduRRejections(void)
{
    uint8_t index;
    PduInfoType pdu;
    Std_ReturnType result;

    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_PDUR_VALIDATE;
    for (index = 0U; index < 4U; index++)
    {
        pdu.SduDataPtr = Loopback_TxBytes;
        pdu.SduLength = LOOPBACK_DATA_LENGTH;
        switch (index)
        {
        case 0U:
            /* Exactly one route was checked by main; this ID cannot match it. */
            result = PduR_ComTransmit((PduIdType)(COM_IPDU_VEHICLE_STATUS ^ 0xFFFFU), &pdu);
            break;
        case 1U:
            result = PduR_ComTransmit(COM_IPDU_VEHICLE_STATUS, NULL);
            break;
        case 2U:
            pdu.SduLength = LOOPBACK_DATA_LENGTH + 1U;
            result = PduR_ComTransmit(COM_IPDU_VEHICLE_STATUS, &pdu);
            break;
        default:
            pdu.SduLength = 1U;
            pdu.SduDataPtr = NULL;
            result = PduR_ComTransmit(COM_IPDU_VEHICLE_STATUS, &pdu);
            break;
        }
        g_CanLoopbackTestResult.pdurRejectionResult = result;
        if (result != E_NOT_OK)
        {
            Loopback_Fail(LOOPBACK_ERROR_PDUR_REJECTION);
            return 0U;
        }
        Can_MainFunction_Write();
        Can_MainFunction_Read();
        if (g_CanLoopbackTestResult.status == LOOPBACK_FAIL)
        {
            return 0U;
        }
        g_CanLoopbackTestResult.pdurRejectedCases++;
    }
    return 1U;
}

/*=========================================================================
 * Shared Test Case - Payload Preparation and Module Selection
 *==========================================================================*/
/** Send one DLC case through CanDrv, CanIf or PduR; check BUSY/copy/callbacks. */
static uint8_t Loopback_RunCase(Loopback_TxPathType path, uint8_t length)
{
    static const uint8_t pattern[LOOPBACK_DATA_LENGTH] =
        {0xA5U, 0x00U, 0xFFU, 0x12U, 0x34U, 0x56U, 0x78U, 0x9BU};
    uint8_t index;
    uint32_t poll;
    uint32_t txBefore = PduR_TxConfirmationCount;
    uint32_t rxBefore = PduR_RxIndicationCount;
    Can_PduType canPdu;
    PduInfoType pdu;

    g_CanLoopbackTestResult.txPath = path;
    g_CanLoopbackTestResult.stage = (path == LOOPBACK_TX_PDUR) ? LOOPBACK_STAGE_PDUR_TX :
        ((path == LOOPBACK_TX_CANIF) ? LOOPBACK_STAGE_CANIF_TX : LOOPBACK_STAGE_DRIVER_TX);
    g_CanLoopbackTestResult.dlc = length;
    g_CanLoopbackTestResult.caseTxCount = 0U;
    g_CanLoopbackTestResult.caseRxCount = 0U;
    g_CanLoopbackTestResult.pollIterations = 0U;
    g_CanLoopbackTestResult.lastRxLength = 0U;
    for (index = 0U; index < LOOPBACK_DATA_LENGTH; index++)
    {
        Loopback_TxBytes[index] = pattern[index] ^ (uint8_t)(length + (uint8_t)path * 16U);
        g_CanLoopbackTestResult.expectedBytes[index] = Loopback_TxBytes[index];
        g_CanLoopbackTestResult.receivedBytes[index] = 0U;
    }
    pdu.SduLength = length;
    pdu.SduDataPtr = (length == 0U) ? NULL : Loopback_TxBytes;
    /* A driver-direct request still needs a configured CanIf local handle
     * so the real CanIf_TxConfirmation forwards it to the capture callback. */
    canPdu.swPduHandle = CANIF_TX_PDU_VEHICLE_STATUS;
    canPdu.id = CanIf_TxPduConfig[0].canId;
    canPdu.length = length;
    canPdu.sdu = pdu.SduDataPtr;
    g_CanLoopbackTestResult.frameActive = 1U;
    if (path == LOOPBACK_TX_PDUR)
    {
        /*=========================================================================
         * PduR Test - COM to CanIf Routing and BUSY Propagation
         *==========================================================================*/
        g_CanLoopbackTestResult.transmitResult = PduR_ComTransmit(COM_IPDU_VEHICLE_STATUS, &pdu);
        if (g_CanLoopbackTestResult.transmitResult != E_OK)
        {
            Loopback_Fail(LOOPBACK_ERROR_TRANSMIT);
            return 0U;
        }
        g_CanLoopbackTestResult.busyResult = PduR_ComTransmit(COM_IPDU_VEHICLE_STATUS, &pdu);
        if (g_CanLoopbackTestResult.busyResult != E_NOT_OK)
        {
            Loopback_Fail(LOOPBACK_ERROR_BUSY);
            return 0U;
        }
    }
    else if (path == LOOPBACK_TX_CANIF)
    {
        /*=========================================================================
         * CanIf Test - Local PDU Mapping and BUSY Propagation
         *==========================================================================*/
        g_CanLoopbackTestResult.transmitResult = CanIf_Transmit(CANIF_TX_PDU_VEHICLE_STATUS, &pdu);
        if (g_CanLoopbackTestResult.transmitResult != E_OK)
        {
            Loopback_Fail(LOOPBACK_ERROR_TRANSMIT);
            return 0U;
        }
        g_CanLoopbackTestResult.busyResult = CanIf_Transmit(CANIF_TX_PDU_VEHICLE_STATUS, &pdu);
        if (g_CanLoopbackTestResult.busyResult != E_NOT_OK)
        {
            Loopback_Fail(LOOPBACK_ERROR_BUSY);
            return 0U;
        }
    }
    else
    {
        /*=========================================================================
         * Can Test - Direct Hardware Object Transmission and CAN_BUSY
         *==========================================================================*/
        g_CanLoopbackTestResult.transmitResult = Can_Write(CanIf_TxPduConfig[0].hth, &canPdu);
        if (g_CanLoopbackTestResult.transmitResult != CAN_OK)
        {
            Loopback_Fail(LOOPBACK_ERROR_TRANSMIT);
            return 0U;
        }
        g_CanLoopbackTestResult.busyResult = Can_Write(CanIf_TxPduConfig[0].hth, &canPdu);
        if (g_CanLoopbackTestResult.busyResult != CAN_BUSY)
        {
            Loopback_Fail(LOOPBACK_ERROR_BUSY);
            return 0U;
        }
    }
    /*=========================================================================
     * Shared Test Checks - Payload Copy, Polling and Exactly-Once Callbacks
     *==========================================================================*/
    /* No lower layer may modify the caller's bytes (including Update bits). */
    for (index = 0U; index < LOOPBACK_DATA_LENGTH; index++)
    {
        if (Loopback_TxBytes[index] != g_CanLoopbackTestResult.expectedBytes[index])
        {
            Loopback_Fail(LOOPBACK_ERROR_PAYLOAD);
            return 0U;
        }
    }
    /* Change the caller buffer after acceptance to verify the driver's copy. */
    for (index = 0U; index < LOOPBACK_DATA_LENGTH; index++)
    {
        Loopback_TxBytes[index] = 0U;
    }
    for (poll = 0U; poll < LOOPBACK_POLL_LIMIT; poll++)
    {
        Can_MainFunction_Write();
        Can_MainFunction_Read();
        Loopback_ObserveCallbacks(txBefore, rxBefore);
        g_CanLoopbackTestResult.pollIterations = poll + 1U;
        if (g_CanLoopbackTestResult.status == LOOPBACK_FAIL)
        {
            return 0U;
        }
        if ((g_CanLoopbackTestResult.caseTxCount == 1U) &&
            (g_CanLoopbackTestResult.caseRxCount == 1U))
        {
            break;
        }
    }
    if (poll == LOOPBACK_POLL_LIMIT)
    {
        Loopback_Fail(LOOPBACK_ERROR_TIMEOUT);
        return 0U;
    }
    /* Repeated polling must not deliver a second confirmation or Rx indication. */
    for (poll = 0U; poll < LOOPBACK_EXTRA_POLLS; poll++)
    {
        Can_MainFunction_Write();
        Can_MainFunction_Read();
        Loopback_ObserveCallbacks(txBefore, rxBefore);
        if (g_CanLoopbackTestResult.status == LOOPBACK_FAIL)
        {
            return 0U;
        }
    }
    g_CanLoopbackTestResult.frameActive = 0U;
    g_CanLoopbackTestResult.passedCases++;
    if (path == LOOPBACK_TX_PDUR)
    {
        g_CanLoopbackTestResult.pdurPassedCases++;
    }
    return 1U;
}

/*=========================================================================
 * COM Test - Signal Packing, Periodic Scheduling, Retry and Full Rx Route
 *==========================================================================*/
/** Poll both CAN functions until one COM Tx completion and Rx I-PDU arrive. */
static uint8_t Loopback_WaitComFrame(uint32_t txTarget, uint32_t rxTarget)
{
    uint32_t poll;
    for (poll = 0U; poll < LOOPBACK_POLL_LIMIT; poll++)
    {
        Can_MainFunction_Write();
        Can_MainFunction_Read();
        if ((Com_TxConfirmationCount == txTarget) &&
            (Com_RxIndicationCount == rxTarget))
        { return 1U; }
        if ((Com_TxConfirmationCount > txTarget) ||
            (Com_RxIndicationCount > rxTarget))
        { break; }
    }
    Loopback_Fail(LOOPBACK_ERROR_COM_RECEIVE);
    return 0U;
}

/** Prove t=1 acceptance, t=11 BUSY and t=12 latest-value retry via COM. */
static uint8_t Loopback_RunComTest(void)
{
    uint32_t txBefore = Com_TxConfirmationCount;
    uint32_t rxBefore = Com_RxIndicationCount;
    uint32_t speed;
    uint32_t gear;
    uint32_t alive;
    uint8_t speedU;
    uint8_t gearU;
    uint8_t aliveU;
    uint8_t tick;

    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_COM_TX;
    if ((Com_SendSignal(COM_SIGNAL_VEHICLE_SPEED, 100U) != E_OK) ||
        (Com_SendSignal(COM_SIGNAL_GEAR, 3U) != E_OK) ||
        (Com_SendSignal(COM_SIGNAL_ALIVE_COUNTER, 5U) != E_OK) ||
        (Com_SendSignal(COM_SIGNAL_GEAR, 128U) != E_NOT_OK))
    { Loopback_Fail(LOOPBACK_ERROR_COM_SIGNAL); return 0U; }

    Com_MainFunctionTx(); /* t=1: period 10, offset 1. */
    if ((CAN0->RAMn[33U] != (CanIf_TxPduConfig[0].canId << 18U)) ||
        (CAN0->RAMn[34U] != 0xC900070BU))
    { Loopback_Fail(LOOPBACK_ERROR_COM_SCHEDULE); return 0U; }
    for (tick = 0U; tick < 9U; tick++)
    { Com_MainFunctionTx(); } /* t=2..10: no nominal due. */
    Com_MainFunctionTx(); /* t=11: nominal due, MB8 still reserved => pending. */
    if ((Com_TxConfirmationCount != txBefore) || (Com_TxDropCount != 0U))
    { Loopback_Fail(LOOPBACK_ERROR_COM_SCHEDULE); return 0U; }

    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_COM_RX;
    if (Loopback_WaitComFrame(txBefore + 1U, rxBefore + 1U) == 0U)
    { return 0U; }
    if ((Com_ReceiveSignal(COM_SIGNAL_RX_VEHICLE_SPEED, &speed, &speedU) != E_OK) ||
        (speed != 100U) || (speedU != 1U))
    { Loopback_Fail(LOOPBACK_ERROR_COM_RECEIVE); return 0U; }
    g_CanLoopbackTestResult.comReceivedSpeed = speed;
    g_CanLoopbackTestResult.comReceivedSpeedU = speedU;

    if (Com_SendSignal(COM_SIGNAL_VEHICLE_SPEED, 120U) != E_OK)
    { Loopback_Fail(LOOPBACK_ERROR_COM_SIGNAL); return 0U; }
    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_COM_TX;
    Com_MainFunctionTx(); /* t=12: pending retry uses latest value. */
    if (Com_TxDropCount != 0U)
    { Loopback_Fail(LOOPBACK_ERROR_COM_SCHEDULE); return 0U; }
    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_COM_RX;
    if (Loopback_WaitComFrame(txBefore + 2U, rxBefore + 2U) == 0U)
    { return 0U; }
    if ((Com_ReceiveSignal(COM_SIGNAL_RX_VEHICLE_SPEED, &speed, &speedU) != E_OK) ||
        (Com_ReceiveSignal(COM_SIGNAL_RX_GEAR, &gear, &gearU) != E_OK) ||
        (Com_ReceiveSignal(COM_SIGNAL_RX_ALIVE_COUNTER, &alive, &aliveU) != E_OK) ||
        (speed != 120U) || (speedU != 1U) ||
        (gear != 3U) || (gearU != 0U) ||
        (alive != 5U) || (aliveU != 0U))
    { Loopback_Fail(LOOPBACK_ERROR_COM_RECEIVE); return 0U; }
    g_CanLoopbackTestResult.comReceivedSpeed = speed;
    g_CanLoopbackTestResult.comReceivedSpeedU = speedU;
    g_CanLoopbackTestResult.comReceivedGear = gear;
    g_CanLoopbackTestResult.comReceivedGearU = gearU;
    g_CanLoopbackTestResult.comReceivedAlive = alive;
    g_CanLoopbackTestResult.comReceivedAliveU = aliveU;
    g_CanLoopbackTestResult.comTxConfirmations = Com_TxConfirmationCount;
    g_CanLoopbackTestResult.comRxIndications = Com_RxIndicationCount;
    g_CanLoopbackTestResult.comDrops = Com_TxDropCount;
    return 1U;
}

/*=========================================================================
 * Board Test Entry Point
 *==========================================================================*/
/** Run all DLC 0..8 cases through one requested stack entry point. */
static uint8_t Loopback_RunPathCases(Loopback_TxPathType path)
{
    uint8_t length;
    for (length = 0U; length <= LOOPBACK_DATA_LENGTH; length++)
    {
        if (Loopback_RunCase(path, length) == 0U) { return 0U; }
    }
    return 1U;
}

/** Test the CanDrv direct path. */
static uint8_t Loopback_TestCanDriver(void)
{ return Loopback_RunPathCases(LOOPBACK_TX_DRIVER); }

/** Test the CanIf path. */
static uint8_t Loopback_TestCanIf(void)
{ return Loopback_RunPathCases(LOOPBACK_TX_CANIF); }

/** Test the PduR Tx path. */
static uint8_t Loopback_TestPduR(void)
{ return Loopback_RunPathCases(LOOPBACK_TX_PDUR); }

/** Run all three raw Tx paths and the full COM Signal loopback test. */
static void Loopback_RunAll(void)
{
    g_CanLoopbackTestResult.status = LOOPBACK_RUNNING;
    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_BSP;
    disable_WDOG();
    init_MCU(); /* If stopped here, inspect the BSP's unbounded SOSC wait. */
    LED_Off(LED_GREEN);
    /*=========================================================================
     * CanIf Test - Loopback PDU Configuration
     *==========================================================================*/
    /* This board test expects one bidirectional VehicleStatus L-PDU binding. */
    if ((CANIF_NUM_TX_PDUS != 1U) || (CANIF_NUM_RX_PDUS != 1U) ||
        (CanIf_TxPduConfig[0].txPduId != CANIF_TX_PDU_VEHICLE_STATUS) ||
        (CanIf_RxPduConfig[0].rxPduId != CANIF_RX_PDU_VEHICLE_STATUS) ||
        (CanIf_TxPduConfig[0].canId != CanIf_RxPduConfig[0].canId))
    {
        Loopback_Fail(LOOPBACK_ERROR_CONFIG);
        goto finished;
    }
    /*=========================================================================
     * PduR Test - Tx Route Configuration
     *==========================================================================*/
    /* Verify both directions and one logical GlobalPduId across the route. */
    if ((PDUR_NUM_TX_ROUTES != 1U) ||
        (PDUR_NUM_RX_ROUTES != 1U) ||
        (PduR_TxRouteConfig[0].sourcePduId != COM_IPDU_VEHICLE_STATUS) ||
        (PduR_TxRouteConfig[0].destPduId != CANIF_TX_PDU_VEHICLE_STATUS) ||
        (PduR_TxRouteConfig[0].globalPduId != GLOBAL_PDU_VEHICLE_STATUS) ||
        (PduR_RxRouteConfig[0].sourcePduId != CANIF_RX_PDU_VEHICLE_STATUS) ||
        (PduR_RxRouteConfig[0].destPduId != COM_IPDU_RX_VEHICLE_STATUS) ||
        (PduR_RxRouteConfig[0].globalPduId != GLOBAL_PDU_VEHICLE_STATUS))
    {
        Loopback_Fail(LOOPBACK_ERROR_CONFIG);
        goto finished;
    }
    g_CanLoopbackTestResult.pdurSourcePduId = PduR_TxRouteConfig[0].sourcePduId;
    g_CanLoopbackTestResult.pdurDestPduId = PduR_TxRouteConfig[0].destPduId;
    g_CanLoopbackTestResult.pdurGlobalPduId = PduR_TxRouteConfig[0].globalPduId;
    /*=========================================================================
     * Can Test - Driver Initialization and Loopback Mode
     *==========================================================================*/
    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_DRIVER_INIT;
    g_CanLoopbackTestResult.driverInitResult = Can_Init();
    if (g_CanLoopbackTestResult.driverInitResult != CAN_OK)
    {
        Loopback_Fail(LOOPBACK_ERROR_DRIVER_INIT);
        goto finished;
    }
    if (Loopback_EnableMode() == 0U)
    {
        goto finished;
    }
    /*=========================================================================
     * CanIf Test - Module Initialization
     *==========================================================================*/
    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_CANIF_INIT;
    g_CanLoopbackTestResult.canIfInitResult = CanIf_Init();
    if (g_CanLoopbackTestResult.canIfInitResult != E_OK)
    {
        Loopback_Fail(LOOPBACK_ERROR_CANIF_INIT);
        goto finished;
    }
    /*=========================================================================
     * COM Test - Module Initialization
     *==========================================================================*/
    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_COM_INIT;
    g_CanLoopbackTestResult.comInitResult = Com_Init();
    if (g_CanLoopbackTestResult.comInitResult != E_OK)
    { Loopback_Fail(LOOPBACK_ERROR_COM_INIT); goto finished; }
    if (Loopback_CheckPduRRejections() == 0U)
    {
        goto finished;
    }
    if ((Loopback_TestCanDriver() == 0U) ||
        (Loopback_TestCanIf() == 0U) ||
        (Loopback_TestPduR() == 0U))
    {
        goto finished;
    }
    if (Loopback_RunComTest() == 0U)
    { goto finished; }
    Loopback_Snapshot();
    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_DONE;
    g_CanLoopbackTestResult.status = LOOPBACK_PASS;
    LED_On(LED_GREEN);

finished:
    /* Set a breakpoint here. PASS=2, FAIL=3; a reset restarts the complete test. */
    for (;;)
    {
    }
}

#else /* Physical TC-003 test on two boards. */

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
    Loopback_RunAll();
#else
    BoardDemo_Run();
#endif
    return 0;
}
#endif
