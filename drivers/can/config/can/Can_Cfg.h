#ifndef CAN_CFG_H_
#define CAN_CFG_H_

#include "../../driver/Can_Types.h"

/* CAN Driver-local controller and hardware-object handles. */
#define CAN_CONTROLLER_0    (0U)
#define CAN_HTH_0           (0U)
#define CAN_HRH_0           (1U)

/* Normal and internal-loopback configurations for the same CAN0 hardware. */
extern const Can_ConfigType Can_Config_Normal;
extern const Can_ConfigType Can_Config_Loopback;

#endif /* CAN_CFG_H_ */
