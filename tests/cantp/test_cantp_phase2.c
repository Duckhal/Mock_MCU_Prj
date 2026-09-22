#include <assert.h>
#include <stdio.h>
#include <string.h>

#define main Test_Phase1RegressionMain
#include "test_cantp_phase1.c"
#undef main

/** Install one deterministic result for each following fake CanIf request. */
static void Test_SetTransmitResults(const Std_ReturnType *Results,
                                    uint8_t Count)
{
    assert(Results != NULL && Count <= TEST_MAX_FRAMES);
    memcpy(Test_TransmitResults, Results,
           (size_t)Count * sizeof(Test_TransmitResults[0]));
    Test_TransmitResultCount = Count;
    Test_TransmitResultIndex = 0U;
}

/** Advance both the fixture timestamp and the real CanTp 1 ms main function. */
static void Test_Advance(uint32_t Ticks)
{
    uint32_t index;
    for (index = 0U; index < Ticks; index++)
    {
        Test_Tick++;
        CanTp_MainFunction();
    }
}

/** Start a 62-byte sender and confirm FF, CF1 and CF2. */
static void Test_ReachCf3(void)
{
    PduInfoType request = {Test_Source, CANTP_MAX_NSDU_LENGTH};
    uint8_t index;
    for (index = 0U; index < CANTP_MAX_NSDU_LENGTH; index++)
    {
        Test_Source[index] = index;
    }
    assert(CanTp_Transmit(CANTP_TX_NSDU, &request) == E_OK);

    Test_Advance(1U);
    assert(Test_FrameCount == 1U && Test_Frames[0].bytes[0] == CANTP_PCI_FF);
    CanTp_TxConfirmation(CANTP_TX_NPDU_DATA);
    Test_InjectCts();

    Test_Advance(1U);
    assert(Test_FrameCount == 2U && Test_Frames[1].bytes[0] == 0x21U);
    CanTp_TxConfirmation(CANTP_TX_NPDU_DATA);

    Test_Advance(CANTP_STMIN_MS);
    assert(Test_FrameCount == 3U && Test_Frames[2].bytes[0] == 0x22U);
    CanTp_TxConfirmation(CANTP_TX_NPDU_DATA);
}

/** T05: retry the exact same CF3 on consecutive ticks before committing it. */
static void Test_DataRetryThenSuccess(void)
{
    static const Std_ReturnType results[6] =
        {E_OK, E_OK, E_OK, E_NOT_OK, E_NOT_OK, E_OK};
    static const uint8_t expectedCf3[8] =
        {0x23U, 0x14U, 0x15U, 0x16U, 0x17U, 0x18U, 0x19U, 0x1AU};

    Test_Reset();
    Test_SetTransmitResults(results, 6U);
    Test_ReachCf3();
    Test_Advance(CANTP_STMIN_MS);
    assert(Test_FrameCount == 4U);
    Test_Advance(1U);
    Test_Advance(1U);
    assert(Test_FrameCount == 6U);
    assert(memcmp(Test_Frames[3].bytes, expectedCf3, 8U) == 0);
    assert(memcmp(Test_Frames[3].bytes, Test_Frames[4].bytes, 8U) == 0);
    assert(memcmp(Test_Frames[3].bytes, Test_Frames[5].bytes, 8U) == 0);
    assert(Test_Frames[4].tick == Test_Frames[3].tick + 1U);
    assert(Test_Frames[5].tick == Test_Frames[4].tick + 1U);
    assert(Test_TxFinalCalls == 0U);

    CanTp_TxConfirmation(CANTP_TX_NPDU_DATA);
    Test_Advance(CANTP_STMIN_MS);
    assert(Test_FrameCount == 7U && Test_Frames[6].bytes[0] == 0x24U);
}

