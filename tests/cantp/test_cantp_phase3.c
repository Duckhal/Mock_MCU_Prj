#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../drivers/can/cantp/Cantp.h"
#include "../../drivers/can/cantp/Cantp_Cfg.h"
#include "../../drivers/can/canif/CanIf.h"

#define TEST_MAX_FRAMES (32U)
#define TEST_QUEUE_SIZE (2U)

typedef enum
{
    TEST_SLOT_FREE = 0,
    TEST_SLOT_RESERVED,
    TEST_SLOT_READY
} Test_SlotStateType;

typedef struct
{
    PduIdType id;
    uint8_t bytes[CANTP_FRAME_LENGTH];
    uint32_t tick;
} Test_FrameType;

static Test_FrameType Test_Frames[TEST_MAX_FRAMES];
static uint8_t Test_FrameCount;
static uint32_t Test_Tick;
static uint8_t Test_TxSource[CANTP_MAX_NSDU_LENGTH];
static uint32_t Test_TxFinalCalls;
static Std_ReturnType Test_TxFinalResult;
static Test_SlotStateType Test_SlotState[TEST_QUEUE_SIZE];
static uint8_t Test_SlotData[TEST_QUEUE_SIZE][CANTP_MAX_NSDU_LENGTH];
static PduLengthType Test_SlotLength[TEST_QUEUE_SIZE];
static uint8_t Test_WriteIndex;
static uint8_t Test_ReservedIndex;
static uint8_t Test_ReservationActive;
static uint32_t Test_StartRxCalls;
static uint32_t Test_CopyRxCalls;
static uint32_t Test_RxFinalCalls;
static Std_ReturnType Test_RxFinalResults[4];
static char Test_AppEvents[16];
static uint8_t Test_AppEventCount;

/** Append one application callback event for ordering evidence. */
static void Test_RecordAppEvent(char Event)
{
    assert(Test_AppEventCount < sizeof(Test_AppEvents));
    Test_AppEvents[Test_AppEventCount++] = Event;
}

/** Capture every accepted Data or FC frame with its virtual tick. */
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
    return E_OK;
}

/** Supply the sender snapshot owned by this deterministic fixture. */
BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType TxNSduId,
                                       uint8_t *DestPtr,
                                       PduLengthType Length)
{
    assert(TxNSduId == CANTP_TX_NSDU && DestPtr != NULL);
    memcpy(DestPtr, Test_TxSource, Length);
    return BUFREQ_OK;
}

/** Record one final sender result. */
void PduR_CanTpTxConfirmation(PduIdType TxNSduId,
                              Std_ReturnType Result)
{
    assert(TxNSduId == CANTP_TX_NSDU);
    Test_TxFinalCalls++;
    Test_TxFinalResult = Result;
}

/** Reserve the next FREE queue slot or report a full application queue. */
BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType RxNSduId,
                                             PduLengthType TotalLength)
{
    uint8_t index = Test_WriteIndex;
    assert(RxNSduId == CANTP_RX_NSDU);
    Test_StartRxCalls++;
    Test_RecordAppEvent('S');
    if ((Test_ReservationActive != 0U) ||
        (Test_SlotState[index] != TEST_SLOT_FREE))
    {
        return BUFREQ_E_OVFL;
    }
    Test_SlotState[index] = TEST_SLOT_RESERVED;
    Test_SlotLength[index] = TotalLength;
    Test_ReservedIndex = index;
    Test_ReservationActive = 1U;
    return BUFREQ_OK;
}

/** Copy exactly one complete N-SDU and publish READY before final indication. */
BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType RxNSduId,
                                       const uint8_t *DataPtr,
                                       PduLengthType Length)
{
    uint8_t index = Test_ReservedIndex;
    assert(RxNSduId == CANTP_RX_NSDU && DataPtr != NULL);
    assert(Test_ReservationActive != 0U);
    assert(Test_SlotState[index] == TEST_SLOT_RESERVED);
    assert(Length == Test_SlotLength[index]);
    memcpy(Test_SlotData[index], DataPtr, Length);
    Test_SlotState[index] = TEST_SLOT_READY;
    Test_CopyRxCalls++;
    Test_RecordAppEvent('C');
    return BUFREQ_OK;
}

