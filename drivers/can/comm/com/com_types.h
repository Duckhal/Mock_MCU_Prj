#ifndef COM_TYPES_H_
#define COM_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

typedef uint16_t Com_SignalIdType;

typedef enum
{
    COM_UNINIT = 0,
    COM_IDLE,
    COM_BUSY,
    COM_ERROR
} Com_StatusType;

/*
 * Forward declaration.
 */
typedef struct Com_ConfigType Com_ConfigType;

#endif /* COM_TYPES_H_ */