#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../drivers/can/cantp/Cantp.h"
#include "../../drivers/can/cantp/Cantp_Cfg.h"
#include "../../drivers/can/canif/CanIf.h"

#define TEST_MAX_FRAMES (32U)

typedef struct
{
    PduIdType id;
    uint8_t bytes[CANTP_FRAME_LENGTH];
    uint32_t tick;
} Test_FrameType;

static uint8_t Test_Source[CANTP_MAX_NSDU_LENGTH];
static Test_FrameType Test_Frames[TEST_MAX_FRAMES];
static uint8_t Test_FrameCount;
static Std_ReturnType Test_TransmitResults[TEST_MAX_FRAMES];
static uint8_t Test_TransmitResultCount;
static uint8_t Test_TransmitResultIndex;
static uint32_t Test_Tick;
static uint32_t Test_CopyTxCalls;
static uint32_t Test_TxFinalCalls;
static Std_ReturnType Test_TxFinalResult;
static uint32_t Test_StartRxCalls;
static uint32_t Test_CopyRxCalls;
static uint32_t Test_RxFinalCalls;
static Std_ReturnType Test_RxFinalResult;
static uint8_t Test_RxData[CANTP_MAX_NSDU_LENGTH];
static PduLengthType Test_RxLength;
static uint8_t Test_RxReserved;
static uint8_t Test_RxReady;
static uint8_t Test_InjectedCtsCount;

/** Print one eight-byte frame in a stable machine-readable evidence format. */
static void Test_PrintFrame(const char *TestId, const char *Direction,
                            uint8_t Index, const Test_FrameType *Frame)
{
    uint8_t byte;
    printf("EVIDENCE %s %s index=%u tick=%lu lpdu=%u bytes=",
           TestId, Direction, (unsigned)Index,
           (unsigned long)Frame->tick, (unsigned)Frame->id);
    for (byte = 0U; byte < CANTP_FRAME_LENGTH; byte++)
    {
        printf("%02X%s", Frame->bytes[byte],
               (byte + 1U < CANTP_FRAME_LENGTH) ? " " : "");
    }
    putchar('\n');
}

/** Return the most recent committed Tx offset retained by the CanTp log. */
static PduLengthType Test_LastConfirmedOffset(void)
{
    uint32_t sequence;
    PduLengthType offset = 0U;
    uint32_t first = (CanTp_LogSequence > CANTP_LOG_CAPACITY) ?
        (CanTp_LogSequence - CANTP_LOG_CAPACITY) : 0U;
    for (sequence = first; sequence < CanTp_LogSequence; sequence++)
    {
        const volatile CanTp_LogRecordType *record =
            &CanTp_LogRecords[sequence % CANTP_LOG_CAPACITY];
        if ((record->sequence == sequence) &&
            (record->event == CANTP_LOG_TX_FRAME_CONFIRMATION))
        {
            offset = (PduLengthType)record->detail;
        }
    }
    return offset;
}

/** Print the sender frame/timing/callback evidence retained by the fixture. */
static void Test_PrintTxEvidence(const char *TestId)
{
    uint8_t index;
    uint8_t confirmation = 0U;
    uint32_t sequence;
    uint32_t first = (CanTp_LogSequence > CANTP_LOG_CAPACITY) ?
        (CanTp_LogSequence - CANTP_LOG_CAPACITY) : 0U;
    printf("EVIDENCE %s TX_SUMMARY frames=%u injected_cts=%u copy_tx=%lu "
           "tx_final_count=%lu tx_final_result=%u committed_offset=%u\n",
           TestId, (unsigned)Test_FrameCount,
           (unsigned)Test_InjectedCtsCount,
           (unsigned long)Test_CopyTxCalls,
           (unsigned long)Test_TxFinalCalls,
           (unsigned)Test_TxFinalResult,
           (unsigned)Test_LastConfirmedOffset());
    for (index = 0U; index < Test_FrameCount; index++)
    {
        Test_PrintFrame(TestId, "DATA_TX", index, &Test_Frames[index]);
    }
    for (sequence = first; sequence < CanTp_LogSequence; sequence++)
    {
        const volatile CanTp_LogRecordType *record =
            &CanTp_LogRecords[sequence % CANTP_LOG_CAPACITY];
        if ((record->sequence == sequence) &&
            (record->event == CANTP_LOG_TX_FRAME_CONFIRMATION))
        {
            confirmation++;
            printf("EVIDENCE %s TX_CONFIRM index=%u committed_offset=%lu\n",
                   TestId, (unsigned)confirmation,
                   (unsigned long)record->detail);
        }
    }
}

