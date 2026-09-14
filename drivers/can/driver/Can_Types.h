#ifndef CAN_TYPES_H_
#define CAN_TYPES_H_

#include <stdbool.h>
#include <stdint.h>

/* Compile-time limits of the current Classic CAN driver implementation. */
#define CAN_CLASSIC_MAX_DLC       (8U)
#define CAN_STANDARD_ID_MAX       (0x7FFU)
#define CAN_SUPPORTED_MB_COUNT    (16U)
#define CAN_SUPPORTED_BAUDRATE    (500000UL)

/* Scalar identifiers used by the CAN Driver public interface. */
typedef uint32_t Can_IdType;
typedef uint8_t Can_HwHandleType;
typedef uint16_t Can_SwPduHandleType;

/* Result of a synchronous CAN Driver request or terminal Tx event. */
typedef enum
{
    CAN_OK = 0,
    CAN_NOT_OK,
    CAN_BUSY,
    CAN_TIMEOUT,
    CAN_INVALID_PARAM,
    CAN_NOT_INITIALIZED,
    CAN_INVALID_STATE,
    CAN_BUS_OFF,
    CAN_CANCELLED
} Can_ReturnType;

/* Lifecycle state of one configured CAN controller. */
typedef enum
{
    CAN_CONTROLLER_UNINIT = 0,
    CAN_CONTROLLER_STOPPED,
    CAN_CONTROLLER_STARTED,
    CAN_CONTROLLER_FAULT
} Can_ControllerModeType;

/* Direction owned by one configured CAN hardware object. */
typedef enum
{
    CAN_HOH_TYPE_TX = 0,
    CAN_HOH_TYPE_RX
} Can_HohType;

/* Maps one global HTH/HRH handle to a controller mailbox and Rx filter. */
typedef struct
{
    Can_HwHandleType hohId;
    uint8_t controllerId;
    Can_HohType type;
    uint8_t mbIndex;
    Can_IdType rxCanId;
    Can_IdType rxIdMask;
} Can_HohConfigType;

/* Immutable configuration used to initialize one CAN controller. */
typedef struct
{
    uint8_t controllerId;
    uint32_t baudrate;
    bool loopbackEnable;
    uint32_t hardwareTimeoutCount;
    const Can_HohConfigType *hohList;
    uint8_t hohCount;
} Can_ConfigType;

/* Tx request passed to Can_Write(); payload is borrowed during the call. */
typedef struct
{
    Can_IdType id;
    Can_SwPduHandleType swPduHandle;
    uint8_t length;
    const uint8_t *sdu;
} Can_PduType;

/*
 * Metadata passed by the CanIf training Rx interface.
 * The payload pointer is borrowed and remains valid only during the callback.
 */
typedef struct
{
    Can_IdType canId;
    uint8_t length;
    const uint8_t *data;
} Can_RxPduType;

/* Hardware metadata attached to one received CAN frame. */
typedef struct
{
    Can_IdType canId;
    Can_HwHandleType hohId;
    uint8_t controllerId;
} Can_HwType;

/* Upper-layer callbacks registered by CanIf or a standalone test harness. */
typedef void (*Can_TxConfirmationCallbackType)(Can_SwPduHandleType swPduHandle,
                                                Can_ReturnType result);
typedef void (*Can_RxIndicationCallbackType)(const Can_HwType *mailbox,
                                             const Can_PduType *pduInfo);
typedef void (*Can_ControllerBusOffCallbackType)(uint8_t controllerId);

/* Callback table retained by the CAN Driver after registration. */
typedef struct
{
    Can_TxConfirmationCallbackType txConfirmation;
    Can_RxIndicationCallbackType rxIndication;
    Can_ControllerBusOffCallbackType controllerBusOff;
} Can_CallbacksType;

/* Snapshot of controller state and error counters. */
typedef struct
{
    Can_ControllerModeType mode;
    bool busOff;
    uint8_t txErrorCounter;
    uint8_t rxErrorCounter;
    Can_ReturnType lastError;
} Can_ControllerStatusType;

/* Aggregate CAN Driver diagnostic counters. */
typedef struct
{
    uint32_t txAccepted;
    uint32_t txCompleted;
    uint32_t txFailed;
    uint32_t txBusy;
    uint32_t rxDelivered;
    uint32_t rxDropped;
    uint32_t invalidInput;
    uint32_t staleEvent;
    uint32_t busOffCount;
    uint32_t hardwareTimeoutCount;
} Can_StatsType;

#endif /* CAN_TYPES_H_ */