/** T06: four Data rejections abort the N-SDU once without preparing CF4. */
static void Test_DataRetryExhaustion(void)
{
    static const Std_ReturnType results[7] =
        {E_OK, E_OK, E_OK, E_NOT_OK, E_NOT_OK, E_NOT_OK, E_NOT_OK};
    uint8_t index;

    Test_Reset();
    Test_SetTransmitResults(results, 7U);
    Test_ReachCf3();
    Test_Advance(CANTP_STMIN_MS);
    Test_Advance(3U);
    assert(Test_FrameCount == 7U);
    for (index = 4U; index < 7U; index++)
    {
        assert(memcmp(Test_Frames[3].bytes, Test_Frames[index].bytes, 8U) == 0);
        assert(Test_Frames[index].tick == Test_Frames[index - 1U].tick + 1U);
    }
    assert(Test_TxFinalCalls == 1U && Test_TxFinalResult == E_NOT_OK);
    assert(CanTp_LastTxAbortReason == CANTP_ABORT_DATA_RETRY_EXHAUSTED);
    Test_Advance(10U);
    assert(Test_FrameCount == 7U && Test_TxFinalCalls == 1U);
}

/** T07: N_Bs expires at 100 ms, while a same-tick CTS cancels it. */
static void Test_NBsTimeoutAndBoundaryCts(void)
{
    PduInfoType request = {Test_Source, 20U};

    Test_Reset();
    assert(CanTp_Transmit(CANTP_TX_NSDU, &request) == E_OK);
    Test_Advance(1U);
    CanTp_TxConfirmation(CANTP_TX_NPDU_DATA);
    Test_Advance(CANTP_N_BS_MS - 1U);
    assert(Test_TxFinalCalls == 0U && Test_FrameCount == 1U);
    Test_Advance(1U);
    assert(Test_TxFinalCalls == 1U && Test_TxFinalResult == E_NOT_OK);
    assert(CanTp_LastTxAbortReason == CANTP_ABORT_N_BS_TIMEOUT);

    Test_Reset();
    assert(CanTp_Transmit(CANTP_TX_NSDU, &request) == E_OK);
    Test_Advance(1U);
    CanTp_TxConfirmation(CANTP_TX_NPDU_DATA);
    Test_Advance(CANTP_N_BS_MS - 1U);
    Test_InjectCts();
    Test_Advance(1U);
    assert(Test_TxFinalCalls == 0U);
    assert(Test_FrameCount == 2U && Test_Frames[1].bytes[0] == 0x21U);
}

/** T08: every valid CF restarts N_Cr and timeout releases partial Rx data. */
static void Test_NCrTimeout(void)
{
    uint8_t ff[8] = {0x10U, 0x14U, 0U, 1U, 2U, 3U, 4U, 5U};
    uint8_t cf1[8] = {0x21U, 6U, 7U, 8U, 9U, 10U, 11U, 12U};
    PduInfoType frame = {ff, sizeof(ff)};

    Test_Reset();
    CanTp_RxIndication(CANTP_RX_NPDU_DATA, &frame);
    Test_Advance(1U);
    assert(Test_FrameCount == 1U);
    CanTp_TxConfirmation(CANTP_TX_NPDU_FC);
    Test_Advance(CANTP_N_CR_MS - 1U);
    assert(Test_RxFinalCalls == 0U && Test_RxReserved != 0U);

    frame.SduDataPtr = cf1;
    CanTp_RxIndication(CANTP_RX_NPDU_DATA, &frame);
    Test_Advance(CANTP_N_CR_MS - 1U);
    assert(Test_RxFinalCalls == 0U && Test_CopyRxCalls == 0U);
    Test_Advance(1U);
    assert(Test_RxFinalCalls == 1U && Test_RxFinalResult == E_NOT_OK);
    assert(Test_RxReserved == 0U && Test_CopyRxCalls == 0U);
    assert(CanTp_LastRxAbortReason == CANTP_ABORT_N_CR_TIMEOUT);
}