/** Release a failed reservation or finalize one already READY queue entry. */
void PduR_CanTpRxIndication(PduIdType RxNSduId,
                            Std_ReturnType Result)
{
    uint8_t index = Test_ReservedIndex;
    assert(RxNSduId == CANTP_RX_NSDU);
    assert(Test_ReservationActive != 0U);
    assert(Test_RxFinalCalls < 4U);
    if (Result == E_OK)
    {
        assert(Test_SlotState[index] == TEST_SLOT_READY);
        Test_WriteIndex = (uint8_t)((index + 1U) % TEST_QUEUE_SIZE);
    }
    else
    {
        assert(Test_SlotState[index] == TEST_SLOT_RESERVED);
        Test_SlotState[index] = TEST_SLOT_FREE;
        Test_SlotLength[index] = 0U;
    }
    Test_ReservationActive = 0U;
    Test_RxFinalResults[Test_RxFinalCalls++] = Result;
    Test_RecordAppEvent('I');
}

/** Return the number of READY entries without exposing partial reservations. */
static uint8_t Test_ReadyCount(void)
{
    uint8_t index;
    uint8_t count = 0U;
    for (index = 0U; index < TEST_QUEUE_SIZE; index++)
    {
        if (Test_SlotState[index] == TEST_SLOT_READY)
        {
            count++;
        }
    }
    return count;
}

/** Reset the transport and every fixture observation. */
static void Test_Reset(void)
{
    uint8_t index;
    memset(Test_Frames, 0, sizeof(Test_Frames));
    memset(Test_TxSource, 0, sizeof(Test_TxSource));
    memset(Test_SlotState, 0, sizeof(Test_SlotState));
    memset(Test_SlotData, 0, sizeof(Test_SlotData));
    memset(Test_SlotLength, 0, sizeof(Test_SlotLength));
    memset(Test_RxFinalResults, 0, sizeof(Test_RxFinalResults));
    memset(Test_AppEvents, 0, sizeof(Test_AppEvents));
    Test_FrameCount = 0U;
    Test_Tick = 0U;
    Test_TxFinalCalls = 0U;
    Test_TxFinalResult = E_OK;
    Test_WriteIndex = 0U;
    Test_ReservedIndex = 0U;
    Test_ReservationActive = 0U;
    Test_StartRxCalls = 0U;
    Test_CopyRxCalls = 0U;
    Test_RxFinalCalls = 0U;
    Test_AppEventCount = 0U;
    for (index = 0U; index < CANTP_MAX_NSDU_LENGTH; index++)
    {
        Test_TxSource[index] = index;
    }
    assert(CanTp_Init() == E_OK);
}

/** Advance the real CanTp scheduler by one virtual millisecond. */
static void Test_Step(void)
{
    Test_Tick++;
    CanTp_MainFunction();
}

/** Deliver one complete eight-byte Data N-PDU. */
static void Test_InjectData(uint8_t *Bytes)
{
    PduInfoType frame = {Bytes, CANTP_FRAME_LENGTH};
    CanTp_RxIndication(CANTP_RX_NPDU_DATA, &frame);
}

/** Deliver one complete eight-byte Flow Control N-PDU. */
static void Test_InjectFc(uint8_t *Bytes)
{
    PduInfoType frame = {Bytes, CANTP_FRAME_LENGTH};
    CanTp_RxIndication(CANTP_RX_NPDU_FC, &frame);
}

