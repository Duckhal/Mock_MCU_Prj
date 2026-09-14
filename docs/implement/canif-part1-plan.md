# Plan triển khai CanIf — Part 1

Trạng thái: **PLAN**. Nguồn: [assignment](../../requirements/assignment_part1_com_signal.md) §4,18–27,30–39; [notes](../../requirements/part1_architecture_notes.md) §19–29. Liên quan: [PduR](pdur-part1-plan.md), [CAN Driver](can-driver-part1-plan.md).

## 1. Mục tiêu và hiện trạng

CanIf sở hữu logical L-PDU→CAN mapping. Tx: TxPduId→CAN ID+HTH. Rx: HRH+CAN ID→RxPduId. Driver sở hữu HOH/controller, PduR sở hữu route; CanIf chỉ tham chiếu các object đó.

Chưa có CanIf implementation; `drivers/can/config/canif/canif_cfg.c/.h` rỗng. Driver hiện có Can_Write với swPduHandle, register callbacks, mode/stats; callback Rx hiện dùng `(Can_HwType*, Can_PduType*)` khác training API `(Hrh, Can_RxPduType*)`.

Không reuse trực tiếp CanUpper làm CanIf vì nó gộp mapping và consumer buffer. Có thể tham khảo lookup từ `can_task/upper` nhưng phải giữ callback về PduR và kiểm HRH+CAN ID.

## 2. Phạm vi và quyết định

- Classic CAN standard11, DLC0..8 ở CanIf; profile COM có exact DLC 1..8.
- Không Tx queue, không retry, không polling scheduler riêng. Can_Write BUSY → local COMM_BUSY/non-OK → COM xử lý.
- Một outstanding request/TxPduId; nhiều TxPduId có thể tham chiếu một HTH nhưng phần cứng quyết định BUSY.
- Chấp nhận giữ runtime metadata (inFlight/handle/counters); đây không phải payload queue.
- PduR là upper owner duy nhất trong Part 1. Không thêm CanTp dispatch trước khi có yêu cầu Part 2.
- Đề xuất giữ public training Rx API và dùng adapter **private** nối callback driver hiện có. Không để COM/PduR phụ thuộc Can_HwType.
- GlobalPduId chỉ phục vụ cấu hình/trace. CanIf không serialize Global ID.

## 3. File inventory

**12 file: 3 có sẵn cần sửa, 9 mới**; shared common header dùng chung các plan.

| # | File | Loại | Chức năng |
|---:|---|---|---|
| 1 | `drivers/can/comm/canif/canif.h` | Mới | Init/Transmit/Rx/confirmation/mode/status/stats public API |
| 2 | `drivers/can/comm/canif/canif_types.h` | Mới | L-PDU config, runtime diagnostics; tham chiếu Can_Types.h không MCU header |
| 3 | `drivers/can/comm/canif/canif.c` | Mới | Init/lookup/validation, Tx ownership, Rx dispatch, confirmation |
| 4 | `drivers/can/comm/canif/canif_can_adapter.h` | Mới | Private callback adapter/registration contract |
| 5 | `drivers/can/comm/canif/canif_can_adapter.c` | Mới | Chuyển Can_HwType+Can_PduType sang training HRH+RxPdu, đăng ký callbacks |
| 6 | `drivers/can/config/canif/canif_cfg.h` | Sửa rỗng | Named logical PDU IDs, const config export |
| 7 | `drivers/can/config/canif/canif_cfg.c` | Sửa rỗng | Tx CAN ID+HTH, Rx HRH+CAN ID, exact DLC, GlobalPduId |
| 8 | `drivers/can/comm/common/comm_types.h` | Sửa/shared | Thêm terminal result mapping nếu cần: CANCELLED/BUS_OFF/TIMEOUT; không cast enum |
| 9 | `tests/host/canif/fake_endpoints.h` | Mới | Điều khiển fake CAN và quan sát fake PduR |
| 10 | `tests/host/canif/fake_endpoints.c` | Mới | Mock Can_Write/register/mode, capture borrowed buffers, emit completions |
| 11 | `tests/host/canif/test_canif.c` | Mới | Mapping/ownership/adapter/error test cases |
| 12 | `tests/host/canif/run_tests.ps1` | Mới | Deterministic runner, lưu logs |

