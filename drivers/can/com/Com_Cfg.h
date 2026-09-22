#ifndef COM_CFG_H_
#define COM_CFG_H_

#include "Com_Types.h"

/* =========================
 * Signal configuration
 * ========================= */

#define COM_NUM_SIGNALS                 (2U)

/* Keep the transmitted LED Signal ID non-zero so enabled Global ID is visible. */
#define COM_SIGNAL_LED_COMMAND          ((PduIdType)1U)
#define COM_SIGNAL_RX_LED_COMMAND       ((PduIdType)2U)

/* Byte 0 is zero by default; enable this to encode COM_SIGNAL_LED_COMMAND. */
#ifndef COM_GLOBAL_ID_ENABLED
#define COM_GLOBAL_ID_ENABLED           (0U)
#endif
#if ((COM_GLOBAL_ID_ENABLED != 0U) && (COM_GLOBAL_ID_ENABLED != 1U))
#error "COM_GLOBAL_ID_ENABLED must be 0U or 1U"
#endif

#define COM_LED_MODE_STEADY             (0U)
#define COM_LED_MODE_BLINK_500_MS       (1U)
#define COM_LED_MODE_BLINK_1000_MS      (2U)
#define COM_LED_MODE_BLINK_2000_MS      (3U)
#define COM_LED_MODE_MAX                COM_LED_MODE_BLINK_2000_MS
#define COM_LED_STATE_OFF               (0U)
#define COM_LED_STATE_ON                (1U)

/* The logical uint32 Signal value maps to wire bytes [mode][state]. */
#define COM_LED_COMMAND_ENCODE(mode, state) \
    ((uint32_t)(mode) | ((uint32_t)(state) << 8U))
#define COM_LED_COMMAND_GET_MODE(value) ((uint8_t)((value) & 0xFFU))
#define COM_LED_COMMAND_GET_STATE(value) ((uint8_t)(((value) >> 8U) & 0xFFU))

/* =========================
 * Signal Group configuration
 * ========================= */

#define COM_NUM_SIGNAL_GROUPS           (2U)

#define COM_SIGNAL_GROUP_LED_CONTROL       ((PduIdType)0U)
#define COM_SIGNAL_GROUP_RX_LED_CONTROL    ((PduIdType)1U)

/* =========================
 * I-PDU configuration
 * ========================= */

#define COM_NUM_IPDUS                   (2U)
#define COM_IPDU_VEHICLE_STATUS         ((PduIdType)0U)
#define COM_IPDU_RX_VEHICLE_STATUS      ((PduIdType)1U)

/* =========================
 * Configuration tables
 * ========================= */

extern const Com_SignalConfigType Com_SignalConfig[COM_NUM_SIGNALS];
extern const Com_SignalGroupConfigType Com_SignalGroupConfig[COM_NUM_SIGNAL_GROUPS];
extern const Com_IPduConfigType Com_IPduConfig[COM_NUM_IPDUS];

#endif /* COM_CFG_H_ */
