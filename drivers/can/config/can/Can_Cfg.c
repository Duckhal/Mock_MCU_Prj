#include "Can_Cfg.h"

#define CAN_HARDWARE_TIMEOUT_COUNT    (1000000UL)
#define CAN_LOOPBACK_TEST_ID          (0x123U)

static const Can_HohConfigType Can_HohConfig[] =
{
    {
        CAN_HTH_0,
        CAN_CONTROLLER_0,
        CAN_HOH_TYPE_TX,
        0U,
        0U,
        0U
    },
    {
        CAN_HRH_0,
        CAN_CONTROLLER_0,
        CAN_HOH_TYPE_RX,
        1U,
        CAN_LOOPBACK_TEST_ID,
        CAN_STANDARD_ID_MAX
    }
};

const Can_ConfigType Can_Config_Normal =
{
    CAN_CONTROLLER_0,
    CAN_SUPPORTED_BAUDRATE,
    false,
    CAN_HARDWARE_TIMEOUT_COUNT,
    Can_HohConfig,
    (uint8_t)(sizeof(Can_HohConfig) / sizeof(Can_HohConfig[0]))
};

const Can_ConfigType Can_Config_Loopback =
{
    CAN_CONTROLLER_0,
    CAN_SUPPORTED_BAUDRATE,
    true,
    CAN_HARDWARE_TIMEOUT_COUNT,
    Can_HohConfig,
    (uint8_t)(sizeof(Can_HohConfig) / sizeof(Can_HohConfig[0]))
};