`Can_RxPduType` thêm vào driver-owned `Can_Types.h` theo CAN Driver plan, chỉ định nghĩa một lần; không tính lại thành file mới CanIf. Report tương lai: `docs/implement/canif-part1-validation.md`.

## 4. Model và contracts

Tx config `{txPduId, globalPduId, canId, hthRef, length}`; Rx config `{rxPduId, globalPduId, canId, hrhRef, length}`. Global ID có thể trùng giữa Tx/Rx endpoint của cùng logical message, nhưng không được map một Global ID sang hai wire message khác nhau. Validator hệ thống xác nhận điều này.

Local config validation: count/pointer/limits; unique Tx ID và Rx ID theo direction; standard CAN ID; DLC range; unique Rx `(HRH,CAN ID)`; direct-binding CAN ID ambiguity. Controller validity và Tx/Rx direction của HOH được cross-check với config driver bởi validator, không suy handle number chẵn/lẻ.

API dự kiến:

```c
Comm_ReturnType CanIf_Init(const CanIf_ConfigType *config);
Comm_ReturnType CanIf_Transmit(PduIdType txPduId, const PduInfoType *pdu);
void CanIf_RxIndication(Can_HwHandleType hrh, const Can_RxPduType *rxPdu);
void CanIf_TxConfirmation(Can_SwPduHandleType handle, Can_ReturnType result);
void CanIf_ControllerBusOff(uint8_t controllerId);
Comm_ReturnType CanIf_SetControllerMode(uint8_t id, Can_ControllerModeType mode);
Comm_ReturnType CanIf_GetControllerStatus(uint8_t id, Can_ControllerStatusType *out);
Comm_ReturnType CanIf_GetStats(CanIf_StatsType *out);
```

CanIf init không gọi Can_Init lần hai. Driver đã init STOPPED; validate CanIf config trước, đăng ký adapter callbacks, publish INIT cuối cùng. Không auto-start controller trước khi PduR/COM sẵn sàng.

Enum return map tường minh: CAN_OK→COMM_OK; BUSY→COMM_BUSY; INVALID_PARAM/NOT_INITIALIZED/INVALID_STATE tương ứng; NOT_OK→COMM_NOT_OK. Terminal CANCELLED/BUS_OFF/TIMEOUT dùng shared result bổ sung, không giả thành accepted. Không so numeric values giữa hai enum khác nhau.

Rx data pointer const, borrowed; adapter tạo metadata local rồi gọi public Rx synchronously. Nếu sau này driver đổi trực tiếp sang training callback, bỏ adapter dư thừa trong task migration có tests, không duy trì hai đường dispatch cùng event.

## 5. Giai đoạn triển khai

### Giai đoạn 1 — Types/config/adapter contract

Viết #1–2,4,6–8. Tạo fake endpoint API #9–10. Chốt mapping ví dụ TxPdu7/CAN0x321/HTH0; RxPdu9/CAN0x321/HRH1. IDs chỉ minh họa trước khi chốt matrix hệ thống.

**Exit:** headers compile độc lập MCU; typed callback signatures khớp không cast function pointer.

### Giai đoạn 2 — Init/lookup và driver registration

Viết #3,5: validation/FindTx/FindRx, register callbacks, initialized state. Tx/Rx-only config được phép; nếu driver config có cả Tx/Rx HOH thì đăng ký cả callback bắt buộc dù mapping upper chỉ một direction.

**Exit:** reject invalid config trước publish; registration lỗi để CanIf UNINIT và không auto-start; second init không xóa inFlight.

### Giai đoạn 3 — Transmit và acceptance

