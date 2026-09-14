#include "com_codec.h"

Comm_ReturnType Com_CodecEncodeSignal(
                                        const Com_SignalConfigType *signalConfig,
                                        uint64_t signalValue,
                                        bool updateBit,
                                        uint8_t *ipduBuffer,
                                        uint16_t ipduLengthBytes
                                    )
{
    if ((signalConfig == NULL) || (ipduBuffer == NULL))
    {
        return COMM_INVALID_PARAM;
    }

    if ((signalConfig->slotStartBit % 8U) != 0U)
    {
        return COMM_INVALID_PARAM;
    }

    if ((signalConfig->slotLengthBits == 0U) ||
        (signalConfig->slotLengthBits > 64U) ||
        ((signalConfig->slotLengthBits % 8U) != 0U))
    {
        return COMM_INVALID_PARAM;
    }

    uint16_t startByte = signalConfig->slotStartBit / 8U;
    uint16_t slotBytes = signalConfig->slotLengthBits / 8U;

    if (((uint32_t)startByte + (uint32_t)slotBytes) >
        (uint32_t)ipduLengthBytes)
    {
        return COMM_INVALID_PARAM;
    }

    uint16_t payloadBits = signalConfig->slotLengthBits - 1U;

    uint64_t maximumPayload =
        UINT64_MAX >> (64U - payloadBits);

    if (signalValue > maximumPayload)
    {
        return COMM_INVALID_PARAM;
    }

    switch (signalConfig->valueType)
    {
        case COM_UINT8:
            if (signalValue > UINT8_MAX)
            {
                return COMM_INVALID_PARAM;
            }
            break;

        case COM_UINT16:
            if (signalValue > UINT16_MAX)
            {
                return COMM_INVALID_PARAM;
            }
            break;

        case COM_UINT32:
            if (signalValue > UINT32_MAX)
            {
                return COMM_INVALID_PARAM;
            }
            break;

        case COM_UINT64:
            break;

        default:
            return COMM_INVALID_PARAM;
    }

    uint64_t encodedSignal = (signalValue << 1U) | (updateBit ? 1U : 0U);

    if (signalConfig->byteOrder == COM_BYTE_ORDER_LITTLE_ENDIAN)
    {
        for (uint16_t index = 0U; index < slotBytes; index++)
        {
            ipduBuffer[startByte + index] =
                (uint8_t)((encodedSignal >> (8U * index)) & 0xFFU);
        }
    }
    else if (signalConfig->byteOrder == COM_BYTE_ORDER_BIG_ENDIAN)
    {
        for (uint16_t index = 0U; index < slotBytes; index++)
        {
            ipduBuffer[startByte + slotBytes - 1U - index] =
                (uint8_t)(encodedSignal >> (8U * index));
        }
    }
    else
    {
        return COMM_INVALID_PARAM;
    }
    
    return COMM_OK;
}

Comm_ReturnType Com_CodecDecodeSignal(
                                        const Com_SignalConfigType *signalConfig,
                                        const uint8_t *ipduBuffer,
                                        uint16_t ipduLengthBytes,
                                        uint64_t *signalValue,
                                        bool *updateBit
                                    )
{
    if ((signalConfig == NULL) ||
        (ipduBuffer == NULL) ||
        (signalValue == NULL) ||
        (updateBit == NULL))
    {
        return COMM_INVALID_PARAM;
    }

    if ((signalConfig->slotStartBit % 8U) != 0U)
    {
        return COMM_INVALID_PARAM;
    }

    if ((signalConfig->slotLengthBits == 0U) ||
        (signalConfig->slotLengthBits > 64U) ||
        ((signalConfig->slotLengthBits % 8U) != 0U))
    {
        return COMM_INVALID_PARAM;
    }

    uint16_t startByte = signalConfig->slotStartBit / 8U;
    uint16_t slotBytes = signalConfig->slotLengthBits / 8U;

    if (((uint32_t)startByte + (uint32_t)slotBytes) >
        (uint32_t)ipduLengthBytes)
    {
        return COMM_INVALID_PARAM;
    }

    uint64_t encodedSignal = 0U;

    if (signalConfig->byteOrder == COM_BYTE_ORDER_LITTLE_ENDIAN)
    {
        for (uint16_t index = 0U; index < slotBytes; index++)
        {
            encodedSignal |=
                ((uint64_t)ipduBuffer[startByte + index])
                << (8U * index);
        }
    }
    else if (signalConfig->byteOrder == COM_BYTE_ORDER_BIG_ENDIAN)
    {
        for (uint16_t index = 0U; index < slotBytes; index++)
        {
            encodedSignal |=
                ((uint64_t)ipduBuffer[startByte + index])
                << (8U * (slotBytes - 1U - index));
        }
    }
    else
    {
        return COMM_INVALID_PARAM;
    }

    bool decodedUpdateBit = ((encodedSignal & 1U) != 0U);
    uint64_t decodedValue = encodedSignal >> 1U;

    *signalValue = decodedValue;
    *updateBit = decodedUpdateBit;

    return COMM_OK;
}

Comm_ReturnType Com_CodecClearUpdateBit(
                                        const Com_SignalConfigType *signalConfig,
                                        uint8_t *ipduBuffer,
                                        uint16_t ipduLengthBytes
                                    )
{
    if ((signalConfig == NULL) || (ipduBuffer == NULL))
    {
        return COMM_INVALID_PARAM;
    }

    if ((signalConfig->slotStartBit % 8U) != 0U)
    {
        return COMM_INVALID_PARAM;
    }

    if ((signalConfig->slotLengthBits == 0U) ||
        (signalConfig->slotLengthBits > 64U) ||
        ((signalConfig->slotLengthBits % 8U) != 0U))
    {
        return COMM_INVALID_PARAM;
    }

    uint16_t startByte = signalConfig->slotStartBit / 8U;
    uint16_t slotBytes = signalConfig->slotLengthBits / 8U;

    if (((uint32_t)startByte + (uint32_t)slotBytes) >
        (uint32_t)ipduLengthBytes)
    {
        return COMM_INVALID_PARAM;
    }
}