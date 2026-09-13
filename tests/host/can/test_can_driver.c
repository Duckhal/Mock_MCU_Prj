#include "Can.h"
#include "Can_Internal.h"
#include "S32K144.h"

#include <stdio.h>
#include <string.h>

#define TEST_TX_HOH              (0U)
#define TEST_RX_HOH              (1U)
#define TEST_CAN_ID              (0x123U)
#define TEST_TX_MB               (0U)
#define TEST_RX_MB               (1U)
#define TEST_MB_WORD_COUNT       (4U)
#define TEST_MB_CS_DLC_SHIFT     (16U)
#define TEST_STANDARD_ID_SHIFT   (18U)
#define TEST_MB_CODE_RX_EMPTY    (0x04UL)
#define TEST_MB_CODE_TX_INACTIVE (0x08UL)
#define TEST_MB_CS_CODE_SHIFT    (24U)

CAN_Type g_fakeCan0;
PCC_Type g_fakePcc;

static uint32_t s_failures;
static uint32_t s_txCallbackCount;
static Can_SwPduHandleType s_lastTxHandle;
static Can_ReturnType s_lastTxResult;
static uint32_t s_rxCallbackCount;
static Can_HwType s_lastMailbox;
static uint8_t s_lastRxData[CAN_CLASSIC_MAX_DLC];
static uint8_t s_lastRxLength;
static uint32_t s_busOffCallbackCount;

