#ifndef CANSTACK_CFG_H_
#define CANSTACK_CFG_H_

#include "CanStack_Types.h"

#define CANSTACK_NUM_GLOBAL_PDUS    (4U)

#define GLOBAL_PDU_VEHICLE_STATUS   ((GlobalPduIdType)0x0010U)
#define GLOBAL_PDU_ENGINE_STATUS    ((GlobalPduIdType)0x0011U)
#define GLOBAL_PDU_CLIMATE_STATUS   ((GlobalPduIdType)0x0012U)
#define GLOBAL_PDU_CANTP_DATA       ((GlobalPduIdType)0x0020U)

#endif /* CANSTACK_CFG_H_ */
