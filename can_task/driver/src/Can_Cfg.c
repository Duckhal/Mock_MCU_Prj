#include "../inc/Can_Cfg.h"

/*
 * HOH configuration table
 *   hohId    : handle number (dùng trong CanUpper)
 *   type     : TX hoặc RX
 *   mbIndex  : MB index vật lý trên FlexCAN0 (0–15)
 *   rxCanId  : chỉ dùng cho RX — CAN ID filter (Standard 11-bit)
 *   rxIdMask : chỉ dùng cho RX — 0x7FF = exact match
 *
 * HTH không cần rxCanId/rxIdMask vì CAN ID khi TX
 * do upper layer quyết định qua Can_PduType.id
 */
const Can_HohConfigType Can_HohConfig[] = {
    /* hohId       type              mbIndex  rxCanId  rxIdMask */
    { CAN_HTH_0,  CAN_HOH_TYPE_TX,  0U,      0x000U,  0x000U },  /* TX MB[0] */
    { CAN_HRH_0,  CAN_HOH_TYPE_RX,  1U,      0x123U,  0x7FFU },  /* RX MB[1], exact match 0x123 */
};

/* Normal mode */
const Can_ConfigType Can_Config = {
    .baudrate       = 500000U,
    .loopbackEnable = 0U,
    .hohList        = Can_HohConfig,
    .hohCount       = 2U,
};

/* Loopback mode */
const Can_ConfigType Can_Config_Loopback = {
    .baudrate       = 500000U,
    .loopbackEnable = 1U,
    .hohList        = Can_HohConfig,
    .hohCount       = 2U,
};