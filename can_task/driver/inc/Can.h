#ifndef CAN_H_
#define CAN_H_

#include "Can_Types.h"
#include "Can_Cfg.h"

void Can_Init(const Can_ConfigType *Config);
Can_ReturnType Can_Write(uint8_t HthId, const Can_PduType *PduInfo);
void Can_MainFunction_Write(void);
void Can_MainFunction_Read(void);
void CAN0_ORed_Message_buffer_IRQHandler(void);

#endif