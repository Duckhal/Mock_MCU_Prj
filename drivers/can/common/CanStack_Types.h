#ifndef CANSTACK_TYPES_H_
#define CANSTACK_TYPES_H_

#include <stdint.h>

typedef uint16_t PduIdType;
typedef uint16_t GlobalPduIdType;
typedef uint8_t PduLengthType;
typedef uint8_t Std_ReturnType;

#define E_OK        ((Std_ReturnType)0U)
#define E_NOT_OK    ((Std_ReturnType)1U)

typedef enum
{
    BUFREQ_OK = 0,
    BUFREQ_E_NOT_OK,
    BUFREQ_E_BUSY,
    BUFREQ_E_OVFL
} BufReq_ReturnType;

typedef struct
{
    uint8_t *SduDataPtr;
    PduLengthType SduLength;
} PduInfoType;

#endif /* CANSTACK_TYPES_H_ */