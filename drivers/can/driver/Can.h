#ifndef CAN_H_
#define CAN_H_

#include "Can_Types.h"

/** Configure CAN0 and leave it initialized in STOPPED/freeze state. */
Can_ReturnType Can_Init(const Can_ConfigType *config);

/** Register upper-layer callbacks while the controller is STOPPED. */
Can_ReturnType Can_RegisterCallbacks(const Can_CallbacksType *callbacks);

/** Start or stop the configured CAN controller with a bounded transition. */
Can_ReturnType Can_SetControllerMode(uint8_t controllerId,
                                     Can_ControllerModeType mode);

/** Submit one Classical CAN frame to the transmit HOH identified by hth. */
Can_ReturnType Can_Write(Can_HwHandleType hth,
                         const Can_PduType *pduInfo);

/** Poll Tx flags and deliver terminal Tx confirmations in main context. */
void Can_MainFunction_Write(void);

/** Poll Rx mailboxes and deliver validated frames in main context. */
void Can_MainFunction_Read(void);

/** Poll controller errors and report/latch a bus-off transition. */
void Can_MainFunction_Error(void);

/** Read current controller lifecycle and hardware error counters. */
Can_ReturnType Can_GetControllerStatus(uint8_t controllerId,
                                       Can_ControllerStatusType *statusOut);

/** Copy accumulated driver diagnostic counters into statsOut. */
Can_ReturnType Can_GetStats(Can_StatsType *statsOut);

/** Reset diagnostic counters without changing controller state. */
void Can_ResetStats(void);

#endif /* CAN_H_ */
