#ifndef NODE_CFG_H_
#define NODE_CFG_H_

#include "../comm/com/com_types.h"
#include "../comm/pdur/pdur_types.h"
#include "../comm/canif/canif_types.h"
#include "../driver/Can_Types.h"

/* Selects the module configurations that form this firmware profile. */
typedef struct
{
    const Com_ConfigType *comConfig;
    const PduR_ConfigType *pduRConfig;
    const CanIf_ConfigType *canIfConfig;
    const Can_ConfigType *canConfig;
} Node_ConfigType;

/* Root object used later to initialize one consistent module profile. */
extern const Node_ConfigType Node_Config;

#endif /* NODE_CFG_H_ */
