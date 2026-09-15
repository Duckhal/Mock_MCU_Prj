#include "S32K144.h"
#include "../bsp/can/board_can.h"
#include "../bsp/LED.h"
#include "../drivers/can/can_driver/Can.h"
#include "../drivers/can/canif/CanIf.h"
#include "../drivers/can/canif/CanIf_Cfg.h"
#include "../drivers/can/pdur/PduR.h"
#include "../drivers/can/pdur/PduR_Cfg.h"
#include "../drivers/can/com/Com_Cfg.h"
#include "../drivers/can/common/CanStack_Cfg.h"
#include <stddef.h>

/* Board-only harness. All test code and CanIf-boundary capture callbacks live here.
 * Use Debug_FLASH and reset the MCU to run again. No CAN peer is required.
 * Break at the final main loop and inspect g_CanLoopbackTestResult.
 * This tests internal loopback, not CAN wiring or transceiver operation.
 * PduR_ComTransmit uses the real PduR module. Its Rx/confirmation APIs are absent;
 * captures below observe the CanIf boundary, not PduR Rx/confirmation routing.
 * Remove captures when production PduR_CanIf callbacks are implemented.
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
    LOOPBACK_STAGE_PDUR_TX
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
    LOOPBACK_ERROR_PDUR_REJECTION
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
 * CanIf Test - TxConfirmation and RxIndication Capture
 *==========================================================================*/
/** Capture the real CanIf confirmation and check the local PDU ID exactly once. */
void PduR_CanIfTxConfirmation(PduIdType TxPduId)
{
    g_CanLoopbackTestResult.lastTxPduId = TxPduId;
    g_CanLoopbackTestResult.txConfirmations++;
    g_CanLoopbackTestResult.caseTxCount++;
    if ((g_CanLoopbackTestResult.frameActive == 0U) ||
        (TxPduId != CANIF_TX_PDU_VEHICLE_STATUS) ||
        (g_CanLoopbackTestResult.caseTxCount != 1U))
    {
        Loopback_Fail(LOOPBACK_ERROR_TX_CALLBACK);
    }
}

/** Copy the borrowed CanIf Rx payload now and compare it with the sent snapshot. */
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    uint8_t index;

    g_CanLoopbackTestResult.lastRxPduId = RxPduId;
    g_CanLoopbackTestResult.rxIndications++;
    g_CanLoopbackTestResult.caseRxCount++;
    if ((g_CanLoopbackTestResult.frameActive == 0U) ||
        (RxPduId != CANIF_RX_PDU_VEHICLE_STATUS) ||
        (g_CanLoopbackTestResult.caseRxCount != 1U) ||
        (PduInfoPtr == NULL))
    {
        Loopback_Fail(LOOPBACK_ERROR_RX_CALLBACK);
        return;
    }
    g_CanLoopbackTestResult.lastRxLength = PduInfoPtr->SduLength;
    if ((PduInfoPtr->SduLength != g_CanLoopbackTestResult.dlc) ||
        (PduInfoPtr->SduLength > LOOPBACK_DATA_LENGTH) ||
        ((PduInfoPtr->SduLength > 0U) && (PduInfoPtr->SduDataPtr == NULL)))
    {
        Loopback_Fail(LOOPBACK_ERROR_RX_CALLBACK);
        return;
    }
    for (index = 0U; index < PduInfoPtr->SduLength; index++)
    {
        g_CanLoopbackTestResult.receivedBytes[index] = PduInfoPtr->SduDataPtr[index];
        if (PduInfoPtr->SduDataPtr[index] != g_CanLoopbackTestResult.expectedBytes[index])
        {
            Loopback_Fail(LOOPBACK_ERROR_PAYLOAD);
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
 * Board Test Entry Point
 *==========================================================================*/
/** Initialize real modules and run all three Tx paths for DLC 0..8; retain results. */
int main(void)
{
    uint8_t path;
    uint8_t length;

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
    /* Current PduR only implements Tx routing; validate the route under test. */
    if ((PDUR_NUM_TX_ROUTES != 1U) ||
        (PduR_TxRouteConfig[0].sourcePduId != COM_IPDU_VEHICLE_STATUS) ||
        (PduR_TxRouteConfig[0].destPduId != CANIF_TX_PDU_VEHICLE_STATUS) ||
        (PduR_TxRouteConfig[0].globalPduId != GLOBAL_PDU_VEHICLE_STATUS))
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
    if (Loopback_CheckPduRRejections() == 0U)
    {
        goto finished;
    }
    for (path = LOOPBACK_TX_DRIVER; path <= LOOPBACK_TX_PDUR; path++)
    {
        for (length = 0U; length <= LOOPBACK_DATA_LENGTH; length++)
        {
            if (Loopback_RunCase((Loopback_TxPathType)path, length) == 0U)
            {
                goto finished;
            }
        }
    }
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
