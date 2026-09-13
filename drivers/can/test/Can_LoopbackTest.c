#include "Can_LoopbackTest.h"

#include "../config/can/Can_Cfg.h"
#include "../driver/Can.h"
#include "../../../bsp/LED.h"
#include "../../../bsp/can/board_can.h"

#include <stdbool.h>
#include <stdint.h>

#define CAN_LOOPBACK_TEST_CAN_ID          (0x123U)
#define CAN_LOOPBACK_TEST_SW_PDU_HANDLE   (1U)
#define CAN_LOOPBACK_TEST_POLL_LIMIT      (1000000UL)
#define CAN_LOOPBACK_TEST_DATA_LENGTH     (4U)

volatile Can_LoopbackTestResultType g_CanLoopbackTestResult =
    CAN_LOOPBACK_TEST_NOT_RUN;

static volatile bool s_txComplete;
static volatile bool s_rxComplete;
static volatile bool s_busOff;
static volatile Can_ReturnType s_txResult;
static uint8_t s_rxData[CAN_CLASSIC_MAX_DLC];
static uint8_t s_rxLength;
static Can_IdType s_rxCanId;

/** Record the terminal result for the loopback frame under test. */
static void Can_LoopbackTest_TxConfirmation(Can_SwPduHandleType swPduHandle,
                                             Can_ReturnType result)
{
    if (swPduHandle == CAN_LOOPBACK_TEST_SW_PDU_HANDLE)
    {
        s_txResult = result;
        s_txComplete = true;
    }
}

/** Validate and copy the received frame before the callback payload expires. */
static void Can_LoopbackTest_RxIndication(const Can_HwType *mailbox,
                                           const Can_PduType *pduInfo)
{
    uint8_t index;

    if ((mailbox == (const Can_HwType *)0) ||
        (pduInfo == (const Can_PduType *)0) ||
        (pduInfo->length > CAN_CLASSIC_MAX_DLC) ||
        ((pduInfo->length > 0U) && (pduInfo->sdu == (const uint8_t *)0)))
    {
        return;
    }

    s_rxCanId = pduInfo->id;
    s_rxLength = pduInfo->length;
    for (index = 0U; index < pduInfo->length; index++)
    {
        s_rxData[index] = pduInfo->sdu[index];
    }
    s_rxComplete = true;
}

/** Latch an unexpected CAN0 bus-off event so the test can fail deterministically. */
static void Can_LoopbackTest_ControllerBusOff(uint8_t controllerId)
{
    if (controllerId == CAN_CONTROLLER_0)
    {
        s_busOff = true;
    }
}

/** Compare received ID, DLC, and payload against the loopback test vector. */
static bool Can_LoopbackTest_DataMatches(const uint8_t *expected,
                                          uint8_t expectedLength)
{
    uint8_t index;

    if ((s_rxCanId != CAN_LOOPBACK_TEST_CAN_ID) ||
        (s_rxLength != expectedLength))
    {
        return false;
    }

    for (index = 0U; index < expectedLength; index++)
    {
        if (s_rxData[index] != expected[index])
        {
            return false;
        }
    }

    return true;
}

/**
 * Initialize BSP and CAN0 internal loopback, transmit one deterministic frame,
 * and expose the terminal result through g_CanLoopbackTestResult and green LED.
 */
void Can_LoopbackTest_Run(void)
{
    static const uint8_t testData[CAN_LOOPBACK_TEST_DATA_LENGTH] =
    {
        0xDEU, 0xADU, 0xBEU, 0xEFU
    };
    static const Can_CallbacksType callbacks =
    {
        Can_LoopbackTest_TxConfirmation,
        Can_LoopbackTest_RxIndication,
        Can_LoopbackTest_ControllerBusOff
    };
    Can_PduType pdu;
    uint32_t pollsRemaining = CAN_LOOPBACK_TEST_POLL_LIMIT;

    g_CanLoopbackTestResult = CAN_LOOPBACK_TEST_RUNNING;
    s_txComplete = false;
    s_rxComplete = false;
    s_busOff = false;
    s_txResult = CAN_NOT_OK;
    s_rxLength = 0U;
    s_rxCanId = 0U;

    disable_WDOG();
    init_MCU();

    if (Can_Init(&Can_Config_Loopback) != CAN_OK)
    {
        g_CanLoopbackTestResult = CAN_LOOPBACK_TEST_FAILED_INIT;
        return;
    }

    if (Can_RegisterCallbacks(&callbacks) != CAN_OK)
    {
        g_CanLoopbackTestResult = CAN_LOOPBACK_TEST_FAILED_CALLBACKS;
        return;
    }

    if (Can_SetControllerMode(CAN_CONTROLLER_0,
                              CAN_CONTROLLER_STARTED) != CAN_OK)
    {
        g_CanLoopbackTestResult = CAN_LOOPBACK_TEST_FAILED_START;
        return;
    }

    pdu.id = CAN_LOOPBACK_TEST_CAN_ID;
    pdu.swPduHandle = CAN_LOOPBACK_TEST_SW_PDU_HANDLE;
    pdu.length = CAN_LOOPBACK_TEST_DATA_LENGTH;
    pdu.sdu = testData;

    if (Can_Write(CAN_HTH_0, &pdu) != CAN_OK)
    {
        g_CanLoopbackTestResult = CAN_LOOPBACK_TEST_FAILED_WRITE;
        return;
    }

    while ((!s_txComplete || !s_rxComplete) &&
           !s_busOff &&
           (pollsRemaining > 0U))
    {
        Can_MainFunction_Error();
        Can_MainFunction_Write();
        Can_MainFunction_Read();
        pollsRemaining--;
    }

    if (s_busOff)
    {
        g_CanLoopbackTestResult = CAN_LOOPBACK_TEST_FAILED_BUS_OFF;
        return;
    }

    if (pollsRemaining == 0U)
    {
        g_CanLoopbackTestResult = CAN_LOOPBACK_TEST_FAILED_TIMEOUT;
        return;
    }

    if ((s_txResult != CAN_OK) ||
        !Can_LoopbackTest_DataMatches(testData,
                                      CAN_LOOPBACK_TEST_DATA_LENGTH))
    {
        g_CanLoopbackTestResult = CAN_LOOPBACK_TEST_FAILED_DATA;
        return;
    }

    g_CanLoopbackTestResult = CAN_LOOPBACK_TEST_PASSED;
    LED_On(LED_GREEN);
}
