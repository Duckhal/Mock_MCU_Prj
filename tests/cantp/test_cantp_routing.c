#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../app/node_app.h"
#include "../../drivers/can/cantp/Cantp_Cfg.h"
#include "../../drivers/can/canif/CanIf_Cfg.h"
#include "../../drivers/can/com/Com.h"
#include "../../drivers/can/pdur/PduR.h"

static uint32_t Test_CanTpTransmitCalls;
static PduIdType Test_LastTxNSdu;
static uint8_t Test_TxSnapshot[CANTP_MAX_NSDU_LENGTH];
static PduLengthType Test_TxSnapshotLength;
static uint32_t Test_RxCalls;
static PduIdType Test_LastRxNPdu;
static uint32_t Test_TxConfirmationCalls;
static PduIdType Test_LastTxNPdu;

/** Emulate CanTp acceptance and request its one PduR Tx snapshot. */
Std_ReturnType CanTp_Transmit(PduIdType TxNSduId,
                              const PduInfoType *PduInfoPtr)
{
    assert(PduInfoPtr != NULL);
    Test_CanTpTransmitCalls++;
    Test_LastTxNSdu = TxNSduId;
    Test_TxSnapshotLength = PduInfoPtr->SduLength;
    assert(PduR_CanTpCopyTxData(TxNSduId, Test_TxSnapshot,
                                 Test_TxSnapshotLength) == BUFREQ_OK);
    return E_OK;
}

/** Capture lower Rx routing from CanIf through PduR to CanTp. */
void CanTp_RxIndication(PduIdType RxNPduId,
                        const PduInfoType *PduInfoPtr)
{
    assert(PduInfoPtr != NULL && PduInfoPtr->SduLength == 8U);
    Test_RxCalls++;
    Test_LastRxNPdu = RxNPduId;
}

/** Capture lower Tx confirmation routing from CanIf through PduR to CanTp. */
void CanTp_TxConfirmation(PduIdType TxNPduId)
{
    Test_TxConfirmationCalls++;
    Test_LastTxNPdu = TxNPduId;
}

/** Satisfy the unchanged direct COM transmit route in PduR. */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId,
                              const PduInfoType *PduInfoPtr)
{
    (void)TxPduId;
    (void)PduInfoPtr;
    return E_OK;
}

/** Satisfy the unchanged direct COM Rx route in PduR. */
void Com_RxIndication(PduIdType ComRxPduId,
                      const PduInfoType *PduInfoPtr)
{
    (void)ComRxPduId;
    (void)PduInfoPtr;
}

/** Satisfy the unchanged direct COM Tx confirmation route in PduR. */
void Com_TxConfirmation(PduIdType ComTxPduId)
{
    (void)ComTxPduId;
}

/** Verify application ownership, Tx snapshot and final callback routing. */
static void Test_ApplicationTxRoute(void)
{
    uint8_t source[20];
    uint8_t tooLong[NODE_APP_MAX_NSDU_LENGTH + 1U] = {0U};
    uint8_t index;
    assert(NodeApp_Init() == E_OK);
    for (index = 0U; index < sizeof(source); index++) { source[index] = index; }

    assert(NodeApp_Transmit(tooLong, sizeof(tooLong)) == E_NOT_OK);
    assert(Test_CanTpTransmitCalls == 0U);
    assert(NodeApp_Transmit(source, sizeof(source)) == E_OK);
    source[0] = 0xFFU;
    assert(Test_CanTpTransmitCalls == 1U);
    assert(Test_LastTxNSdu == CANTP_TX_NSDU);
    assert(Test_TxSnapshotLength == sizeof(source));
    assert(Test_TxSnapshot[0] == 0U);
    assert(Test_TxSnapshot[19] == 19U);

    PduR_CanTpTxConfirmation(CANTP_TX_NSDU, E_OK);
    assert(NodeApp_TxConfirmationCount == 1U);
    assert(NodeApp_LastTxResult == E_OK);
}

/** Verify RESERVED data becomes READY before the final Rx indication. */
static void Test_ApplicationRxQueue(void)
{
    uint8_t payload[62];
    uint8_t secondPayload[3] = {0xA1U, 0xA2U, 0xA3U};
    uint8_t output[62];
    PduLengthType length = 0U;
    uint8_t index;
    for (index = 0U; index < sizeof(payload); index++) { payload[index] = index; }

    assert(PduR_CanTpStartOfReception(CANTP_RX_NSDU,
                                      CANTP_MAX_NSDU_LENGTH + 1U) ==
           BUFREQ_E_NOT_OK);
    assert(PduR_CanTpStartOfReception(CANTP_RX_NSDU,
                                      sizeof(payload)) == BUFREQ_OK);
    assert(NodeApp_GetReadyCount() == 0U);
    assert(PduR_CanTpCopyRxData(CANTP_RX_NSDU, payload,
                                 sizeof(payload)) == BUFREQ_OK);
    assert(NodeApp_GetReadyCount() == 1U);
    assert(NodeApp_RxIndicationCount == 0U);
    PduR_CanTpRxIndication(CANTP_RX_NSDU, E_OK);
    assert(NodeApp_RxIndicationCount == 1U);
    assert(NodeApp_LastRxResult == E_OK);

    assert(PduR_CanTpStartOfReception(CANTP_RX_NSDU,
                                      sizeof(secondPayload)) == BUFREQ_OK);
    assert(PduR_CanTpCopyRxData(CANTP_RX_NSDU, secondPayload,
                                sizeof(secondPayload)) == BUFREQ_OK);
    PduR_CanTpRxIndication(CANTP_RX_NSDU, E_OK);
    assert(NodeApp_GetReadyCount() == 2U);
    assert(PduR_CanTpStartOfReception(CANTP_RX_NSDU, 1U) == BUFREQ_E_OVFL);

    assert(NodeApp_Receive(output, sizeof(output), &length) == E_OK);
    assert(length == sizeof(payload));
    assert(memcmp(output, payload, sizeof(payload)) == 0);
    assert(NodeApp_Receive(output, sizeof(output), &length) == E_OK);
    assert(length == sizeof(secondPayload));
    assert(memcmp(output, secondPayload, sizeof(secondPayload)) == 0);
    assert(NodeApp_GetReadyCount() == 0U);
}

/** Verify separate Data and FC L-PDUs map to their configured N-PDU handles. */
static void Test_LowerRoutes(void)
{
    uint8_t bytes[8] = {0U};
    PduInfoType frame = {bytes, sizeof(bytes)};

    PduR_CanIfRxIndication(CANIF_RX_PDU_CANTP_DATA, &frame);
    assert(Test_RxCalls == 1U && Test_LastRxNPdu == CANTP_RX_NPDU_DATA);
    PduR_CanIfRxIndication(CANIF_RX_PDU_CANTP_FC, &frame);
    assert(Test_RxCalls == 2U && Test_LastRxNPdu == CANTP_RX_NPDU_FC);

    PduR_CanIfTxConfirmation(CANIF_TX_PDU_CANTP_DATA);
    assert(Test_TxConfirmationCalls == 1U &&
           Test_LastTxNPdu == CANTP_TX_NPDU_DATA);
    PduR_CanIfTxConfirmation(CANIF_TX_PDU_CANTP_FC);
    assert(Test_TxConfirmationCalls == 2U &&
           Test_LastTxNPdu == CANTP_TX_NPDU_FC);
}

int main(void)
{
    Test_ApplicationTxRoute();
    Test_ApplicationRxQueue();
    Test_LowerRoutes();
    puts("PASS: App/PduR/CanTp Phase-1 ownership and Data/FC routes.");
    return 0;
}