/** Assert the exact fixed eight-byte overflow Flow Control encoding. */
static void Test_AssertOvflwFrame(const Test_FrameType *Frame)
{
    static const uint8_t expected[CANTP_FRAME_LENGTH] =
    {
        0x32U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
    };
    assert(Frame != NULL);
    assert(Frame->id == CANTP_CANIF_TX_LPDU_FC);
    assert(memcmp(Frame->bytes, expected, sizeof(expected)) == 0);
}

/** Start one segmented sender and leave it waiting for the first FC. */
static void Test_StartTxWaitingForFc(void)
{
    PduInfoType request = {Test_TxSource, 20U};
    assert(CanTp_Transmit(CANTP_TX_NSDU, &request) == E_OK);
    Test_Step();
    assert(Test_FrameCount == 1U);
    assert(Test_Frames[0].id == CANTP_CANIF_TX_LPDU_DATA);
    CanTp_TxConfirmation(CANTP_TX_NPDU_DATA);
}

/** Build a deterministic First Frame from one logical payload base. */
static void Test_MakeFirstFrame(uint8_t *Frame, uint8_t Length,
                                uint8_t Base)
{
    uint8_t index;
    memset(Frame, 0, CANTP_FRAME_LENGTH);
    Frame[0] = CANTP_PCI_FF;
    Frame[1] = Length;
    for (index = 0U; index < CANTP_FF_PAYLOAD_LENGTH; index++)
    {
        Frame[index + 2U] = (uint8_t)(Base + index);
    }
}

/** Build one deterministic Consecutive Frame with caller-selected padding. */
static void Test_MakeConsecutiveFrame(uint8_t *Frame, uint8_t Sn,
                                      uint8_t Base, uint8_t DataBytes)
{
    uint8_t index;
    memset(Frame, 0xEE, CANTP_FRAME_LENGTH);
    Frame[0] = (uint8_t)(CANTP_PCI_CF | (Sn & CANTP_PCI_SN_MASK));
    for (index = 0U; index < DataBytes; index++)
    {
        Frame[index + 1U] = (uint8_t)(Base + index);
    }
}

/** Confirm a newly accepted CTS and enter the receive CF window. */
static void Test_RequestAndConfirmCts(uint8_t ExpectedFrameIndex)
{
    Test_Step();
    assert(Test_FrameCount == (uint8_t)(ExpectedFrameIndex + 1U));
    assert(Test_Frames[ExpectedFrameIndex].id == CANTP_CANIF_TX_LPDU_FC);
    assert(Test_Frames[ExpectedFrameIndex].bytes[0] == 0x30U);
    assert(Test_Frames[ExpectedFrameIndex].bytes[1] == CANTP_BLOCK_SIZE);
    assert(Test_Frames[ExpectedFrameIndex].bytes[2] == CANTP_STMIN_MS);
    CanTp_TxConfirmation(CANTP_TX_NPDU_FC);
}

/** Append a valid CF range and confirm the second CTS after CF4 if needed. */
static void Test_CompleteSegmentedRx(uint8_t TotalLength, uint8_t Base,
                                     uint8_t FirstSn,
                                     uint8_t ExistingReceived,
                                     uint8_t NextFcFrameIndex)
{
    uint8_t frame[CANTP_FRAME_LENGTH];
    uint8_t sn = FirstSn;
    uint8_t received = ExistingReceived;
    uint8_t blockCount = (uint8_t)((ExistingReceived - 6U) / 7U);
    while (received < TotalLength)
    {
        uint8_t remaining = (uint8_t)(TotalLength - received);
        uint8_t dataBytes = (remaining < CANTP_CF_PAYLOAD_LENGTH) ?
            remaining : CANTP_CF_PAYLOAD_LENGTH;
        Test_MakeConsecutiveFrame(frame, sn, (uint8_t)(Base + received),
                                  dataBytes);
        Test_InjectData(frame);
        received = (uint8_t)(received + dataBytes);
        sn = (uint8_t)((sn + 1U) & CANTP_PCI_SN_MASK);
        blockCount++;
        if ((received < TotalLength) && (blockCount == CANTP_BLOCK_SIZE))
        {
            Test_RequestAndConfirmCts(NextFcFrameIndex);
            NextFcFrameIndex++;
            blockCount = 0U;
        }
    }
}

