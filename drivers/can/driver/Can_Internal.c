#include "Can_Internal.h"

#include <stddef.h>

/**
 * Validate the complete static CAN configuration before any hardware access.
 * The check covers supported controller/timing, timeout, HOH fields, standard
 * Rx IDs/masks, and duplicate local handles or mailbox assignments.
 */
Can_ReturnType Can_InternalValidateConfig(const Can_ConfigType *config)
{
    uint8_t firstIndex;
    uint8_t secondIndex;

    if ((config == NULL) || (config->hohList == NULL) ||
        (config->hohCount == 0U) ||
        (config->hohCount > CAN_SUPPORTED_MB_COUNT) ||
        (config->controllerId != 0U) ||
        (config->baudrate != CAN_SUPPORTED_BAUDRATE) ||
        (config->hardwareTimeoutCount == 0U))
    {
        return CAN_INVALID_PARAM;
    }

    for (firstIndex = 0U; firstIndex < config->hohCount; firstIndex++)
    {
        const Can_HohConfigType *firstHoh = &config->hohList[firstIndex];

        if ((firstHoh->controllerId != config->controllerId) ||
            (firstHoh->mbIndex >= CAN_SUPPORTED_MB_COUNT) ||
            ((firstHoh->type != CAN_HOH_TYPE_TX) &&
             (firstHoh->type != CAN_HOH_TYPE_RX)))
        {
            return CAN_INVALID_PARAM;
        }

        if ((firstHoh->type == CAN_HOH_TYPE_RX) &&
            ((firstHoh->rxCanId > CAN_STANDARD_ID_MAX) ||
             (firstHoh->rxIdMask > CAN_STANDARD_ID_MAX)))
        {
            return CAN_INVALID_PARAM;
        }

        for (secondIndex = (uint8_t)(firstIndex + 1U);
             secondIndex < config->hohCount;
             secondIndex++)
        {
            const Can_HohConfigType *secondHoh = &config->hohList[secondIndex];

            if ((firstHoh->hohId == secondHoh->hohId) ||
                (firstHoh->mbIndex == secondHoh->mbIndex))
            {
                return CAN_INVALID_PARAM;
            }
        }
    }

    return CAN_OK;
}

/**
 * Pack zero to eight payload bytes into the two FlexCAN mailbox data words.
 * The first payload byte occupies the most significant byte of word 0.
 */
Can_ReturnType Can_InternalPackPayload(const uint8_t *data,
                                       uint8_t length,
                                       uint32_t *word0Out,
                                       uint32_t *word1Out)
{
    uint8_t byteIndex;
    uint32_t word0 = 0U;
    uint32_t word1 = 0U;

    if ((word0Out == NULL) || (word1Out == NULL) ||
        (length > CAN_CLASSIC_MAX_DLC) ||
        ((length > 0U) && (data == NULL)))
    {
        return CAN_INVALID_PARAM;
    }

    for (byteIndex = 0U; byteIndex < length; byteIndex++)
    {
        if (byteIndex < 4U)
        {
            word0 |= (uint32_t)data[byteIndex] << ((3U - byteIndex) * 8U);
        }
        else
        {
            word1 |= (uint32_t)data[byteIndex] << ((7U - byteIndex) * 8U);
        }
    }

    *word0Out = word0;
    *word1Out = word1;
    return CAN_OK;
}

/**
 * Unpack two FlexCAN mailbox data words into caller-owned payload storage.
 * Only the requested Classical CAN DLC bytes are written.
 */
Can_ReturnType Can_InternalUnpackPayload(uint32_t word0,
                                         uint32_t word1,
                                         uint8_t length,
                                         uint8_t *dataOut)
{
    uint8_t byteIndex;

    if ((length > CAN_CLASSIC_MAX_DLC) ||
        ((length > 0U) && (dataOut == NULL)))
    {
        return CAN_INVALID_PARAM;
    }

    for (byteIndex = 0U; byteIndex < length; byteIndex++)
    {
        if (byteIndex < 4U)
        {
            dataOut[byteIndex] =
                (uint8_t)((word0 >> ((3U - byteIndex) * 8U)) & 0xFFU);
        }
        else
        {
            dataOut[byteIndex] =
                (uint8_t)((word1 >> ((7U - byteIndex) * 8U)) & 0xFFU);
        }
    }

    return CAN_OK;
}
