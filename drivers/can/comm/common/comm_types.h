#ifndef COMM_TYPES_H_
#define COMM_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

/* Module-local PDU handle used only inside one firmware configuration. */
typedef uint16_t PduIdType;

/* System-wide identity shared by every endpoint of one logical message. */
typedef uint16_t GlobalPduIdType;

/* Common synchronous result codes exchanged between communication modules. */
typedef enum
{
    COMM_OK = 0,
    COMM_NOT_OK,
    COMM_BUSY,
    COMM_INVALID_PARAM,
    COMM_NOT_INITIALIZED,
    COMM_INVALID_STATE,
    COMM_TIMEOUT,
    COMM_BUS_OFF,
    COMM_CANCELLED
} Comm_ReturnType;

/* Borrowed payload view; the receiver must consume or copy it during the call. */
typedef struct
{
    const uint8_t *data;
    uint16_t length;
} PduInfoType;

#endif /* COMM_TYPES_H_ */