/** T09: abort before append when one CF carries the wrong sequence number. */
static void Test_T09WrongSn(void)
{
    uint8_t frame[CANTP_FRAME_LENGTH];
    Test_Reset();
    Test_MakeFirstFrame(frame, 62U, 0U);
    Test_InjectData(frame);
    Test_RequestAndConfirmCts(0U);
    Test_MakeConsecutiveFrame(frame, 1U, 6U, 7U);
    Test_InjectData(frame);
    Test_MakeConsecutiveFrame(frame, 2U, 13U, 7U);
    Test_InjectData(frame);
    Test_MakeConsecutiveFrame(frame, 4U, 20U, 7U);
    Test_InjectData(frame);

    assert(Test_RxFinalCalls == 1U);
    assert(Test_RxFinalResults[0] == E_NOT_OK);
    assert(Test_ReservationActive == 0U && Test_ReadyCount() == 0U);
    assert(Test_CopyRxCalls == 0U && Test_StartRxCalls == 1U);
    assert(CanTp_LastRxAbortReason == CANTP_ABORT_SEQUENCE_NUMBER);
    assert(Test_AppEventCount == 2U &&
           memcmp(Test_AppEvents, "SI", 2U) == 0);
    printf("EVIDENCE T09 expected_sn=3 injected_pci=24 copy_rx=0 "
           "rx_final_count=1 result=%u reserved=0 ready=0 abort_reason=%u\n",
           (unsigned)Test_RxFinalResults[0],
           (unsigned)CanTp_LastRxAbortReason);
}

/** Fill both application queue entries without changing their payload later. */
static void Test_FillReadyQueue(void)
{
    uint8_t slot;
    uint8_t byte;
    for (slot = 0U; slot < TEST_QUEUE_SIZE; slot++)
    {
        Test_SlotState[slot] = TEST_SLOT_READY;
        Test_SlotLength[slot] = 3U;
        for (byte = 0U; byte < 3U; byte++)
        {
            Test_SlotData[slot][byte] = (uint8_t)(0xA0U + slot * 0x10U + byte);
        }
    }
}

/** T10: preserve a full READY queue, emit OVFLW and abort the waiting sender. */
static void Test_T10QueueFull(void)
{
    uint8_t incoming[CANTP_FRAME_LENGTH];
    uint8_t readySnapshot[TEST_QUEUE_SIZE][3];
    PduInfoType request = {Test_TxSource, CANTP_MAX_NSDU_LENGTH};
    Test_Reset();
    Test_FillReadyQueue();
    memcpy(readySnapshot[0], Test_SlotData[0], 3U);
    memcpy(readySnapshot[1], Test_SlotData[1], 3U);

    assert(CanTp_Transmit(CANTP_TX_NSDU, &request) == E_OK);
    Test_Step();
    assert(Test_Frames[0].id == CANTP_CANIF_TX_LPDU_DATA);
    CanTp_TxConfirmation(CANTP_TX_NPDU_DATA);
    memcpy(incoming, Test_Frames[0].bytes, CANTP_FRAME_LENGTH);
    Test_InjectData(incoming);
    Test_Step();

    assert(Test_FrameCount == 2U);
    Test_AssertOvflwFrame(&Test_Frames[1]);
    Test_InjectFc(Test_Frames[1].bytes);
    assert(Test_TxFinalCalls == 1U && Test_TxFinalResult == E_NOT_OK);
    assert(CanTp_LastTxAbortReason == CANTP_ABORT_INVALID_FLOW_CONTROL);
    assert(Test_RxFinalCalls == 0U && Test_ReservationActive == 0U);
    assert(Test_ReadyCount() == 2U);
    assert(memcmp(readySnapshot[0], Test_SlotData[0], 3U) == 0);
    assert(memcmp(readySnapshot[1], Test_SlotData[1], 3U) == 0);
    printf("EVIDENCE T10 queue_before=READY_READY queue_after=READY_READY "
           "start_rx_calls=%lu ovflw=32_00_00_00_00_00_00_00 "
           "tx_final_count=1 "
           "tx_result=%u rx_final_count=0 ready_payloads_intact=1\n",
           (unsigned long)Test_StartRxCalls,
           (unsigned)Test_TxFinalResult);
}

