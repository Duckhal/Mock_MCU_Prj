#ifndef COM_CFG_H_
#define COM_CFG_H_

#include "../../comm/com/com_types.h"

/* Static storage limits enforced later by Com_Init(). */
#define COM_MAX_IPDU_COUNT          (8U)
#define COM_MAX_GROUP_COUNT         (8U)
#define COM_MAX_SIGNAL_COUNT        (64U)
#define COM_MAX_IPDU_LENGTH_BYTES   (8U)

/* VehicleStatus timing expressed in 1 ms Com_MainFunctionTx ticks. */
#define COM_VEHICLE_STATUS_PERIOD_TICKS   (10U)
#define COM_VEHICLE_STATUS_OFFSET_TICKS   (1U)
#define COM_VEHICLE_STATUS_MAX_RETRIES    (3U)

/* COM-local Signal handles for the current training profile. */
enum
{
    COM_SIGNAL_VEHICLE_SPEED = 0U,
    COM_SIGNAL_GEAR,
    COM_SIGNAL_ALIVE_COUNTER
};

/* COM-local Signal Group handles for the current training profile. */
enum
{
    COM_GROUP_VEHICLE_STATUS = 0U
};

/* COM-local I-PDU handles for the current training profile. */
enum
{
    COM_IPDU_VEHICLE_STATUS = 0U
};

/* Immutable COM configuration selected by Node_Config. */
extern const Com_ConfigType Com_Config;

#endif /* COM_CFG_H_ */
