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

/* Returns the Signal table index matching a COM-local Signal ID. */
static Comm_ReturnType Com_FindSignalIndex(
    Com_SignalIdType signalId,
    uint16_t *indexOut)
{
    if (indexOut == NULL)
    {
        return COMM_INVALID_PARAM;
    }

    if ((s_status != COM_INIT) || (s_config == NULL))
    {
        return COMM_NOT_INITIALIZED;
    }

    for (uint16_t i = 0U; i < s_config->signalCount; i++)
    {
        if (s_config->signals[i].signalId == signalId)
        {
            *indexOut = i;
            return COMM_OK;
        }
    }

    return COMM_INVALID_PARAM;
}

/* Returns the Group table index whose references contain the Signal ID. */
static Comm_ReturnType Com_FindGroupIndexContainingSignal(
    Com_SignalIdType signalId,
    uint16_t *indexOut)
{
    if (indexOut == NULL)
    {
        return COMM_INVALID_PARAM;
    }

    if ((s_status != COM_INIT) || (s_config == NULL))
    {
        return COMM_NOT_INITIALIZED;
    }

    for (uint16_t i = 0U; i < s_config->signalGroupCount; i++)
    {
        const Com_SignalGroupConfigType *group = &s_config->signalGroups[i];

        for (uint16_t j = 0U; j < group->signalCount; j++)
        {
            if (group->signalRefs[j] == signalId)
            {
                *indexOut = i;
                return COMM_OK;
            }
        }
    }

    return COMM_INVALID_PARAM;
}

/* Returns the I-PDU table index referencing a COM-local Group ID. */
static Comm_ReturnType Com_FindIPduIndexByGroup(
    Com_SignalGroupIdType groupId,
    uint16_t *indexOut)
{
    if (indexOut == NULL)
    {
        return COMM_INVALID_PARAM;
    }

    if ((s_status != COM_INIT) || (s_config == NULL))
    {
        return COMM_NOT_INITIALIZED;
    }

    for (uint16_t i = 0U; i < s_config->ipduCount; i++)
    {
        if (s_config->ipdus[i].signalGroupRef == groupId)
        {
            *indexOut = i;
            return COMM_OK;
        }
    }

    return COMM_INVALID_PARAM;
}

/* Returns the I-PDU table index matching a COM-local I-PDU ID. */
static Comm_ReturnType Com_FindIPduIndexById(
    PduIdType ipduId,
    uint16_t *indexOut)
{
    if (indexOut == NULL)
    {
        return COMM_INVALID_PARAM;
    }

    if ((s_status != COM_INIT) || (s_config == NULL))
    {
        return COMM_NOT_INITIALIZED;
    }

    for (uint16_t i = 0U; i < s_config->ipduCount; i++)
    {
        if (s_config->ipdus[i].ipduId == ipduId)
        {
            *indexOut = i;
            return COMM_OK;
        }
    }

    return COMM_INVALID_PARAM;
}



/* Resolves a Signal ID from an explicit configuration before COM is initialized. */
static Comm_ReturnType Com_FindSignalIndexInConfig(
    const Com_ConfigType *config,
    Com_SignalIdType signalId,
    uint16_t *indexOut)
{
    if ((config == NULL) || (indexOut == NULL) ||
        (config->signalCount > COM_MAX_SIGNAL_COUNT) ||
        ((config->signalCount != 0U) && (config->signals == NULL)))
    {
        return COMM_INVALID_PARAM;
    }

    for (uint16_t i = 0U; i < config->signalCount; i++)
    {
        if (config->signals[i].signalId == signalId)
        {
            *indexOut = i;
            return COMM_OK;
        }
    }

    return COMM_INVALID_PARAM;
}

/* Resolves a Group ID from an explicit configuration without reading module state. */
static Comm_ReturnType Com_FindGroupIndexInConfig(
    const Com_ConfigType *config,
    Com_SignalGroupIdType groupId,
    uint16_t *indexOut)
{
    if ((config == NULL) || (indexOut == NULL) ||
        (config->signalGroupCount > COM_MAX_GROUP_COUNT) ||
        ((config->signalGroupCount != 0U) && (config->signalGroups == NULL)))
    {
        return COMM_INVALID_PARAM;
    }

    for (uint16_t i = 0U; i < config->signalGroupCount; i++)
    {
        if (config->signalGroups[i].groupId == groupId)
        {
            *indexOut = i;
            return COMM_OK;
        }
    }

    return COMM_INVALID_PARAM;
}

