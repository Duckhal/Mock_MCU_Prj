#include "Can_Cfg.h"

const Can_ControllerConfigType Can_ControllerConfig[CAN_NUM_CONTROLLERS] =
{
    {
        .controllerId = CAN_CONTROLLER_0,
        .instance = 0U
    }
};

const Can_HardwareObjectConfigType Can_HardwareObjectConfig[CAN_NUM_HOH] =
{
    {
        .objectId = CAN_HTH_CAN0_TX,
        .objectType = CAN_OBJECT_TYPE_TX,
        .controllerId = CAN_CONTROLLER_0,
        .hwObjectIndex = 8U
    },

    {
        .objectId = CAN_HRH_CAN0_RX,
        .objectType = CAN_OBJECT_TYPE_RX,
        .controllerId = CAN_CONTROLLER_0,
        .hwObjectIndex = 9U
    }
};