/** T11: reject oversized/malformed FF without a reservation or replacement. */
static void Test_T11LengthValidation(void)
{
    uint8_t frame[CANTP_FRAME_LENGTH];
    uint8_t malformed[CANTP_FRAME_LENGTH] = {0x10U, 0x06U};
    uint8_t invalidSf0[CANTP_FRAME_LENGTH] = {0x00U, 0x00U};
    uint8_t invalidSf7[CANTP_FRAME_LENGTH] = {0x00U, 0x07U};
    Test_Reset();
    Test_MakeFirstFrame(frame, 100U, 0x10U);
    Test_InjectData(frame);
    Test_Step();
    assert(Test_FrameCount == 1U);
    Test_AssertOvflwFrame(&Test_Frames[0]);
    assert(Test_StartRxCalls == 0U && Test_RxFinalCalls == 0U);
    CanTp_TxConfirmation(CANTP_TX_NPDU_FC);

    Test_MakeFirstFrame(frame, 20U, 0x20U);
    Test_InjectData(frame);
    Test_RequestAndConfirmCts(1U);
    assert(Test_ReservationActive != 0U);
    Test_InjectData(malformed);
    Test_InjectData(invalidSf0);
    Test_InjectData(invalidSf7);
    assert(Test_RxFinalCalls == 0U && Test_ReservationActive != 0U);
    assert(Test_StartRxCalls == 1U && Test_FrameCount == 2U);
    printf("EVIDENCE T11 oversized_pci=10_64 "
           "ovflw=32_00_00_00_00_00_00_00 "
           "oversized_start_rx=0 oversized_rx_final=0 malformed_short_ff="
           "discard active_session_preserved=1 invalid_sf_0_7=discard\n");
}

/** T12: copy one 62-byte message once and publish READY before final success. */
static void Test_T12CopyOrdering(void)
{
    uint8_t frame[CANTP_FRAME_LENGTH];
    uint8_t index;
    Test_Reset();
    Test_MakeFirstFrame(frame, 62U, 0U);
    Test_InjectData(frame);
    Test_RequestAndConfirmCts(0U);
    Test_CompleteSegmentedRx(62U, 0U, 1U, 6U, 1U);

    assert(Test_StartRxCalls == 1U && Test_CopyRxCalls == 1U);
    assert(Test_RxFinalCalls == 1U && Test_RxFinalResults[0] == E_OK);
    assert(Test_ReadyCount() == 1U && Test_SlotLength[0] == 62U);
    for (index = 0U; index < 62U; index++)
    {
        assert(Test_SlotData[0][index] == index);
    }
    assert(Test_AppEventCount == 3U &&
           memcmp(Test_AppEvents, "SCI", 3U) == 0);
    printf("EVIDENCE T12 start_rx=1 copy_rx=1 rx_final=1 result=%u "
           "callback_order=START_COPY_READY_INDICATION length=62 "
           "payload_match=1 ready_before_indication=1\n",
           (unsigned)Test_RxFinalResults[0]);
}

