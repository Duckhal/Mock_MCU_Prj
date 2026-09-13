#ifndef CANUPPER_TYPES_H_
#define CANUPPER_TYPES_H_

#include <stdint.h>

typedef uint8_t PduIdType;

/* Trạng thái TX của một PDU */
typedef enum {
    PDU_TX_IDLE    = 0,   /* Chưa có gì để gửi */
    PDU_TX_PENDING = 1,   /* Đang chờ TX */
    PDU_TX_DONE    = 2    /* TX đã hoàn tất */
} CanUpper_TxStatusType;

/* Một entry trong TX PDU table — mỗi PDU map đến đúng một HTH */
typedef struct {
    PduIdType             pduId;   /* PDU ID (từ CanUpper_Cfg.h) */
    uint32_t              canId;   /* CAN ID sẽ dùng khi TX frame này */
    uint8_t               hthId;   /* HTH ID map đến TX HOH trong driver */
    CanUpper_TxStatusType status;  /* Trạng thái TX hiện tại */
} CanUpper_TxPduType;

/* Một entry trong RX PDU table — mỗi PDU map đến đúng một HRH */
typedef struct {
    PduIdType  pduId;          /* PDU ID (từ CanUpper_Cfg.h) */
    uint8_t    hrhId;          /* HRH ID map đến RX HOH trong driver */
    uint8_t    data[8];        /* Buffer lưu data của frame nhận được */
    uint8_t    length;         /* Số byte nhận được */
    uint8_t    newDataFlag;    /* 1 = có data mới chưa đọc, 0 = chưa có */
} CanUpper_RxPduType;

#endif /* CANUPPER_TYPES_H_ */
