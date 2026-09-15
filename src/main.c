#include "S32K144.h"
#include "../bsp/can/board_can.h"
#include "../bsp/LED.h"
#include "../drivers/can/can_driver/Can.h"
#include "../drivers/can/canif/CanIf.h"
#include "../drivers/can/canif/CanIf_Cfg.h"
#include <stddef.h>

/* Board-only harness. All test code and PduR capture callbacks live here.
 * Use Debug_FLASH and reset the MCU to run again. No CAN peer is required.
 * Break at the final main loop and inspect g_CanLoopbackTestResult.
 * This tests internal loopback, not CAN wiring or transceiver operation.
 * Do not link these capture callbacks with a future production PduR implementation.
 */
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
    LOOPBACK_STAGE_DONE
} Loopback_StageType;

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
    LOOPBACK_ERROR_TIMEOUT
} Loopback_ErrorType;

typedef struct
{
    Loopback_StatusType status;
    Loopback_StageType stage;
    Loopback_ErrorType error;
    uint32_t passedCases;       /* Expected: 18 (two paths, DLC 0..8). */
    uint32_t txConfirmations;   /* Cumulative; expected: 18. */
    uint32_t rxIndications;     /* Cumulative; expected: 18. */
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

/** Send one DLC case through the selected API, checking BUSY, copy and callbacks. */
static uint8_t Loopback_RunCase(uint8_t throughCanIf, uint8_t length)
{
    static const uint8_t pattern[LOOPBACK_DATA_LENGTH] =
        {0xA5U, 0x00U, 0xFFU, 0x12U, 0x34U, 0x56U, 0x78U, 0x9BU};
    uint8_t index;
    uint32_t poll;
    Can_PduType canPdu;
    PduInfoType pdu;

    g_CanLoopbackTestResult.stage = (throughCanIf != 0U) ?
        LOOPBACK_STAGE_CANIF_TX : LOOPBACK_STAGE_DRIVER_TX;
    g_CanLoopbackTestResult.dlc = length;
    g_CanLoopbackTestResult.caseTxCount = 0U;
    g_CanLoopbackTestResult.caseRxCount = 0U;
    g_CanLoopbackTestResult.pollIterations = 0U;
    g_CanLoopbackTestResult.lastRxLength = 0U;
    for (index = 0U; index < LOOPBACK_DATA_LENGTH; index++)
    {
        Loopback_TxBytes[index] = pattern[index] ^ (uint8_t)(length + throughCanIf * 16U);
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
    if (throughCanIf != 0U)
    {
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
    return 1U;
}

/** Initialize real modules and run both Tx paths for DLC 0..8; preserve final state. */
int main(void)
{
    uint8_t path;
    uint8_t length;

    g_CanLoopbackTestResult.status = LOOPBACK_RUNNING;
    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_BSP;
    disable_WDOG();
    init_MCU(); /* If stopped here, inspect the BSP's unbounded SOSC wait. */
    LED_Off(LED_GREEN);
    /* This board test expects one bidirectional VehicleStatus L-PDU binding. */
    if ((CANIF_NUM_TX_PDUS != 1U) || (CANIF_NUM_RX_PDUS != 1U) ||
        (CanIf_TxPduConfig[0].txPduId != CANIF_TX_PDU_VEHICLE_STATUS) ||
        (CanIf_RxPduConfig[0].rxPduId != CANIF_RX_PDU_VEHICLE_STATUS) ||
        (CanIf_TxPduConfig[0].canId != CanIf_RxPduConfig[0].canId))
    {
        Loopback_Fail(LOOPBACK_ERROR_CONFIG);
        goto finished;
    }
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
    g_CanLoopbackTestResult.stage = LOOPBACK_STAGE_CANIF_INIT;
    g_CanLoopbackTestResult.canIfInitResult = CanIf_Init();
    if (g_CanLoopbackTestResult.canIfInitResult != E_OK)
    {
        Loopback_Fail(LOOPBACK_ERROR_CANIF_INIT);
        goto finished;
    }
    for (path = 0U; path < 2U; path++)
    {
        for (length = 0U; length <= LOOPBACK_DATA_LENGTH; length++)
        {
            if (Loopback_RunCase(path, length) == 0U)
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