1. Validate init/id/pointer/exact DLC; state STARTED do driver xác nhận.
2. Nếu txPduId đang inFlight thì trả COMM_BUSY.
3. Construct Can_PduType với canId từ config, payload borrowed và swPduHandle=txPduId (hai type uint16 hiện tương thích; validate limits).
4. Gọi Can_Write(hth) đúng một lần. CAN_OK mới đánh dấu inFlight; failure không giữ pointer hoặc tạo outstanding record.
5. Payload đã copy trước return OK nhờ driver; không thêm Tx buffering ở CanIf.

**Exit:** shared HTH không overwrite metadata; BUSY không bị enqueue; lower calls từ main không callback completion đồng bộ trong Can_Write.

### Giai đoạn 4 — Tx completion, stop và bus-off

Resolve swPduHandle→TxPduId; chỉ accept terminal event khi có inFlight; clear trước forward PduR confirmation. Unknown/no inFlight → stale counter, không dispatch lần hai.

BusOff callback chỉ latch controller event/diagnostics, không tự tạo thêm terminal confirmation cho mỗi PDU nếu driver đã sở hữu nghĩa vụ đó. Drain confirmations từ Can_MainFunction_Write ở cả STOPPED/FAULT.

Không chống stale event sau tái sử dụng cùng handle chỉ bằng bool: driver phải vô hiệu event cũ trước nhận request mới. Test fault/recovery ở Driver plan là dependency của correctness này.

**Exit:** accepted request có một terminal callback; rejected request không có callback; no retransmit sau accepted failure vì retry COM chỉ áp dụng rejection trước acceptance.

### Giai đoạn 5 — Rx mapping và lifetime

Adapter validate pointers, convert HRH/frame metadata, gọi public Rx. Public Rx kiểm init, ID/DLC/profile, lookup cả HRH+CAN ID, rồi gọi PduR_CanIfRxIndication(mappedRxId,pdu).

Không dispatch chỉ theo HRH. Unknown mapping/length mismatch drop/count; không cast payload thành signal. Callback chain return trước stack-local buffer hết lifetime.

**Exit:** một HRH nhận 0x100/0x321 đưa đúng hai logical Rx ID; same CAN ID với HRH khác không bị nhầm; upper snapshot byte đúng.

### Giai đoạn 6 — Ghép real PduR/COM và fake CAN

Test COM→PduR→CanIf→fake Can_Write và Rx ngược. BUSY return lên nguyên semantics; acceptance clear U chỉ ở COM; completion nhận đúng COM source ID.

**Exit:** host tests, ARM object compile và report pass; driver/hardware integration theo plan riêng.

## 6. Test matrix

| Case | Expected |
|---|---|
| CANIF-01 Invalid config/duplicate pair/ID/length | Init fail deterministic |
| CANIF-02 TxPdu7→CAN0x321/HTH0 | Fake thấy đúng fields và swHandle7 |
| CANIF-03 Shared HTH cho hai logical PDU | BUSY không mất request đầu, không queue request thứ hai |
| CANIF-04 Không STARTED/null/size mismatch | Không accept, không giữ pointer |
| CANIF-05 Confirmation khi chưa inFlight/duplicate | Không forward; tăng stale |
| CANIF-06 Accepted→success/cancel/busoff | Một terminal event, đúng reverse route |
| CANIF-07 HRH1 + hai CAN ID | Resolve hai RxPduId khác nhau |
| CANIF-08 Unknown HRH/ID/DLC | Drop/counter, không gọi COM |
| CANIF-09 Callback adapter | Metadata đúng, borrowed buffer được tiêu thụ ngay |
| CANIF-10 Reuse HTH sau stop/recovery | Event cũ không complete request mới; cần driver tests |
| CANIF-11 Multi-controller model | HOH lookup xác định controller, Tx API không thêm controllerId |
| CANIF-12 COM/PduR/CanIf real + fake CAN | Identity trace và retry semantics xuyên ba tầng |

## 7. Hoàn thành

Correct logical/hardware mapping, không queue/retry ở CanIf, callbacks một đường duy nhất, return/terminal enum map tường minh, config binding pass và log tests thật. BasicCAN/multi-controller hardware chỉ được đánh dấu chạy sau khi có bằng chứng tầng driver tương ứng.
