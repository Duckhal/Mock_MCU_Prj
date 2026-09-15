#ifndef CANSTACK_TYPES_H
#define CANSTACK_TYPES_H

#include <stdint.h>

typedef uint16_t PduIdType;
typedef uint16_t GlobalPduIdType;
typedef uint8_t PduLengthType;
typedef uint8_t Std_ReturnType;

#define E_OK        ((Std_ReturnType)0U)
#define E_NOT_OK    ((Std_ReturnType)1U)

typedef struct
{
    uint8_t *SduDataPtr;
    PduLengthType SduLength;
} PduInfoType;

#endif /* CANSTACK_TYPES_H */