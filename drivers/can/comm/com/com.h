#ifndef COM_H_
#define COM_H_

#include <stdint.h>
#include "com_types.h"
#include "../common/comm_types.h"

/*
 * Khởi tạo COM bằng configuration tĩnh.
 *
 * Sau khi thành công:
 * - các Tx I-PDU có initial value;
 * - các Rx signal có default value;
 * - không I-PDU nào đang pending.
 */
Comm_ReturnType Com_Init(const Com_ConfigType *config);

/*
 * Cập nhật một Tx signal.
 *
 * COM tra configuration để:
 * - kiểm tra kiểu/kích thước;
 * - tìm Tx I-PDU chứa signal;
 * - pack signal vào shadow buffer;
 * - đánh dấu I-PDU cần phát.
 *
 * Hàm này chưa gọi CanIf/CAN Driver.
 */
Comm_ReturnType Com_SendSignal(
    Com_SignalIdType signalId,
    const void *signalData,
    uint16_t signalDataSize);

/*
 * Đọc giá trị mới nhất của một Rx signal.
 */
Comm_ReturnType Com_ReceiveSignal(
    Com_SignalIdType signalId,
    void *signalDataOut,
    uint16_t outputCapacity);

/*
 * Xử lý các Tx I-PDU periodic/triggered.
 * Đây là nơi COM gọi PduR_ComTransmit().
 */
void Com_MainFunctionTx(uint32_t nowMs);

/*
 * Xử lý Rx timeout/freshness.
 * Có thể chưa hiện thực trong loopback đầu tiên,
 * nhưng interface hữu ích khi giao tiếp ba ECU.
 */
void Com_MainFunctionRx(uint32_t nowMs);

/*
 * Callback do PduR gọi khi nhận một Rx I-PDU.
 *
 * COM phải copy/unpack dữ liệu trong thời gian callback;
 * không được giữ lại pduInfo->data.
 */
void Com_RxIndication(
    PduIdType rxIPduId,
    const PduInfoType *pduInfo);

/*
 * Callback do PduR gọi khi quá trình phát I-PDU kết thúc.
 */
void Com_TxConfirmation(
    PduIdType txIPduId,
    Comm_ReturnType result);

#endif /* COM_H_ */