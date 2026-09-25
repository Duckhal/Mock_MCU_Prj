#include "can_loopback_test.h"
#include "S32K144.h"
#include "../app/node_app.h"
#include "../drivers/can/can_driver/Can.h"
#include "../drivers/can/cantp/Cantp.h"
#include "../drivers/systick/Driver_SysTick.h"

#define CANTP_LOOPBACK_FREEZE_LIMIT  (100000UL)
#define CANTP_LOOPBACK_TIMEOUT_TICKS (500U)
#define CANTP_LOOPBACK_SPIN_LIMIT    (50000000UL)

volatile CanTpLoopback_ResultType g_CanTpLoopbackTestResult;

/** Capture CAN0 registers for debugger evidence without locking the Rx MB. */
static void CanTpLoopback_Snapshot(void)
{
    g_CanTpLoopbackTestResult.mcr = CAN0->MCR;
    g_CanTpLoopbackTestResult.ctrl1 = CAN0->CTRL1;
    g_CanTpLoopbackTestResult.esr1 = CAN0->ESR1;
    g_CanTpLoopbackTestResult.iflag1 = CAN0->IFLAG1;
}

/** Preserve the first detected failure and its hardware snapshot. */
static void CanTpLoopback_Fail(CanTpLoopback_ErrorType Error)
{
    if (g_CanTpLoopbackTestResult.status != CANTP_LOOPBACK_FAIL)
    {
        g_CanTpLoopbackTestResult.error = Error;
        CanTpLoopback_Snapshot();
        g_CanTpLoopbackTestResult.status = CANTP_LOOPBACK_FAIL;
    }
}

/** Wait for one bounded CAN0 MCR handshake. */
static uint8_t CanTpLoopback_WaitMcr(uint32_t Mask, uint32_t Expected)
{
    uint32_t remaining;

    for (remaining = CANTP_LOOPBACK_FREEZE_LIMIT; remaining > 0U; remaining--)
    {
        if ((CAN0->MCR & Mask) == Expected)
        {
            return 1U;
        }
    }
    return 0U;
}

/** Change internal loopback only while CAN0 acknowledges Freeze mode. */
Std_ReturnType CanTpLoopbackTest_SetEnabled(uint8_t Enable)
{
    if (g_CanTpLoopbackTestResult.status != CANTP_LOOPBACK_FAIL)
    {
        g_CanTpLoopbackTestResult.stage = (Enable != 0U) ?
            CANTP_LOOPBACK_STAGE_ENABLE_MODE :
            CANTP_LOOPBACK_STAGE_DISABLE_MODE;
    }

    CAN0->MCR |= CAN_MCR_FRZ_MASK | CAN_MCR_HALT_MASK;
    if (CanTpLoopback_WaitMcr(CAN_MCR_FRZACK_MASK,
                              CAN_MCR_FRZACK_MASK) == 0U)
    {
        CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_FREEZE_ENTRY);
        return E_NOT_OK;
    }

    if (Enable != 0U)
    {
        CAN0->CTRL1 |= CAN_CTRL1_LPB_MASK;
        CAN0->MCR &= ~CAN_MCR_SRXDIS_MASK;
    }
    else
    {
        CAN0->CTRL1 &= ~CAN_CTRL1_LPB_MASK;
        CAN0->MCR |= CAN_MCR_SRXDIS_MASK;
    }

    CAN0->MCR &= ~(CAN_MCR_HALT_MASK | CAN_MCR_FRZ_MASK);
    if (CanTpLoopback_WaitMcr(CAN_MCR_FRZACK_MASK | CAN_MCR_NOTRDY_MASK,
                              0U) == 0U)
    {
        CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_FREEZE_EXIT);
        return E_NOT_OK;
    }
    if ((Enable != 0U) &&
        (((CAN0->CTRL1 & CAN_CTRL1_LPB_MASK) == 0U) ||
         ((CAN0->MCR & CAN_MCR_SRXDIS_MASK) != 0U)))
    {
        CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_MODE);
        return E_NOT_OK;
    }
    if ((Enable == 0U) &&
        (((CAN0->CTRL1 & CAN_CTRL1_LPB_MASK) != 0U) ||
         ((CAN0->MCR & CAN_MCR_SRXDIS_MASK) == 0U)))
    {
        CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_MODE);
        return E_NOT_OK;
    }
    return E_OK;
}

