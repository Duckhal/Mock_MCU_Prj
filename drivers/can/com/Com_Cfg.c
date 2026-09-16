#include "Com_Cfg.h"
#include "../common/CanStack_Cfg.h"

static const PduIdType VehicleStatusRxSignalList[] =
{
    COM_SIGNAL_RX_VEHICLE_SPEED, COM_SIGNAL_RX_GEAR,
    COM_SIGNAL_RX_ALIVE_COUNTER
};

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
    },
    { COM_SIGNAL_RX_VEHICLE_SPEED, COM_SIGNAL_GROUP_RX_VEHICLE_STATUS, 0U, 16U },
    { COM_SIGNAL_RX_GEAR, COM_SIGNAL_GROUP_RX_VEHICLE_STATUS, 16U, 8U },
    { COM_SIGNAL_RX_ALIVE_COUNTER, COM_SIGNAL_GROUP_RX_VEHICLE_STATUS, 24U, 8U }
};

const Com_SignalGroupConfigType
Com_SignalGroupConfig[COM_NUM_SIGNAL_GROUPS] =
{
    {
        .signalGroupId = COM_SIGNAL_GROUP_VEHICLE_STATUS,
        .signalList = VehicleStatusSignalList,
        .numSignals = 3U
    },
    { COM_SIGNAL_GROUP_RX_VEHICLE_STATUS, VehicleStatusRxSignalList, 3U }
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
    },
    { COM_IPDU_RX_VEHICLE_STATUS, GLOBAL_PDU_VEHICLE_STATUS,
      COM_IPDU_RX, 8U, COM_SIGNAL_GROUP_RX_VEHICLE_STATUS, 0U, 0U, 0U }
};
