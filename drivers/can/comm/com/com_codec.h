#ifndef COM_CODEC_H_
#define COM_CODEC_H_

#include "../common/comm_types.h"
#include "com_types.h"

Comm_ReturnType Com_CodecEncodeSignal(
                                        const Com_SignalConfigType *signalConfig,
                                        uint64_t signalValue,
                                        bool updateBit,
                                        uint8_t *ipduBuffer,
                                        uint16_t ipduLengthBytes
                                    );

Comm_ReturnType Com_CodecDecodeSignal(
                                        const Com_SignalConfigType *signalConfig,
                                        const uint8_t *ipduBuffer,
                                        uint16_t ipduLengthBytes,
                                        uint64_t *signalValue,
                                        bool *updateBit
                                    );

Comm_ReturnType Com_CodecClearUpdateBit(
                                        const Com_SignalConfigType *signalConfig,
                                        uint8_t *ipduBuffer,
                                        uint16_t ipduLengthBytes
                                    );

#endif /* COM_CODEC_H_ */