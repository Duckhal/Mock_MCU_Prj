#ifndef COM_CFG_H_
#define COM_CFG_H_

#include <stdint.h>
#include "../../comm/com/com_types.h"

#define COM_MAX_IPDU_COUNT          (8U)
#define COM_MAX_GROUP_COUNT         (8U)
#define COM_MAX_SIGNAL_COUNT        (64U)
#define COM_MAX_IPDU_LENGTH_BYTES   (8U)

enum
{
    COM_SIGNAL_VEHICLE_SPEED = 0U,
    COM_SIGNAL_GEAR,
    COM_SIGNAL_ALIVE_COUNTER
};

enum
{
    COM_GROUP_VEHICLE_STATUS = 0U
};

enum
{
    COM_IPDU_VEHICLE_STATUS = 0U
};

extern const Com_ConfigType Com_Config;

#endif /* COM_CFG_H_ */