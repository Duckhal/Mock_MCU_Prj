#include "Com_Cfg.h"
#include "../common/CanStack_Cfg.h"

static const PduIdType VehicleStatusSignalList[] =
{
    COM_SIGNAL_VEHICLE_SPEED,
    COM_SIGNAL_GEAR,
    COM_SIGNAL_ALIVE_COUNTER
};

const Com_SignalConfigType
Com_SignalConfig[COM_NUM_SIGNALS] =
{
    {
        .signalId = COM_SIGNAL_VEHICLE_SPEED,
        .signalGroupId = COM_SIGNAL_GROUP_VEHICLE_STATUS,
        .slotStartBit = 0U,
        .slotLength = 16U
    },

    {
        .signalId = COM_SIGNAL_GEAR,
        .signalGroupId = COM_SIGNAL_GROUP_VEHICLE_STATUS,
        .slotStartBit = 16U,
        .slotLength = 8U
    },

    {
        .signalId = COM_SIGNAL_ALIVE_COUNTER,
        .signalGroupId = COM_SIGNAL_GROUP_VEHICLE_STATUS,
        .slotStartBit = 24U,
        .slotLength = 8U
    }
};

const Com_SignalGroupConfigType
Com_SignalGroupConfig[COM_NUM_SIGNAL_GROUPS] =
{
    {
        .signalGroupId = COM_SIGNAL_GROUP_VEHICLE_STATUS,
        .signalList = VehicleStatusSignalList,
        .numSignals = 3U
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

        .signalGroupId = COM_SIGNAL_GROUP_VEHICLE_STATUS,

        .periodTicks = 10U,
        .initialOffsetTicks = 1U,

        .maxRetries = 3U
    }
};