/** T14: replace one active session and complete only the new payload. */
static void Test_T14Replacement(void)
{
    uint8_t frame[CANTP_FRAME_LENGTH];
    uint8_t index;
    Test_Reset();
    Test_MakeFirstFrame(frame, 62U, 0xA0U);
    Test_InjectData(frame);
    Test_RequestAndConfirmCts(0U);
    Test_MakeConsecutiveFrame(frame, 1U, 0xA6U, 7U);
    Test_InjectData(frame);
    Test_MakeConsecutiveFrame(frame, 2U, 0xADU, 7U);
    Test_InjectData(frame);

    Test_MakeFirstFrame(frame, 62U, 0x40U);
    Test_InjectData(frame);
    assert(Test_RxFinalCalls == 1U &&
           Test_RxFinalResults[0] == E_NOT_OK);
    assert(CanTp_LastRxAbortReason == CANTP_ABORT_RX_REPLACED);
    assert(Test_StartRxCalls == 2U && Test_ReservationActive != 0U);
    Test_RequestAndConfirmCts(1U);
    Test_CompleteSegmentedRx(62U, 0x40U, 1U, 6U, 2U);

    assert(Test_CopyRxCalls == 1U && Test_RxFinalCalls == 2U);
    assert(Test_RxFinalResults[1] == E_OK && Test_ReadyCount() == 1U);
    for (index = 0U; index < 62U; index++)
    {
        assert(Test_SlotData[0][index] == (uint8_t)(0x40U + index));
    }
    assert(Test_AppEventCount == 5U &&
           memcmp(Test_AppEvents, "SISCI", 5U) == 0);
    printf("EVIDENCE T14 old_received=20 old_result=%u old_release=1 "
           "new_start=1 new_result=%u new_ready=1 payload_match=1 "
           "callback_order=SISCI abort_reason=%u\n",
           (unsigned)Test_RxFinalResults[0],
           (unsigned)Test_RxFinalResults[1],
           (unsigned)CanTp_LastRxAbortReason);
}

/** Verify final-CF padding, queue-full SF and accepted OVFLW pending locking. */
static void Test_SupplementalDefenses(void)
{
    uint8_t frame[CANTP_FRAME_LENGTH];
    uint8_t sf[CANTP_FRAME_LENGTH] = {0x00U, 0x03U, 1U, 2U, 3U};
    uint8_t firstOvflw[CANTP_FRAME_LENGTH];
    uint8_t index;

    Test_Reset();
    Test_MakeFirstFrame(frame, 60U, 0U);
    Test_InjectData(frame);
    Test_RequestAndConfirmCts(0U);
    Test_CompleteSegmentedRx(60U, 0U, 1U, 6U, 1U);
    assert(Test_SlotLength[0] == 60U && Test_CopyRxCalls == 1U);
    for (index = 0U; index < 60U; index++)
    {
        assert(Test_SlotData[0][index] == index);
    }

    Test_Reset();
    Test_FillReadyQueue();
    Test_InjectData(sf);
    assert(Test_StartRxCalls == 1U && Test_FrameCount == 0U);
    assert(Test_RxFinalCalls == 0U && Test_ReadyCount() == 2U);

    Test_MakeFirstFrame(frame, 62U, 0U);
    Test_InjectData(frame);
    Test_Step();
    assert(Test_FrameCount == 1U && Test_Frames[0].bytes[0] == 0x32U);
    memcpy(firstOvflw, Test_Frames[0].bytes, CANTP_FRAME_LENGTH);
    Test_MakeFirstFrame(frame, 100U, 0x30U);
    Test_InjectData(frame);
    for (index = 0U; index < CANTP_N_AR_MS; index++)
    {
        Test_Step();
    }
    Test_MakeFirstFrame(frame, 62U, 0x50U);
    Test_InjectData(frame);
    assert(Test_FrameCount == 1U);
    assert(memcmp(firstOvflw, Test_Frames[0].bytes, CANTP_FRAME_LENGTH) == 0);
    assert(Test_RxFinalCalls == 0U);
    CanTp_TxConfirmation(CANTP_TX_NPDU_FC);
    Test_InjectData(frame);
    Test_Step();
    assert(Test_FrameCount == 2U && Test_Frames[1].bytes[0] == 0x32U);

    Test_Reset();
    Test_MakeFirstFrame(frame, 20U, 0x20U);
    Test_InjectData(frame);
    Test_RequestAndConfirmCts(0U);
    Test_InjectData(sf);
    assert(Test_RxFinalCalls == 2U);
    assert(Test_RxFinalResults[0] == E_NOT_OK);
    assert(Test_RxFinalResults[1] == E_OK);
    assert(Test_CopyRxCalls == 1U && Test_ReadyCount() == 1U);
    assert(Test_SlotLength[0] == 3U);
    assert(memcmp(Test_SlotData[0], &sf[2], 3U) == 0);
    assert(Test_FrameCount == 1U);
    printf("EVIDENCE SUP_PHASE3 final_cf_length60_padding_ignored=1 "
           "queue_full_sf_no_fc=1 ovflw_pending_preserved=1 "
           "nar_late_confirmation_no_rx_session=1 "
           "valid_sf_replacement=1\n");
}

