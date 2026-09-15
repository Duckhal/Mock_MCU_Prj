#ifndef COM_CFG_H
#define COM_CFG_H

#include "Com_Types.h"

/* =========================
 * Signal configuration
 * ========================= */

#define COM_NUM_SIGNALS                 (3U)

#define COM_SIGNAL_VEHICLE_SPEED        ((PduIdType)0U)
#define COM_SIGNAL_GEAR                 ((PduIdType)1U)
#define COM_SIGNAL_ALIVE_COUNTER        ((PduIdType)2U)

/* =========================
 * Signal Group configuration
 * ========================= */

#define COM_NUM_SIGNAL_GROUPS           (1U)

#define COM_SIGNAL_GROUP_VEHICLE_STATUS \
    ((PduIdType)0U)

/* =========================
 * I-PDU configuration
 * ========================= */

#define COM_NUM_IPDUS                   (1U)
#define COM_IPDU_VEHICLE_STATUS         ((PduIdType)0U)

/* =========================
 * Configuration tables
 * ========================= */

extern const Com_SignalConfigType Com_SignalConfig[COM_NUM_SIGNALS];

extern const Com_SignalGroupConfigType Com_SignalGroupConfig[COM_NUM_SIGNAL_GROUPS];

extern const Com_IPduConfigType Com_IPduConfig[COM_NUM_IPDUS];

#endif /* COM_CFG_H */