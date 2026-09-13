#ifndef CANUPPER_CFG_H_
#define CANUPPER_CFG_H_

#include "../../driver/inc/Can_Cfg.h"

/* ── TX PDU IDs ─────────────────────────────────────────────── */
#define PDU_TX_LED_COMMAND     0U   /* Gửi lệnh điều khiển LED, CAN ID 0x123, dùng CAN_HTH_0 */

/* ── RX PDU IDs ─────────────────────────────────────────────── */
#define PDU_RX_LED_COMMAND     1U   /* Nhận lệnh LED, CAN ID 0x123, từ CAN_HRH_0 */

#endif /* CANUPPER_CFG_H */