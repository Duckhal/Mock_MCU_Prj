#ifndef CAN_CFG_H_
#define CAN_CFG_H_

#include "Can_Types.h"

/* HOH ID definitions */
#define CAN_HTH_0    0U   /* TX Handle: dùng MB[0] */
#define CAN_HRH_0    1U   /* RX Handle: nhận frame CAN ID 0x123, dùng MB[1] */

/* Extern declarations — định nghĩa thực tế nằm trong Can_Cfg.c */
extern const Can_HohConfigType Can_HohConfig[];
extern const Can_ConfigType    Can_Config;
extern const Can_ConfigType    Can_Config_Loopback;

#endif /* CAN_CFG_H_ */