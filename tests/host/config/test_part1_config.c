#include <stdbool.h>
#include <stdio.h>

#include "../../../drivers/can/config/can/Can_Cfg.h"
#include "../../../drivers/can/config/canif/canif_cfg.h"
#include "../../../drivers/can/config/comm_matrix_cfg.h"
#include "../../../drivers/can/config/com/com_cfg.h"
#include "../../../drivers/can/config/node_cfg.h"
#include "../../../drivers/can/config/pdur/pdur_cfg.h"

#define TEST_CHECK(condition)                                                   \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            (void)printf("FAIL line %d: %s\n", __LINE__, #condition);          \
            return 1;                                                           \
        }                                                                       \
    } while (false)

/*
 * Verifies that the handwritten module configurations describe one consistent
 * Direct CAN Binding without executing any communication-stack logic.
 */
int main(void)
{
    const Com_IPduConfigType *comIpdu;
    const PduR_TxRouteConfigType *pduRRoute;
    const CanIf_TxPduConfigType *canIfTxPdu;
    bool hthFound = false;
    uint8_t hohIndex;
    uint16_t ipduIndex;
    uint16_t comparisonIndex;

    TEST_CHECK(Node_Config.comConfig == &Com_Config);
    TEST_CHECK(Node_Config.pduRConfig == &PduR_Config);
    TEST_CHECK(Node_Config.canIfConfig == &CanIf_Config);
    TEST_CHECK(Node_Config.canConfig == &Can_Config_Normal);

    TEST_CHECK(Com_Config.signalCount == 3U);
    TEST_CHECK(Com_Config.signalGroupCount == 1U);
    TEST_CHECK(Com_Config.ipduCount == 1U);
    TEST_CHECK(PduR_Config.txRouteCount == 1U);
    TEST_CHECK(PduR_Config.rxRouteCount == 0U);
    TEST_CHECK(CanIf_Config.txPduCount == 1U);
    TEST_CHECK(CanIf_Config.rxPduCount == 0U);

    comIpdu = &Com_Config.ipdus[0];
    pduRRoute = &PduR_Config.txRoutes[0];
    canIfTxPdu = &CanIf_Config.txPdus[0];

    TEST_CHECK(comIpdu->globalPduId == COMM_MATRIX_GLOBAL_PDU_VEHICLE_STATUS);
    TEST_CHECK(pduRRoute->globalPduId == comIpdu->globalPduId);
    TEST_CHECK(canIfTxPdu->globalPduId == comIpdu->globalPduId);
    TEST_CHECK(pduRRoute->sourceComIPduId == comIpdu->ipduId);
    TEST_CHECK(pduRRoute->destinationCanIfTxPduId == canIfTxPdu->txPduId);
    TEST_CHECK(canIfTxPdu->canId == COMM_MATRIX_CAN_ID_VEHICLE_STATUS);
    TEST_CHECK(canIfTxPdu->lengthBytes == comIpdu->lengthBytes);
    TEST_CHECK(comIpdu->lengthBytes == COMM_MATRIX_VEHICLE_STATUS_LENGTH_BYTES);
    TEST_CHECK(COMM_MATRIX_COM_MAIN_FUNCTION_PERIOD_MS == 1U);

    for (ipduIndex = 0U; ipduIndex < Com_Config.ipduCount; ipduIndex++)
    {
        const Com_IPduConfigType *currentIpdu = &Com_Config.ipdus[ipduIndex];

        if (currentIpdu->direction != COM_IPDU_TX)
        {
            continue;
        }

        TEST_CHECK((currentIpdu->periodTicks %
                    COMM_MATRIX_TX_PHASE_WINDOW_TICKS) == 0U);
        TEST_CHECK(currentIpdu->initialOffsetTicks >=
                   COMM_MATRIX_TX_PHASE_OFFSET_MIN_TICKS);
        TEST_CHECK(currentIpdu->initialOffsetTicks <=
                   COMM_MATRIX_TX_PHASE_OFFSET_MAX_TICKS);

        for (comparisonIndex = (uint16_t)(ipduIndex + 1U);
             comparisonIndex < Com_Config.ipduCount;
             comparisonIndex++)
        {
            const Com_IPduConfigType *otherIpdu =
                &Com_Config.ipdus[comparisonIndex];

            if (otherIpdu->direction == COM_IPDU_TX)
            {
                TEST_CHECK(currentIpdu->initialOffsetTicks !=
                           otherIpdu->initialOffsetTicks);
            }
        }
    }

    for (hohIndex = 0U; hohIndex < Can_Config_Normal.hohCount; hohIndex++)
    {
        const Can_HohConfigType *hoh = &Can_Config_Normal.hohList[hohIndex];

        if (hoh->hohId == canIfTxPdu->hthRef)
        {
            TEST_CHECK(hoh->type == CAN_HOH_TYPE_TX);
            hthFound = true;
            break;
        }
    }

    TEST_CHECK(hthFound);

    (void)puts("Part 1 type/config tests passed");
    return 0;
}
