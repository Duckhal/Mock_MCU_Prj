#ifndef COMM_TYPES_H_
#define COMM_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

typedef uint16_t PduIdType;
typedef uint16_t GlobalPduIdType;

typedef enum
{
    COMM_OK = 0,
    COMM_NOT_OK,
    COMM_BUSY,
    COMM_INVALID_PARAM,
    COMM_NOT_INITIALIZED,
    COMM_INVALID_STATE
} Comm_ReturnType;

typedef struct
{
    const uint8_t *data;
    uint16_t length;
} PduInfoType;

#endif /* COMM_TYPES_H_ */