/* Validates capacities, IDs, ownership, slot layout and timing without changing state. */
static Comm_ReturnType Com_ValidateConfig(const Com_ConfigType *config)
{
    uint8_t signalOwners[COM_MAX_SIGNAL_COUNT] = {0U};
    uint8_t groupOwners[COM_MAX_GROUP_COUNT] = {0U};
    uint8_t scratchBuffer[COM_MAX_IPDU_LENGTH_BYTES];

    if ((config == NULL) ||
        (config->signalCount > COM_MAX_SIGNAL_COUNT) ||
        (config->signalGroupCount > COM_MAX_GROUP_COUNT) ||
        (config->ipduCount > COM_MAX_IPDU_COUNT))
    {
        return COMM_INVALID_PARAM;
    }

    if (((config->signalCount != 0U) && (config->signals == NULL)) ||
        ((config->signalGroupCount != 0U) && (config->signalGroups == NULL)) ||
        ((config->ipduCount != 0U) && (config->ipdus == NULL)))
    {
        return COMM_INVALID_PARAM;
    }

    for (uint16_t i = 0U; i < config->signalCount; i++)
    {
        const Com_SignalConfigType *signal = &config->signals[i];

        for (uint16_t j = 0U; j < i; j++)
        {
            if (config->signals[j].signalId == signal->signalId)
            {
                return COMM_INVALID_PARAM;
            }
        }

        /* The codec checks type, endian, alignment, width and both value limits. */
        if (Com_CodecEncodeSignal(signal, signal->initialValue, false,
                                  scratchBuffer, sizeof(scratchBuffer)) != COMM_OK)
        {
            return COMM_INVALID_PARAM;
        }
    }

    for (uint16_t i = 0U; i < config->signalGroupCount; i++)
    {
        const Com_SignalGroupConfigType *group = &config->signalGroups[i];

        if ((group->signalCount == 0U) ||
            (group->signalCount > COM_MAX_SIGNAL_COUNT) ||
            (group->signalRefs == NULL))
        {
            return COMM_INVALID_PARAM;
        }

        for (uint16_t j = 0U; j < i; j++)
        {
            if (config->signalGroups[j].groupId == group->groupId)
            {
                return COMM_INVALID_PARAM;
            }
        }

        for (uint16_t j = 0U; j < group->signalCount; j++)
        {
            uint16_t signalIndex;

            if (Com_FindSignalIndexInConfig(config, group->signalRefs[j],
                                            &signalIndex) != COMM_OK)
            {
                return COMM_INVALID_PARAM;
            }

            /* Also rejects the same reference repeated within one Group. */
            if (signalOwners[signalIndex] != 0U)
            {
                return COMM_INVALID_PARAM;
            }
            signalOwners[signalIndex] = 1U;
        }
    }

    for (uint16_t i = 0U; i < config->signalCount; i++)
    {
        if (signalOwners[i] != 1U)
        {
            return COMM_INVALID_PARAM;
        }
    }

    for (uint16_t i = 0U; i < config->ipduCount; i++)
    {
        const Com_IPduConfigType *ipdu = &config->ipdus[i];
        bool occupiedBytes[COM_MAX_IPDU_LENGTH_BYTES] = {false};
        uint16_t groupIndex;

        if ((ipdu->lengthBytes == 0U) ||
            (ipdu->lengthBytes > COM_MAX_IPDU_LENGTH_BYTES) ||
            ((ipdu->direction != COM_IPDU_TX) && (ipdu->direction != COM_IPDU_RX)))
        {
            return COMM_INVALID_PARAM;
        }

        if (((ipdu->direction == COM_IPDU_TX) && (ipdu->periodTicks == 0U)) ||
            ((ipdu->direction == COM_IPDU_RX) &&
             ((ipdu->periodTicks != 0U) || (ipdu->initialOffsetTicks != 0U) ||
              (ipdu->maxRetries != 0U))))
        {
            return COMM_INVALID_PARAM;
        }

        for (uint16_t j = 0U; j < i; j++)
        {
            if ((config->ipdus[j].ipduId == ipdu->ipduId) ||
                (config->ipdus[j].globalPduId == ipdu->globalPduId))
            {
                return COMM_INVALID_PARAM;
            }
        }

        if (Com_FindGroupIndexInConfig(config, ipdu->signalGroupRef,
                                       &groupIndex) != COMM_OK)
        {
            return COMM_INVALID_PARAM;
        }
        if (groupOwners[groupIndex] != 0U)
        {
            return COMM_INVALID_PARAM;
        }
        groupOwners[groupIndex] = 1U;

        const Com_SignalGroupConfigType *group = &config->signalGroups[groupIndex];
        for (uint16_t j = 0U; j < group->signalCount; j++)
        {
            uint16_t signalIndex;
            if (Com_FindSignalIndexInConfig(config, group->signalRefs[j],
                                            &signalIndex) != COMM_OK)
            {
                return COMM_INVALID_PARAM;
            }

            const Com_SignalConfigType *signal = &config->signals[signalIndex];
            if (Com_CodecEncodeSignal(signal, signal->initialValue, false,
                                      scratchBuffer, ipdu->lengthBytes) != COMM_OK)
            {
                return COMM_INVALID_PARAM;
            }

            uint16_t startByte = signal->slotStartBit / 8U;
            uint16_t slotBytes = signal->slotLengthBits / 8U;
            for (uint16_t byte = startByte; byte < startByte + slotBytes; byte++)
            {
                if (occupiedBytes[byte])
                {
                    return COMM_INVALID_PARAM;
                }
                occupiedBytes[byte] = true;
            }
        }
    }

    for (uint16_t i = 0U; i < config->signalGroupCount; i++)
    {
        if (groupOwners[i] != 1U)
        {
            return COMM_INVALID_PARAM;
        }
    }

    return COMM_OK;
}

