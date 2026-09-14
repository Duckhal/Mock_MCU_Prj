#include "node_cfg.h"

#include "can/Can_Cfg.h"
#include "canif/canif_cfg.h"
#include "com/com_cfg.h"
#include "pdur/pdur_cfg.h"

/*
 * Root configuration for the current Tx-oriented training profile.
 * Module initialization logic will consume these pointers later.
 */
const Node_ConfigType Node_Config =
{
    .comConfig = &Com_Config,
    .pduRConfig = &PduR_Config,
    .canIfConfig = &CanIf_Config,
    .canConfig = &Can_Config_Normal
};
