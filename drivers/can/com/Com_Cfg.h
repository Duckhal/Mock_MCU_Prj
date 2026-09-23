#ifndef COM_CFG_H_
#define COM_CFG_H_

#include "Com_Types.h"

/* Application signals for the three fixed ECU roles. */
#define COM_NUM_SIGNALS                    (8U)
#define COM_SIGNAL_TX_ALIVE_COUNTER        ((PduIdType)0U)
#define COM_SIGNAL_TX_KEEPALIVE_RATE       ((PduIdType)1U)
#define COM_SIGNAL_RX_ALIVE_COUNTER        ((PduIdType)2U)
#define COM_SIGNAL_RX_KEEPALIVE_RATE       ((PduIdType)3U)
#define COM_SIGNAL_TX_SLAVE1_STATUS        ((PduIdType)4U)
#define COM_SIGNAL_RX_SLAVE1_STATUS        ((PduIdType)5U)
#define COM_SIGNAL_TX_SLAVE2_STATUS        ((PduIdType)6U)
#define COM_SIGNAL_RX_SLAVE2_STATUS        ((PduIdType)7U)

#define COM_KEEPALIVE_RATE_LEVEL_MAX       (6U)
#define COM_SLAVE_STATUS_NORMAL            (0U)
#define COM_SLAVE_STATUS_MASTER_LOST       (1U)

/* One Signal Group owns each direction of each wire I-PDU. */
#define COM_NUM_SIGNAL_GROUPS              (6U)
#define COM_SIGNAL_GROUP_TX_KEEPALIVE      ((PduIdType)0U)
#define COM_SIGNAL_GROUP_RX_KEEPALIVE      ((PduIdType)1U)
#define COM_SIGNAL_GROUP_TX_SLAVE1_STATUS  ((PduIdType)2U)
#define COM_SIGNAL_GROUP_RX_SLAVE1_STATUS  ((PduIdType)3U)
#define COM_SIGNAL_GROUP_TX_SLAVE2_STATUS  ((PduIdType)4U)
#define COM_SIGNAL_GROUP_RX_SLAVE2_STATUS  ((PduIdType)5U)

/* Local COM I-PDU IDs; CAN IDs are bound by CanIf_Cfg.c. */
#define COM_NUM_IPDUS                      (6U)
#define COM_IPDU_TX_KEEPALIVE              ((PduIdType)0U)
#define COM_IPDU_RX_KEEPALIVE              ((PduIdType)1U)
#define COM_IPDU_TX_SLAVE1_STATUS          ((PduIdType)2U)
#define COM_IPDU_RX_SLAVE1_STATUS          ((PduIdType)3U)
#define COM_IPDU_TX_SLAVE2_STATUS          ((PduIdType)4U)
#define COM_IPDU_RX_SLAVE2_STATUS          ((PduIdType)5U)

#define COM_KEEPALIVE_TX_PERIOD_TICKS      (10U)
#define COM_SLAVE_STATUS_TX_PERIOD_TICKS   (500U)

extern const Com_SignalConfigType Com_SignalConfig[COM_NUM_SIGNALS];
extern const Com_SignalGroupConfigType
    Com_SignalGroupConfig[COM_NUM_SIGNAL_GROUPS];
extern const Com_IPduConfigType Com_IPduConfig[COM_NUM_IPDUS];

#endif /* COM_CFG_H_ */