/** Capture each immutable Data or FC frame accepted by the fake CanIf. */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId,
                              const PduInfoType *PduInfoPtr)
{
    Test_FrameType *frame;
    assert(PduInfoPtr != NULL && PduInfoPtr->SduDataPtr != NULL);
    assert(PduInfoPtr->SduLength == CANTP_FRAME_LENGTH);
    assert(Test_FrameCount < TEST_MAX_FRAMES);
    frame = &Test_Frames[Test_FrameCount++];
    frame->id = TxPduId;
    frame->tick = Test_Tick;
    memcpy(frame->bytes, PduInfoPtr->SduDataPtr, CANTP_FRAME_LENGTH);
    if (Test_TransmitResultIndex < Test_TransmitResultCount)
    {
        return Test_TransmitResults[Test_TransmitResultIndex++];
    }
    return E_OK;
}

/** Provide exactly one Tx snapshot for an accepted N-SDU. */
BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType TxNSduId,
                                       uint8_t *DestPtr,
                                       PduLengthType Length)
{
    assert(TxNSduId == CANTP_TX_NSDU && DestPtr != NULL);
    memcpy(DestPtr, Test_Source, Length);
    Test_CopyTxCalls++;
    return BUFREQ_OK;
}

/** Capture the one final sender result. */
void PduR_CanTpTxConfirmation(PduIdType TxNSduId,
                               Std_ReturnType Result)
{
    assert(TxNSduId == CANTP_TX_NSDU);
    Test_TxFinalCalls++;
    Test_TxFinalResult = Result;
}

/** Reserve the single queue slot used by this isolated CanTp test. */
BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType RxNSduId,
                                              PduLengthType TotalLength)
{
    assert(RxNSduId == CANTP_RX_NSDU);
    assert(TotalLength >= 1U && TotalLength <= CANTP_MAX_NSDU_LENGTH);
    assert(Test_RxReserved == 0U);
    Test_RxReserved = 1U;
    Test_StartRxCalls++;
    Test_RxLength = TotalLength;
    return BUFREQ_OK;
}

/** Copy only a complete reassembled N-SDU into the fake application queue. */
BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType RxNSduId,
                                        const uint8_t *DataPtr,
                                        PduLengthType Length)
{
    assert(RxNSduId == CANTP_RX_NSDU && DataPtr != NULL);
    assert(Length == Test_RxLength);
    memcpy(Test_RxData, DataPtr, Length);
    Test_CopyRxCalls++;
    return BUFREQ_OK;
}

/** Capture the one final receiver result after the complete copy. */
void PduR_CanTpRxIndication(PduIdType RxNSduId,
                             Std_ReturnType Result)
{
    assert(RxNSduId == CANTP_RX_NSDU);
    assert(Test_CopyRxCalls == ((Result == E_OK) ? 1U : 0U));
    assert(Test_RxReserved != 0U);
    Test_RxReserved = 0U;
    Test_RxReady = (uint8_t)((Result == E_OK) ? 1U : 0U);
    Test_RxFinalCalls++;
    Test_RxFinalResult = Result;
}

