#ifndef CAN_INTERNAL_H_
#define CAN_INTERNAL_H_

#include "Can_Types.h"

/** Validate all static CAN configuration fields without hardware access. */
Can_ReturnType Can_InternalValidateConfig(const Can_ConfigType *config);

/** Pack a Classical CAN payload into two FlexCAN mailbox data words. */
Can_ReturnType Can_InternalPackPayload(const uint8_t *data,
                                       uint8_t length,
                                       uint32_t *word0Out,
                                       uint32_t *word1Out);

/** Unpack two FlexCAN mailbox data words into a byte payload. */
Can_ReturnType Can_InternalUnpackPayload(uint32_t word0,
                                         uint32_t word1,
                                         uint8_t length,
                                         uint8_t *dataOut);

#endif /* CAN_INTERNAL_H_ */
