#ifndef CAN_CFG_H_
#define CAN_CFG_H_

#include "../../driver/Can_Types.h"

#define CAN_CONTROLLER_0    (0U)
#define CAN_HTH_0           (0U)
#define CAN_HRH_0           (1U)

extern const Can_ConfigType Can_Config_Normal;
extern const Can_ConfigType Can_Config_Loopback;

#endif /* CAN_CFG_H_ */