#define TEST_CHECK(condition)                                                \
    do                                                                       \
    {                                                                        \
        if (!(condition))                                                    \
        {                                                                    \
            printf("FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition);     \
            s_failures++;                                                    \
        }                                                                    \
    } while (0)

/** Capture Tx confirmation values so lifecycle assertions can inspect them. */
static void Test_TxConfirmation(Can_SwPduHandleType swPduHandle,
                                Can_ReturnType result)
{
    s_txCallbackCount++;
    s_lastTxHandle = swPduHandle;
    s_lastTxResult = result;
}

/** Copy callback-owned Rx metadata and payload into persistent test storage. */
static void Test_RxIndication(const Can_HwType *mailbox,
                              const Can_PduType *pduInfo)
{
    s_rxCallbackCount++;
    s_lastMailbox = *mailbox;
    s_lastRxLength = pduInfo->length;
    (void)memcpy(s_lastRxData, pduInfo->sdu, pduInfo->length);
}

/** Verify and count each controller bus-off callback. */
static void Test_BusOff(uint8_t controllerId)
{
    TEST_CHECK(controllerId == 0U);
    s_busOffCallbackCount++;
}

/** Verify payload packing/unpacking byte order, bounds, and null handling. */
static void Test_InternalHelpers(void)
{
    const uint8_t payload[CAN_CLASSIC_MAX_DLC] =
        {0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0xFFU};
    uint8_t unpacked[CAN_CLASSIC_MAX_DLC] = {0U};
    uint32_t word0 = 0U;
    uint32_t word1 = 0U;

    TEST_CHECK(Can_InternalPackPayload(payload, 8U, &word0, &word1) == CAN_OK);
    TEST_CHECK(word0 == 0x00112233UL);
    TEST_CHECK(word1 == 0x445566FFUL);
    TEST_CHECK(Can_InternalUnpackPayload(word0, word1, 8U, unpacked) == CAN_OK);
    TEST_CHECK(memcmp(payload, unpacked, sizeof(payload)) == 0);
    TEST_CHECK(Can_InternalPackPayload(NULL, 0U, &word0, &word1) == CAN_OK);
    TEST_CHECK((word0 == 0U) && (word1 == 0U));
    TEST_CHECK(Can_InternalPackPayload(NULL, 1U, &word0, &word1) ==
               CAN_INVALID_PARAM);
    TEST_CHECK(Can_InternalUnpackPayload(0U, 0U, 9U, unpacked) ==
               CAN_INVALID_PARAM);
}

/** Verify accepted baseline config and representative invalid configurations. */
static void Test_ConfigValidation(void)
{
    Can_HohConfigType hohs[2] =
    {
        {TEST_TX_HOH, 0U, CAN_HOH_TYPE_TX, TEST_TX_MB, 0U, 0U},
        {TEST_RX_HOH, 0U, CAN_HOH_TYPE_RX, TEST_RX_MB,
         TEST_CAN_ID, CAN_STANDARD_ID_MAX}
    };
    Can_ConfigType config = {0U, CAN_SUPPORTED_BAUDRATE, true, 8U, hohs, 2U};

    TEST_CHECK(Can_InternalValidateConfig(&config) == CAN_OK);
    TEST_CHECK(Can_InternalValidateConfig(NULL) == CAN_INVALID_PARAM);

    config.baudrate = 250000U;
    TEST_CHECK(Can_InternalValidateConfig(&config) == CAN_INVALID_PARAM);
    config.baudrate = CAN_SUPPORTED_BAUDRATE;

    config.hardwareTimeoutCount = 0U;
    TEST_CHECK(Can_InternalValidateConfig(&config) == CAN_INVALID_PARAM);
    config.hardwareTimeoutCount = 8U;

    hohs[1].mbIndex = TEST_TX_MB;
    TEST_CHECK(Can_InternalValidateConfig(&config) == CAN_INVALID_PARAM);
    hohs[1].mbIndex = TEST_RX_MB;

    hohs[1].hohId = TEST_TX_HOH;
    TEST_CHECK(Can_InternalValidateConfig(&config) == CAN_INVALID_PARAM);
    hohs[1].hohId = TEST_RX_HOH;

    hohs[1].rxCanId = CAN_STANDARD_ID_MAX + 1U;
    TEST_CHECK(Can_InternalValidateConfig(&config) == CAN_INVALID_PARAM);
}

/** Exercise init, start, Tx/Rx polling, busy handling, and bus-off behavior. */
static void Test_DriverLifecycle(void)
{
    static const Can_HohConfigType hohs[2] =
    {
        {TEST_TX_HOH, 0U, CAN_HOH_TYPE_TX, TEST_TX_MB, 0U, 0U},
        {TEST_RX_HOH, 0U, CAN_HOH_TYPE_RX, TEST_RX_MB,
         TEST_CAN_ID, CAN_STANDARD_ID_MAX}
    };
    static const Can_ConfigType config =
        {0U, CAN_SUPPORTED_BAUDRATE, true, 8U, hohs, 2U};
    static const Can_CallbacksType callbacks =
        {Test_TxConfirmation, Test_RxIndication, Test_BusOff};
    const uint8_t txData[4] = {0xDEU, 0xADU, 0xBEU, 0xEFU};
    const Can_PduType pdu = {TEST_CAN_ID, 77U, 4U, txData};
    Can_ControllerStatusType status;
    Can_StatsType stats;

    (void)memset(&g_fakeCan0, 0, sizeof(g_fakeCan0));
    (void)memset(&g_fakePcc, 0, sizeof(g_fakePcc));

    g_fakeCan0.MCR = CAN_MCR_FRZACK_MASK;
    TEST_CHECK(Can_Init(&config) == CAN_INVALID_STATE);

    g_fakePcc.PCCn[PCC_FlexCAN0_INDEX] = PCC_PCCn_CGC_MASK;
    TEST_CHECK(Can_Init(&config) == CAN_OK);
    TEST_CHECK(Can_GetControllerStatus(0U, &status) == CAN_OK);
    TEST_CHECK(status.mode == CAN_CONTROLLER_STOPPED);
    TEST_CHECK((g_fakeCan0.CTRL1 & CAN_CTRL1_LPB_MASK) != 0U);
    TEST_CHECK((g_fakeCan0.RAMn[TEST_TX_MB * TEST_MB_WORD_COUNT] >>
                TEST_MB_CS_CODE_SHIFT) == TEST_MB_CODE_TX_INACTIVE);
    TEST_CHECK((g_fakeCan0.RAMn[TEST_RX_MB * TEST_MB_WORD_COUNT] >>
                TEST_MB_CS_CODE_SHIFT) == TEST_MB_CODE_RX_EMPTY);

    g_fakeCan0.MCR &= ~CAN_MCR_FRZACK_MASK;
    TEST_CHECK(Can_SetControllerMode(0U, CAN_CONTROLLER_STARTED) ==
               CAN_INVALID_STATE);
    TEST_CHECK(Can_RegisterCallbacks(&callbacks) == CAN_OK);
    TEST_CHECK(Can_SetControllerMode(0U, CAN_CONTROLLER_STARTED) == CAN_OK);

    TEST_CHECK(Can_Write(TEST_TX_HOH, NULL) == CAN_INVALID_PARAM);
    TEST_CHECK(Can_Write(99U, &pdu) == CAN_INVALID_PARAM);
    TEST_CHECK(Can_Write(TEST_TX_HOH, &pdu) == CAN_OK);
    g_fakeCan0.IFLAG1 = 0U; /* Fake write-one-to-clear side effect. */
    TEST_CHECK(g_fakeCan0.RAMn[1U] == (TEST_CAN_ID << TEST_STANDARD_ID_SHIFT));
    TEST_CHECK(g_fakeCan0.RAMn[2U] == 0xDEADBEEFUL);
    TEST_CHECK(Can_Write(TEST_TX_HOH, &pdu) == CAN_BUSY);

    /* FlexCAN hardware returns a transmitted mailbox to TX_INACTIVE before
     * raising the completion flag.  Model that transition in the register
     * fake so the next Can_Write() sees an available mailbox. */
    g_fakeCan0.RAMn[TEST_TX_MB * TEST_MB_WORD_COUNT] =
        TEST_MB_CODE_TX_INACTIVE << TEST_MB_CS_CODE_SHIFT;
    g_fakeCan0.IFLAG1 = 1UL << TEST_TX_MB;
    Can_MainFunction_Write();
    g_fakeCan0.IFLAG1 = 0U;
    TEST_CHECK(s_txCallbackCount == 1U);
    TEST_CHECK(s_lastTxHandle == 77U);
    TEST_CHECK(s_lastTxResult == CAN_OK);

    g_fakeCan0.RAMn[TEST_RX_MB * TEST_MB_WORD_COUNT] =
        (TEST_MB_CODE_RX_EMPTY << TEST_MB_CS_CODE_SHIFT) |
        (4UL << TEST_MB_CS_DLC_SHIFT);
    g_fakeCan0.RAMn[(TEST_RX_MB * TEST_MB_WORD_COUNT) + 1U] =
        TEST_CAN_ID << TEST_STANDARD_ID_SHIFT;
    g_fakeCan0.RAMn[(TEST_RX_MB * TEST_MB_WORD_COUNT) + 2U] = 0xDEADBEEFUL;
    g_fakeCan0.RAMn[(TEST_RX_MB * TEST_MB_WORD_COUNT) + 3U] = 0U;
    g_fakeCan0.IFLAG1 = 1UL << TEST_RX_MB;
    Can_MainFunction_Read();
    g_fakeCan0.IFLAG1 = 0U;
    TEST_CHECK(s_rxCallbackCount == 1U);
    TEST_CHECK(s_lastMailbox.canId == TEST_CAN_ID);
    TEST_CHECK(s_lastMailbox.hohId == TEST_RX_HOH);
    TEST_CHECK(s_lastRxLength == 4U);
    TEST_CHECK(memcmp(s_lastRxData, txData, sizeof(txData)) == 0);

    TEST_CHECK(Can_Write(TEST_TX_HOH, &pdu) == CAN_OK);
    g_fakeCan0.IFLAG1 = 0U;
    g_fakeCan0.MCR |= CAN_MCR_FRZACK_MASK;
    g_fakeCan0.ESR1 = CAN_ESR1_BOFFINT_MASK |
                      (2UL << CAN_ESR1_FLTCONF_SHIFT);
    Can_MainFunction_Error();
    TEST_CHECK(s_busOffCallbackCount == 1U);
    Can_MainFunction_Write();
    g_fakeCan0.IFLAG1 = 0U;
    TEST_CHECK(s_txCallbackCount == 2U);
    TEST_CHECK(s_lastTxResult == CAN_BUS_OFF);
    TEST_CHECK(Can_Write(TEST_TX_HOH, &pdu) == CAN_BUS_OFF);

    TEST_CHECK(Can_GetStats(&stats) == CAN_OK);
    TEST_CHECK(stats.txAccepted == 2U);
    TEST_CHECK(stats.txCompleted == 1U);
    TEST_CHECK(stats.txFailed == 1U);
    TEST_CHECK(stats.txBusy == 1U);
    TEST_CHECK(stats.rxDelivered == 1U);
    TEST_CHECK(stats.busOffCount == 1U);
}

/** Run all deterministic host tests and return a process failure on any check. */
int main(void)
{
    Test_InternalHelpers();
    Test_ConfigValidation();
    Test_DriverLifecycle();

    if (s_failures != 0U)
    {
        printf("CAN host tests failed: %lu\n", (unsigned long)s_failures);
        return 1;
    }

    printf("CAN host tests passed\n");
    return 0;
}
