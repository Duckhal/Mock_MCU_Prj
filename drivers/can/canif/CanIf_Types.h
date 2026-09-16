#ifndef CANIF_TYPES_H_
#define CANIF_TYPES_H_

#include "../common/CanStack_Types.h"
#include "../can_driver/Can_Types.h"

typedef struct
{
    PduIdType txPduId;
    Can_IdType canId;
    Can_HwHandleType hth;
} CanIf_TxPduConfigType;

typedef struct
{
    PduIdType rxPduId;
    Can_IdType canId;
    Can_HwHandleType hrh;
} CanIf_RxPduConfigType;

#endif /* CANIF_TYPES_H_ */