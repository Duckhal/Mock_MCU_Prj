#ifndef CAN_TYPES_H_
#define CAN_TYPES_H_

#include <stdint.h>

typedef enum {
    CAN_OK      = 0,   /* Thành công */
    CAN_NOT_OK  = 1,   /* Lỗi tham số hoặc trạng thái */
    CAN_BUSY    = 2    /* Message Buffer đang bận */
} Can_ReturnType;

typedef enum {
    CAN_HOH_TYPE_TX = 0,   /* Hardware Transmit Handle */
    CAN_HOH_TYPE_RX = 1    /* Hardware Receive Handle  */
} Can_HohType;

typedef struct {
    uint8_t      hohId;    /* Handle ID, dùng để tra cứu từ upper layer */
    Can_HohType  type;     /* TX hoặc RX */
    uint8_t      mbIndex;  /* Message Buffer index trên FlexCAN (0–15) */
    uint32_t     rxCanId;  /* Chỉ dùng cho RX: CAN ID filter (Standard 11-bit) */
    uint32_t     rxIdMask; /* Chỉ dùng cho RX: 0x7FF = exact match */
} Can_HohConfigType;

typedef struct {
    uint32_t                 baudrate;      /* Baud rate, ví dụ: 500000 */
    uint8_t                  loopbackEnable;/* 1 = bật Loopback Mode, 0 = normal */
    const Can_HohConfigType *hohList;       /* Trỏ đến mảng HOH config */
    uint8_t                  hohCount;      /* Số lượng HOH trong mảng */
} Can_ConfigType;

#define CAN_STANDARD_ID_MAX    0x7FFU   /* Max Standard 11-bit CAN ID */
#define CAN_STANDARD_ID_SHIFT  18U      /* Shift để encode vào MB register */

typedef struct {
    uint32_t        id;     /* CAN ID sẽ dùng khi TX (Standard 11-bit, max 0x7FF) */
    uint8_t         length; /* Số byte data, 0–8 */
    const uint8_t  *sdu;    /* Pointer đến data buffer (const: driver chỉ đọc) */
} Can_PduType;

typedef struct {
    uint32_t canId;   /* CAN ID của frame nhận được (đọc từ MB register) */
    uint8_t  hohId;   /* HRH ID của MB đã nhận frame */
} Can_HwType;



#endif