/** Verify each unsupported FC aborts only an active sender waiting for FC. */
static void Test_InvalidFlowControl(void)
{
    uint8_t invalidCts[CANTP_FRAME_LENGTH] = {0x30U, 0x03U, 0x05U};
    uint8_t invalidStmin[CANTP_FRAME_LENGTH] = {0x30U, 0x04U, 0x06U};
    uint8_t ovflw[CANTP_FRAME_LENGTH] = {0x32U};
    uint8_t waitFc[CANTP_FRAME_LENGTH] = {0x31U};
    uint8_t idleCts[CANTP_FRAME_LENGTH] = {0x30U, 0x04U, 0x05U};
    Test_Reset();
    Test_InjectFc(idleCts);
    assert(Test_TxFinalCalls == 0U);
    Test_StartTxWaitingForFc();
    Test_InjectFc(invalidCts);
    assert(Test_TxFinalCalls == 1U && Test_TxFinalResult == E_NOT_OK);
    assert(CanTp_LastTxAbortReason == CANTP_ABORT_INVALID_FLOW_CONTROL);

    Test_Reset();
    Test_StartTxWaitingForFc();
    Test_InjectFc(invalidStmin);
    assert(Test_TxFinalCalls == 1U && Test_TxFinalResult == E_NOT_OK);
    assert(CanTp_LastTxAbortReason == CANTP_ABORT_INVALID_FLOW_CONTROL);

    Test_Reset();
    Test_StartTxWaitingForFc();
    Test_InjectFc(ovflw);
    assert(Test_TxFinalCalls == 1U && Test_TxFinalResult == E_NOT_OK);
    assert(CanTp_LastTxAbortReason == CANTP_ABORT_INVALID_FLOW_CONTROL);

    Test_Reset();
    Test_StartTxWaitingForFc();
    Test_InjectFc(waitFc);
    assert(Test_TxFinalCalls == 1U && Test_TxFinalResult == E_NOT_OK);
    assert(CanTp_LastTxAbortReason == CANTP_ABORT_INVALID_FLOW_CONTROL);
    printf("EVIDENCE SUP_INVALID_FC idle_fc_ignored=1 bad_bs=3 "
           "bad_stmin=6 ovflw_aborts=1 unsupported_wait_aborts=1 "
           "each_tx_final_count=1 result=%u abort_reason=%u\n",
           (unsigned)Test_TxFinalResult,
           (unsigned)CanTp_LastTxAbortReason);
}

int main(void)
{
    Test_T09WrongSn();
    Test_T10QueueFull();
    Test_T11LengthValidation();
    Test_T12CopyOrdering();
    Test_T14Replacement();
    Test_SupplementalDefenses();
    Test_InvalidFlowControl();
    puts("PASS: CanTp Phase 3 T09-T12/T14 and defensive regressions.");
    return 0;
}
