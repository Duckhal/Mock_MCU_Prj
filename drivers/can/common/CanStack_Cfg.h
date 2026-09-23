#ifndef CANSTACK_CFG_H_
#define CANSTACK_CFG_H_

#include "CanStack_Types.h"

#define CANSTACK_NUM_GLOBAL_PDUS    (4U)

#define GLOBAL_PDU_KEEPALIVE        ((GlobalPduIdType)0x0010U)
#define GLOBAL_PDU_SLAVE1_STATUS    ((GlobalPduIdType)0x0011U)
#define GLOBAL_PDU_SLAVE2_STATUS    ((GlobalPduIdType)0x0012U)
#define GLOBAL_PDU_CANTP_DATA       ((GlobalPduIdType)0x0020U)

#endif /* CANSTACK_CFG_H_ */