/* Builds initial values with U=0 before publishing the module configuration. */
static Comm_ReturnType Com_InitializeIPduBuffer(
    const Com_ConfigType *config,
    uint16_t ipduIndex)
{
    uint8_t candidate[COM_MAX_IPDU_LENGTH_BYTES] = {0U};
    uint16_t groupIndex;

    if ((Com_ValidateConfig(config) != COMM_OK) || (ipduIndex >= config->ipduCount))
    {
        return COMM_INVALID_PARAM;
    }

    const Com_IPduConfigType *ipdu = &config->ipdus[ipduIndex];
    if (Com_FindGroupIndexInConfig(config, ipdu->signalGroupRef, &groupIndex) != COMM_OK)
    {
        return COMM_INVALID_PARAM;
    }

    const Com_SignalGroupConfigType *group = &config->signalGroups[groupIndex];
    for (uint16_t i = 0U; i < group->signalCount; i++)
    {
        uint16_t signalIndex;
        if (Com_FindSignalIndexInConfig(config, group->signalRefs[i], &signalIndex) != COMM_OK)
        {
            return COMM_INVALID_PARAM;
        }

        const Com_SignalConfigType *signal = &config->signals[signalIndex];
        Comm_ReturnType result = Com_CodecEncodeSignal(
            signal, signal->initialValue, false, candidate, ipdu->lengthBytes);
        if (result != COMM_OK)
        {
            return result;
        }
    }

    memcpy(s_ipduRuntime[ipduIndex].buffer, candidate, sizeof(candidate));
    return COMM_OK;
}

/* Clears every Group U-bit atomically after the lower stack accepts a Tx request. */
static Comm_ReturnType Com_ClearGroupUpdateBits(uint16_t ipduIndex)
{
    uint8_t candidate[COM_MAX_IPDU_LENGTH_BYTES];
    uint16_t groupIndex;

    if ((s_status != COM_INIT) || (s_config == NULL))
    {
        return COMM_NOT_INITIALIZED;
    }
    if ((ipduIndex >= s_config->ipduCount) || (ipduIndex >= COM_MAX_IPDU_COUNT))
    {
        return COMM_INVALID_PARAM;
    }

    const Com_IPduConfigType *ipdu = &s_config->ipdus[ipduIndex];
    if (ipdu->direction != COM_IPDU_TX)
    {
        return COMM_INVALID_PARAM;
    }
    if (Com_FindGroupIndexInConfig(s_config, ipdu->signalGroupRef, &groupIndex) != COMM_OK)
    {
        return COMM_INVALID_PARAM;
    }

    memcpy(candidate, s_ipduRuntime[ipduIndex].buffer, sizeof(candidate));
    const Com_SignalGroupConfigType *group = &s_config->signalGroups[groupIndex];
    for (uint16_t i = 0U; i < group->signalCount; i++)
    {
        uint16_t signalIndex;
        if (Com_FindSignalIndexInConfig(s_config, group->signalRefs[i], &signalIndex) != COMM_OK)
        {
            return COMM_INVALID_PARAM;
        }

        Comm_ReturnType result = Com_CodecClearUpdateBit(
            &s_config->signals[signalIndex], candidate, ipdu->lengthBytes);
        if (result != COMM_OK)
        {
            return result;
        }
    }

    memcpy(s_ipduRuntime[ipduIndex].buffer, candidate, sizeof(candidate));
    return COMM_OK;
}

/* Validates the configuration and initializes all COM runtime data. */
Comm_ReturnType Com_Init(const Com_ConfigType *config)
{
    Comm_ReturnType result;

    if (s_status == COM_INIT)
    {
        return COMM_INVALID_STATE;
    }

    s_status = COM_UNINIT;
    s_config = NULL;

    result = Com_ValidateConfig(config);

    if (result != COMM_OK)
    {
        return result;
    }

    memset(s_ipduRuntime, 0, sizeof(s_ipduRuntime));
    s_invalidInputCount = 0U;

    for (uint16_t i = 0U; i < config->ipduCount; i++)
    {
        result = Com_InitializeIPduBuffer(config, i);

        if (result != COMM_OK)
        {
            memset(s_ipduRuntime, 0, sizeof(s_ipduRuntime));
            return result;
        }

        s_ipduRuntime[i].counter = config->ipdus[i].initialOffsetTicks;
    }

    s_config = config;
    s_status = COM_INIT;
    return COMM_OK;
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