/** Reset all fixture observations and initialize the real CanTp module. */
static void Test_Reset(void)
{
    memset(Test_Source, 0, sizeof(Test_Source));
    memset(Test_Frames, 0, sizeof(Test_Frames));
    memset(Test_RxData, 0, sizeof(Test_RxData));
    Test_FrameCount = 0U;
    Test_TransmitResultCount = 0U;
    Test_TransmitResultIndex = 0U;
    memset(Test_TransmitResults, 0, sizeof(Test_TransmitResults));
    Test_Tick = 0U;
    Test_CopyTxCalls = 0U;
    Test_TxFinalCalls = 0U;
    Test_TxFinalResult = E_NOT_OK;
    Test_StartRxCalls = 0U;
    Test_CopyRxCalls = 0U;
    Test_RxFinalCalls = 0U;
    Test_RxFinalResult = E_NOT_OK;
    Test_RxLength = 0U;
    Test_RxReserved = 0U;
    Test_RxReady = 0U;
    Test_InjectedCtsCount = 0U;
    assert(CanTp_Init() == E_OK);
}

/** Feed the fixed valid CTS frame to the sender. */
static void Test_InjectCts(void)
{
    uint8_t bytes[8] = {0x30U, 0x04U, 0x05U, 0U, 0U, 0U, 0U, 0U};
    PduInfoType frame = {bytes, sizeof(bytes)};
    Test_InjectedCtsCount++;
    CanTp_RxIndication(CANTP_RX_NPDU_FC, &frame);
}

/** Run a complete sender session and locally confirm every accepted frame. */
static uint8_t Test_RunTx(PduLengthType Length)
{
    PduInfoType request = {Test_Source, Length};
    uint8_t processed = 0U;
    uint8_t dataFrames = 0U;
    uint8_t cfInBlock = 0U;
    PduLengthType confirmedBytes = 0U;
    uint32_t guard;

    assert(CanTp_Transmit(CANTP_TX_NSDU, &request) == E_OK);
    assert(Test_CopyTxCalls == 1U && Test_FrameCount == 0U);
    for (guard = 0U; (guard < 200U) && (Test_TxFinalCalls == 0U); guard++)
    {
        Test_Tick++;
        CanTp_MainFunction();
        while (processed < Test_FrameCount)
        {
            const Test_FrameType *frame = &Test_Frames[processed++];
            assert(frame->id == CANTP_CANIF_TX_LPDU_DATA);
            dataFrames++;
            if (frame->bytes[0] == CANTP_PCI_SF)
            {
                confirmedBytes = Length;
            }
            else if (frame->bytes[0] == CANTP_PCI_FF)
            {
                confirmedBytes = CANTP_FF_PAYLOAD_LENGTH;
                cfInBlock = 0U;
            }
            else
            {
                PduLengthType left = (PduLengthType)(Length - confirmedBytes);
                PduLengthType bytes = (left < CANTP_CF_PAYLOAD_LENGTH) ?
                    left : CANTP_CF_PAYLOAD_LENGTH;
                confirmedBytes = (PduLengthType)(confirmedBytes + bytes);
                cfInBlock++;
            }

            CanTp_TxConfirmation(CANTP_TX_NPDU_DATA);
            if ((confirmedBytes < Length) &&
                ((frame->bytes[0] == CANTP_PCI_FF) ||
                 (cfInBlock == CANTP_BLOCK_SIZE)))
            {
                Test_InjectCts();
                cfInBlock = 0U;
            }
        }
    }
    assert(guard < 200U);
    assert(Test_TxFinalCalls == 1U && Test_TxFinalResult == E_OK);
    return dataFrames;
}

