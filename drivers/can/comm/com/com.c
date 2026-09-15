#include "com.h"

#include <stddef.h>
#include <string.h>

#include "com_codec.h"
#include "../../config/com/com_cfg.h"
#include "../pdur/pdur_com.h"

typedef struct
{
    uint8_t buffer[COM_MAX_IPDU_LENGTH_BYTES];
    uint16_t counter;
    bool pending;
    uint8_t retryCount;
    Com_StatsType stats;
} Com_IPduRuntimeType;

static const Com_ConfigType *s_config = NULL;
static Com_StatusType s_status = COM_UNINIT;
static Com_IPduRuntimeType s_ipduRuntime[COM_MAX_IPDU_COUNT];
static uint32_t s_invalidInputCount = 0U;

/* Helper function used to check if data size matched signal data type */
static Comm_ReturnType Com_GetValueTypeSize(
    Com_SignalValueType valueType, 
    uint16_t *sizeOut)
{
    if (sizeOut == NULL)
    {
        return COMM_INVALID_PARAM;
    }

    switch (valueType)
    {
        case COM_UINT8:
            *sizeOut = 1U;
            break;
        
        case COM_UINT16:
            *sizeOut = 2U;
            break;
        
        case COM_UINT32:
            *sizeOut = 3U;
            break;

        case COM_UINT64:
            *sizeOut = 4U;
            break;

        default:
            return COMM_INVALID_PARAM;
    }

    return COMM_OK;
}

static Comm_ReturnType Com_ReadApplicationValue(
    Com_SignalValueType type,
    const void *data,
    uint16_t size,
    uint64_t *valueOut)
{
    uint16_t expectedSize;
    uint64_t localValue;
    Comm_ReturnType result;

    if ((data == NULL) || (valueOut == NULL))
    {
        return COMM_INVALID_PARAM;
    }

    uint16_t expectedSize;

    result = Com_GetValueTypeSize(type, &expectedSize);

    if (result != COMM_OK)
    {
        return result;
    }

    if (size != expectedSize)
    {
        return COMM_INVALID_PARAM;
    }
    
    switch (type)
    {
        case COM_UINT8:
            uint8_t tempValue;
            memcpy(&tempValue, data, sizeof(tempValue));
            localValue = tempValue;
            break;

        case COM_UINT16:
            uint16_t tempValue;
            memcpy(&tempValue, data, sizeof(tempValue));
            localValue = tempValue;
            break;

        case COM_UINT32:
            uint32_t tempValue;
            memcpy(&tempValue, data, sizeof(tempValue));
            localValue = tempValue;
            break;

        case COM_UINT64:
            uint64_t tempValue;
            memcpy(&tempValue, data, sizeof(tempValue));
            localValue = tempValue;
            break;

        default:
            return COMM_INVALID_PARAM;
    }

    *valueOut = localValue;

    return COMM_OK;
}

static Comm_ReturnType Com_WriteApplicationValue(
    Com_SignalValueType type,
    uint64_t value,
    void *dataOut,
    uint16_t capacity)
{
    uint16_t expectedSize;
    Comm_ReturnType result;

    if (dataOut == NULL)
    {
        return COMM_INVALID_PARAM;
    }

    result = Com_GetValueTypeSize(type, &expectedSize);

    if (result != COMM_OK)
    {
        return result;
    }

    if (capacity < expectedSize)
    {
        return COMM_INVALID_PARAM;
    }

    switch (type)
    {
        case COM_UINT8:
        {
            uint8_t temporaryValue;

            if (value > UINT8_MAX)
            {
                return COMM_INVALID_PARAM;
            }

            temporaryValue = (uint8_t)value;
            memcpy(dataOut, &temporaryValue, sizeof(temporaryValue));
            break;
        }

        case COM_UINT16:
        {
            uint16_t temporaryValue;

            if (value > UINT16_MAX)
            {
                return COMM_INVALID_PARAM;
            }

            temporaryValue = (uint16_t)value;
            memcpy(dataOut, &temporaryValue, sizeof(temporaryValue));
            break;
        }

        case COM_UINT32:
        {
            uint32_t temporaryValue;

            if (value > UINT32_MAX)
            {
                return COMM_INVALID_PARAM;
            }

            temporaryValue = (uint32_t)value;
            memcpy(dataOut, &temporaryValue, sizeof(temporaryValue));
            break;
        }

        case COM_UINT64:
        {
            uint64_t temporaryValue = value;
            memcpy(dataOut, &temporaryValue, sizeof(temporaryValue));
            break;
        }

        default:
            return COMM_INVALID_PARAM;
    }

    return COMM_OK;
}

/* Validates the configuration and initializes all COM runtime data. */
Comm_ReturnType Com_Init(const Com_ConfigType *config)
{

}

/* Updates one Tx Signal value and sets its Update Bit without transmitting. */
Comm_ReturnType Com_SendSignal(Com_SignalIdType signalId, const void *signalData, 
                                uint16_t signalDataSize);

/* Copies the latest valid Rx Signal value to the caller. */
Comm_ReturnType Com_ReceiveSignal(Com_SignalIdType signalId, void *signalDataOut,
                                    uint16_t outputCapacity);

/* Advances every Tx I-PDU scheduler by exactly one 1 ms tick. */
void Com_MainFunctionTx(void);

/* Receives and atomically commits one I-PDU supplied by PduR. */
void Com_RxIndication(PduIdType rxIPduId, const PduInfoType *pduInfo);

/* Records the final lower-layer result for an accepted Tx request. */
void Com_TxConfirmation(PduIdType txIPduId, Comm_ReturnType result);

/* Copies the diagnostic counters of one configured I-PDU. */
Comm_ReturnType Com_GetStats(PduIdType ipduId, Com_StatsType *statsOut);