#ifndef CAN_CFG_H
#define CAN_CFG_H

#include "Can_Types.h"

/* =========================
 * Controller configuration
 * ========================= */
#define CAN_NUM_CONTROLLERS     (1U)
#define CAN_CONTROLLER_0        ((Can_ControllerIdType)0U)

/* =========================
 * Hardware Objects
 * ========================= */
#define CAN_NUM_HOH             (2U)
#define CAN_HTH_CAN0_TX         ((Can_HwHandleType)0U)
#define CAN_HRH_CAN0_RX         ((Can_HwHandleType)1U)

/* =========================
 * Configuration tables
 * ========================= */
extern const Can_ControllerConfigType Can_ControllerConfig[CAN_NUM_CONTROLLERS];
extern const Can_HardwareObjectConfigType Can_HardwareObjectConfig[CAN_NUM_HOH];

#endif /* CAN_CFG_H */