/** T13: N_As abort retains the lock until one late confirmation clears it. */
static void Test_NAsLateConfirmation(void)
{
    PduInfoType request = {Test_Source, 5U};

    Test_Reset();
    assert(CanTp_Transmit(CANTP_TX_NSDU, &request) == E_OK);
    Test_Advance(1U);
    assert(Test_FrameCount == 1U);
    Test_Advance(CANTP_N_AS_MS - 1U);
    assert(Test_TxFinalCalls == 0U);
    Test_Advance(1U);
    assert(Test_TxFinalCalls == 1U && Test_TxFinalResult == E_NOT_OK);
    assert(CanTp_LastTxAbortReason == CANTP_ABORT_N_AS_TIMEOUT);
    assert(CanTp_Transmit(CANTP_TX_NSDU, &request) == E_NOT_OK);
    assert(Test_TxFinalCalls == 1U);

    CanTp_TxConfirmation(CANTP_TX_NPDU_DATA);
    assert(Test_TxFinalCalls == 1U);
    assert(CanTp_Transmit(CANTP_TX_NSDU, &request) == E_OK);
}

/** Verify FC retry, N_Ar timeout, pending lock and late FC confirmation. */
static void Test_FcRetryAndNAr(void)
{
    static const Std_ReturnType retryResults[3] =
        {E_NOT_OK, E_NOT_OK, E_OK};
    static const Std_ReturnType rejectResults[4] =
        {E_NOT_OK, E_NOT_OK, E_NOT_OK, E_NOT_OK};
    uint8_t ff[8] = {0x10U, 0x14U, 0U, 1U, 2U, 3U, 4U, 5U};
    PduInfoType frame = {ff, sizeof(ff)};

    Test_Reset();
    Test_SetTransmitResults(retryResults, 3U);
    CanTp_RxIndication(CANTP_RX_NPDU_DATA, &frame);
    Test_Advance(3U);
    assert(Test_FrameCount == 3U);
    assert(memcmp(Test_Frames[0].bytes, Test_Frames[1].bytes, 8U) == 0);
    assert(memcmp(Test_Frames[0].bytes, Test_Frames[2].bytes, 8U) == 0);
    assert(Test_Frames[1].tick == Test_Frames[0].tick + 1U);
    assert(Test_Frames[2].tick == Test_Frames[1].tick + 1U);
    CanTp_TxConfirmation(CANTP_TX_NPDU_FC);
    assert(Test_RxFinalCalls == 0U);

    Test_Reset();
    CanTp_RxIndication(CANTP_RX_NPDU_DATA, &frame);
    Test_Advance(1U);
    Test_Advance(CANTP_N_AR_MS - 1U);
    assert(Test_RxFinalCalls == 0U);
    Test_Advance(1U);
    assert(Test_RxFinalCalls == 1U && Test_RxReserved == 0U);
    assert(CanTp_LastRxAbortReason == CANTP_ABORT_N_AR_TIMEOUT);
    CanTp_RxIndication(CANTP_RX_NPDU_DATA, &frame);
    assert(Test_StartRxCalls == 1U);
    CanTp_TxConfirmation(CANTP_TX_NPDU_FC);
    assert(Test_RxFinalCalls == 1U);
    CanTp_RxIndication(CANTP_RX_NPDU_DATA, &frame);
    assert(Test_StartRxCalls == 2U);

    Test_Reset();
    Test_SetTransmitResults(rejectResults, 4U);
    CanTp_RxIndication(CANTP_RX_NPDU_DATA, &frame);
    Test_Advance(4U);
    assert(Test_FrameCount == 4U);
    assert(Test_RxFinalCalls == 1U && Test_RxReserved == 0U);
    assert(CanTp_LastRxAbortReason == CANTP_ABORT_FC_RETRY_EXHAUSTED);
}

int main(void)
{
    Test_TxVectors();
    Test_RxVectors();
    Test_DataRetryThenSuccess();
    Test_DataRetryExhaustion();
    Test_NBsTimeoutAndBoundaryCts();
    Test_NCrTimeout();
    Test_NAsLateConfirmation();
    Test_FcRetryAndNAr();
    puts("PASS: CanTp Phase 2 T04-T08, T13, FC retry/N_Ar and regressions.");
    return 0;
}