/** Check every byte and STmin boundary for T01, T02 and T03 sender vectors. */
static void Test_TxVectors(void)
{
    uint8_t index;
    Test_Reset();
    for (index = 0U; index < 5U; index++) { Test_Source[index] = index; }
    assert(Test_RunTx(5U) == 1U);
    {
        const uint8_t expected[8] = {0x00U, 0x05U, 0x00U, 0x01U,
                                     0x02U, 0x03U, 0x04U, 0x00U};
        assert(memcmp(Test_Frames[0].bytes, expected, sizeof(expected)) == 0);
    }
    Test_PrintTxEvidence("T01");

    Test_Reset();
    for (index = 0U; index < 20U; index++) { Test_Source[index] = index; }
    assert(Test_RunTx(20U) == 3U);
    {
        const uint8_t expected[3][8] =
        {
            {0x10U, 0x14U, 0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U},
            {0x21U, 0x06U, 0x07U, 0x08U, 0x09U, 0x0AU, 0x0BU, 0x0CU},
            {0x22U, 0x0DU, 0x0EU, 0x0FU, 0x10U, 0x11U, 0x12U, 0x13U}
        };
        for (index = 0U; index < 3U; index++)
        {
            assert(memcmp(Test_Frames[index].bytes, expected[index], 8U) == 0);
        }
        assert((Test_Frames[2].tick - Test_Frames[1].tick) >=
               CANTP_STMIN_MS);
    }
    Test_PrintTxEvidence("T02");

    Test_Reset();
    for (index = 0U; index < 62U; index++) { Test_Source[index] = index; }
    assert(Test_RunTx(62U) == 9U);
    assert(Test_Frames[0].bytes[0] == 0x10U &&
           Test_Frames[0].bytes[1] == 0x3EU);
    for (index = 1U; index < 9U; index++)
    {
        uint8_t byte;
        assert(Test_Frames[index].bytes[0] == (uint8_t)(0x20U | index));
        for (byte = 1U; byte < 8U; byte++)
        {
            uint8_t expected = (uint8_t)(6U + ((index - 1U) * 7U) +
                                         (byte - 1U));
            assert(Test_Frames[index].bytes[byte] == expected);
        }
        if (index > 1U)
        {
            assert((Test_Frames[index].tick -
                    Test_Frames[index - 1U].tick) >= CANTP_STMIN_MS);
        }
    }
    assert(Test_Frames[8].bytes[7] == 0x3DU);
    Test_PrintTxEvidence("T03");
    printf("EVIDENCE T04 CTS_COUNT=%u\n",
           (unsigned)Test_InjectedCtsCount);
    for (index = 1U; index < 9U; index++)
    {
        uint32_t delta = (index == 1U) ? 0U :
            (Test_Frames[index].tick - Test_Frames[index - 1U].tick);
        printf("EVIDENCE T04 CF%u request_tick=%lu delta_from_prior_cf=%lu\n",
               (unsigned)index,
               (unsigned long)Test_Frames[index].tick,
               (unsigned long)delta);
    }
}

/** Confirm each generated CTS before injecting the next CF block. */
static void Test_ConfirmNewFcFrames(uint8_t *Processed)
{
    Test_Tick++;
    CanTp_MainFunction();
    while (*Processed < Test_FrameCount)
    {
        const uint8_t expected[8] =
            {0x30U, 0x04U, 0x05U, 0U, 0U, 0U, 0U, 0U};
        assert(Test_Frames[*Processed].id == CANTP_CANIF_TX_LPDU_FC);
        assert(memcmp(Test_Frames[*Processed].bytes, expected, 8U) == 0);
        (*Processed)++;
        CanTp_TxConfirmation(CANTP_TX_NPDU_FC);
    }
}

