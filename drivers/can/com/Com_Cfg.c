#include "Com_Cfg.h"
#include "../common/CanStack_Cfg.h"

static const PduIdType LedControlRxSignalList[] =
{
    COM_SIGNAL_RX_LED_COMMAND
};

static const PduIdType LedControlSignalList[] =
{
    COM_SIGNAL_LED_COMMAND
};

const Com_SignalConfigType
Com_SignalConfig[COM_NUM_SIGNALS] =
{
    {
        .signalId = COM_SIGNAL_LED_COMMAND,
        .signalGroupId = COM_SIGNAL_GROUP_LED_CONTROL,
        .slotStartBit = 8U,
        .slotLength = 16U,
        .useUpdateBit = 0U
    },
    {
        .signalId = COM_SIGNAL_RX_LED_COMMAND,
        .signalGroupId = COM_SIGNAL_GROUP_RX_LED_CONTROL,
        .slotStartBit = 8U,
        .slotLength = 16U,
        .useUpdateBit = 0U
    }
};

const Com_SignalGroupConfigType
Com_SignalGroupConfig[COM_NUM_SIGNAL_GROUPS] =
{
    {
        .signalGroupId = COM_SIGNAL_GROUP_LED_CONTROL,
        .signalList = LedControlSignalList,
        .numSignals = 1U
    },
    {
        .signalGroupId = COM_SIGNAL_GROUP_RX_LED_CONTROL,
        .signalList = LedControlRxSignalList,
        .numSignals = 1U
    }
};

const Com_IPduConfigType
Com_IPduConfig[COM_NUM_IPDUS] =
{
    {
        .ipduId = COM_IPDU_VEHICLE_STATUS,
        .globalPduId = GLOBAL_PDU_VEHICLE_STATUS,

        .direction = COM_IPDU_TX,
        .length = 8U,

        .signalGroupId = COM_SIGNAL_GROUP_LED_CONTROL,

        .periodTicks = 10U,
        .initialOffsetTicks = 1U,

        .maxRetries = 3U
    },
    { COM_IPDU_RX_VEHICLE_STATUS, GLOBAL_PDU_VEHICLE_STATUS,
      COM_IPDU_RX, 8U, COM_SIGNAL_GROUP_RX_LED_CONTROL, 0U, 0U, 0U }
};