/** Initialize the observable result and deterministic increasing payload. */
static void CanTpLoopback_ResetResult(void)
{
    uint8_t index;

    g_CanTpLoopbackTestResult.status = CANTP_LOOPBACK_RUNNING;
    g_CanTpLoopbackTestResult.stage = CANTP_LOOPBACK_STAGE_IDLE;
    g_CanTpLoopbackTestResult.error = CANTP_LOOPBACK_ERROR_NONE;
    g_CanTpLoopbackTestResult.transmitResult = E_NOT_OK;
    g_CanTpLoopbackTestResult.txResult = E_NOT_OK;
    g_CanTpLoopbackTestResult.rxResult = E_NOT_OK;
    g_CanTpLoopbackTestResult.receivedLength = 0U;
    g_CanTpLoopbackTestResult.elapsedTicks = 0U;
    g_CanTpLoopbackTestResult.txConfirmationCount = 0U;
    g_CanTpLoopbackTestResult.rxIndicationCount = 0U;
    for (index = 0U; index < CANTP_LOOPBACK_PAYLOAD_LENGTH; index++)
    {
        g_CanTpLoopbackTestResult.expectedData[index] = index;
        g_CanTpLoopbackTestResult.receivedData[index] = 0U;
    }
    CanTpLoopback_Snapshot();
}

/** Verify final callbacks, queue length and every reassembled payload byte. */
static uint8_t CanTpLoopback_Verify(void)
{
    uint8_t received[CANTP_LOOPBACK_PAYLOAD_LENGTH];
    PduLengthType length = 0U;
    uint8_t index;

    g_CanTpLoopbackTestResult.stage = CANTP_LOOPBACK_STAGE_VERIFY;
    g_CanTpLoopbackTestResult.txResult = NodeApp_LastTxResult;
    g_CanTpLoopbackTestResult.rxResult = NodeApp_LastRxResult;
    g_CanTpLoopbackTestResult.txConfirmationCount =
        NodeApp_TxConfirmationCount;
    g_CanTpLoopbackTestResult.rxIndicationCount =
        NodeApp_RxIndicationCount;

    if (NodeApp_LastTxResult != E_OK)
    {
        CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_TX_RESULT);
        return 0U;
    }
    if (NodeApp_LastRxResult != E_OK)
    {
        CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_RX_RESULT);
        return 0U;
    }
    if (NodeApp_Receive(received, sizeof(received), &length) != E_OK)
    {
        CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_RECEIVE);
        return 0U;
    }
    g_CanTpLoopbackTestResult.receivedLength = length;
    if (length != CANTP_LOOPBACK_PAYLOAD_LENGTH)
    {
        CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_LENGTH);
        return 0U;
    }

    for (index = 0U; index < length; index++)
    {
        g_CanTpLoopbackTestResult.receivedData[index] = received[index];
        if (received[index] !=
            g_CanTpLoopbackTestResult.expectedData[index])
        {
            CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_PAYLOAD);
            return 0U;
        }
    }
    return 1U;
}