/** Inject one complete receive vector and verify one final full-message copy. */
static void Test_RunRx(const char *TestId,
                       const uint8_t Frames[][8], uint8_t FrameCount,
                       const uint8_t *Expected, PduLengthType Length,
                       uint8_t ExpectedFcCount)
{
    uint8_t index;
    uint8_t processedFc = 0U;
    Test_Reset();
    for (index = 0U; index < FrameCount; index++)
    {
        PduInfoType frame = {(uint8_t *)Frames[index], CANTP_FRAME_LENGTH};
        CanTp_RxIndication(CANTP_RX_NPDU_DATA, &frame);
        Test_ConfirmNewFcFrames(&processedFc);
        if (index + 1U < FrameCount)
        {
            assert(Test_CopyRxCalls == 0U && Test_RxFinalCalls == 0U);
        }
    }
    assert(processedFc == ExpectedFcCount);
    assert(Test_StartRxCalls == 1U && Test_CopyRxCalls == 1U);
    assert(Test_RxFinalCalls == 1U && Test_RxFinalResult == E_OK);
    assert(Test_RxLength == Length);
    assert(memcmp(Test_RxData, Expected, Length) == 0);
    assert(Test_RxReady != 0U);
    printf("EVIDENCE %s RX_SUMMARY data_frames=%u fc_frames=%u "
           "start_rx=%lu copy_rx=%lu rx_final_count=%lu rx_final_result=%u "
           "length=%u reservation=%u ready=%u payload_match=1\n",
           TestId, (unsigned)FrameCount, (unsigned)processedFc,
           (unsigned long)Test_StartRxCalls,
           (unsigned long)Test_CopyRxCalls,
           (unsigned long)Test_RxFinalCalls,
           (unsigned)Test_RxFinalResult, (unsigned)Test_RxLength,
           (unsigned)Test_RxReserved, (unsigned)Test_RxReady);
    for (index = 0U; index < processedFc; index++)
    {
        Test_PrintFrame(TestId, "FC_TX", index, &Test_Frames[index]);
    }
    printf("EVIDENCE %s RX_PAYLOAD first=%02X last=%02X\n",
           TestId, Test_RxData[0], Test_RxData[Length - 1U]);
}

/** Check reassembly, block CTS count and final padding handling for T01-T03. */
static void Test_RxVectors(void)
{
    uint8_t expected62[62];
    uint8_t index;
    static const uint8_t sf[1][8] =
    {
        {0x00U, 0x05U, 0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x00U}
    };
    static const uint8_t ff20[3][8] =
    {
        {0x10U, 0x14U, 0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U},
        {0x21U, 0x06U, 0x07U, 0x08U, 0x09U, 0x0AU, 0x0BU, 0x0CU},
        {0x22U, 0x0DU, 0x0EU, 0x0FU, 0x10U, 0x11U, 0x12U, 0x13U}
    };
    static const uint8_t ff62[9][8] =
    {
        {0x10U, 0x3EU, 0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U},
        {0x21U, 0x06U, 0x07U, 0x08U, 0x09U, 0x0AU, 0x0BU, 0x0CU},
        {0x22U, 0x0DU, 0x0EU, 0x0FU, 0x10U, 0x11U, 0x12U, 0x13U},
        {0x23U, 0x14U, 0x15U, 0x16U, 0x17U, 0x18U, 0x19U, 0x1AU},
        {0x24U, 0x1BU, 0x1CU, 0x1DU, 0x1EU, 0x1FU, 0x20U, 0x21U},
        {0x25U, 0x22U, 0x23U, 0x24U, 0x25U, 0x26U, 0x27U, 0x28U},
        {0x26U, 0x29U, 0x2AU, 0x2BU, 0x2CU, 0x2DU, 0x2EU, 0x2FU},
        {0x27U, 0x30U, 0x31U, 0x32U, 0x33U, 0x34U, 0x35U, 0x36U},
        {0x28U, 0x37U, 0x38U, 0x39U, 0x3AU, 0x3BU, 0x3CU, 0x3DU}
    };
    static const uint8_t expected5[5] = {0U, 1U, 2U, 3U, 4U};
    static const uint8_t expected20[20] =
        {0U,1U,2U,3U,4U,5U,6U,7U,8U,9U,10U,11U,12U,13U,14U,15U,16U,17U,18U,19U};

    for (index = 0U; index < 62U; index++) { expected62[index] = index; }
    Test_RunRx("T01", sf, 1U, expected5, 5U, 0U);
    Test_RunRx("T02", ff20, 3U, expected20, 20U, 1U);
    Test_RunRx("T03", ff62, 9U, expected62, 62U, 2U);
}

int main(void)
{
    Test_TxVectors();
    Test_RxVectors();
    puts("PASS: CanTp Phase 1 T01-T03 exact Tx/Rx vectors, CTS blocks and STmin.");
    return 0;
}
