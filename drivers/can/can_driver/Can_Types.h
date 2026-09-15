#ifndef CAN_TYPES_H
#define CAN_TYPES_H

#include <stdint.h>
#include "../common/CanStack_Types.h"

typedef uint32_t Can_IdType;
typedef uint8_t Can_HwHandleType;
typedef uint8_t Can_ControllerIdType;

typedef enum
{
    CAN_OBJECT_TYPE_TX = 0,
    CAN_OBJECT_TYPE_RX
} Can_ObjectType;

typedef enum
{
    CAN_OK = 0,
    CAN_NOT_OK,
    CAN_BUSY
} Can_ReturnType;

typedef struct
{
    PduIdType swPduHandle;
    Can_IdType id;
    PduLengthType length;
    const uint8_t *sdu;
} Can_PduType;

typedef struct
{
    Can_IdType canId;
    PduLengthType length;
    uint8_t *dataPtr;
} Can_RxPduType;

typedef struct
{
    Can_ControllerIdType controllerId;
    uint8_t instance;
    uint32_t baudRate;
} Can_ControllerConfigType;

typedef struct
{
    Can_HwHandleType objectId;
    Can_ObjectType objectType;
    Can_ControllerIdType controllerId;
    uint8_t hwObjectIndex;
} Can_HardwareObjectConfigType;

#endif /* CAN_TYPES_H */