/** Run one real 62-byte CanTp transfer, then restore normal CAN operation. */
Std_ReturnType CanTpLoopbackTest_Run(void)
{
    uint8_t payload[CANTP_LOOPBACK_PAYLOAD_LENGTH];
    uint8_t index;
    uint32_t txBefore;
    uint32_t rxBefore;
    uint32_t startTick;
    uint32_t processedTick;
    uint32_t now;
    uint32_t spins = CANTP_LOOPBACK_SPIN_LIMIT;
    uint8_t completed = 0U;

    CanTpLoopback_ResetResult();
    for (index = 0U; index < CANTP_LOOPBACK_PAYLOAD_LENGTH; index++)
    {
        payload[index] = g_CanTpLoopbackTestResult.expectedData[index];
    }
    if (CanTpLoopbackTest_SetEnabled(1U) != E_OK)
    {
        return E_NOT_OK;
    }

    txBefore = NodeApp_TxConfirmationCount;
    rxBefore = NodeApp_RxIndicationCount;
    g_CanTpLoopbackTestResult.stage = CANTP_LOOPBACK_STAGE_TRANSMIT;
    g_CanTpLoopbackTestResult.transmitResult = NodeApp_Transmit(
        payload, CANTP_LOOPBACK_PAYLOAD_LENGTH);
    if (g_CanTpLoopbackTestResult.transmitResult != E_OK)
    {
        CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_TRANSMIT);
        (void)CanTpLoopbackTest_SetEnabled(0U);
        return E_NOT_OK;
    }

    g_CanTpLoopbackTestResult.stage = CANTP_LOOPBACK_STAGE_POLL;
    startTick = Driver_SysTick_GetTicks();
    processedTick = startTick;
    while (((uint32_t)(processedTick - startTick) <
            CANTP_LOOPBACK_TIMEOUT_TICKS) &&
           (spins > 0U))
    {
        spins--;
        now = Driver_SysTick_GetTicks();
        while ((uint32_t)(now - processedTick) > 0U)
        {
            processedTick++;
            Can_MainFunction_Write();
            Can_MainFunction_Read();
            CanTp_MainFunction();
            g_CanTpLoopbackTestResult.elapsedTicks =
                (uint32_t)(processedTick - startTick);

            if ((NodeApp_TxConfirmationCount == (txBefore + 1U)) &&
                (NodeApp_LastTxResult != E_OK))
            {
                CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_TX_RESULT);
                break;
            }
            if ((NodeApp_RxIndicationCount == (rxBefore + 1U)) &&
                (NodeApp_LastRxResult != E_OK))
            {
                CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_RX_RESULT);
                break;
            }
            if ((NodeApp_TxConfirmationCount == (txBefore + 1U)) &&
                (NodeApp_RxIndicationCount == (rxBefore + 1U)) &&
                (NodeApp_GetReadyCount() == 1U))
            {
                completed = 1U;
                break;
            }
        }
        if ((completed != 0U) ||
            (g_CanTpLoopbackTestResult.status == CANTP_LOOPBACK_FAIL))
        {
            break;
        }
    }

    g_CanTpLoopbackTestResult.txResult = NodeApp_LastTxResult;
    g_CanTpLoopbackTestResult.rxResult = NodeApp_LastRxResult;
    g_CanTpLoopbackTestResult.txConfirmationCount =
        NodeApp_TxConfirmationCount;
    g_CanTpLoopbackTestResult.rxIndicationCount =
        NodeApp_RxIndicationCount;
    if ((completed == 0U) &&
        (g_CanTpLoopbackTestResult.status != CANTP_LOOPBACK_FAIL))
    {
        CanTpLoopback_Fail(CANTP_LOOPBACK_ERROR_TIMEOUT);
    }
    else
    {
        (void)CanTpLoopback_Verify();
    }

    if (CanTpLoopbackTest_SetEnabled(0U) != E_OK)
    {
        return E_NOT_OK;
    }
    if (g_CanTpLoopbackTestResult.status == CANTP_LOOPBACK_FAIL)
    {
        return E_NOT_OK;
    }

    CanTpLoopback_Snapshot();
    g_CanTpLoopbackTestResult.stage = CANTP_LOOPBACK_STAGE_DONE;
    g_CanTpLoopbackTestResult.status = CANTP_LOOPBACK_PASS;
    return E_OK;
}