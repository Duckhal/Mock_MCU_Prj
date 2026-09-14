# Plan triển khai PduR — Part 1 Direct CAN Binding

Trạng thái: **TYPES/CONFIG ĐÃ CÓ, LOGIC CHƯA TRIỂN KHAI**. `pdur_types.h` và direct Tx route config đã được tạo; public API, lookup, validation runtime và callback routing vẫn theo các giai đoạn bên dưới. Đọc cùng [COM plan](com-part1-plan.md), [CanIf plan](canif-part1-plan.md) và [mục lục](README.md).

## 1. Yêu cầu, hiện trạng và ranh giới

Nguồn chính: [assignment](../../requirements/assignment_part1_com_signal.md) §4,17,25,30–31,36,38–39; [architecture notes](../../requirements/part1_architecture_notes.md) §21–22,30–34.

PduR sở hữu route logical PDU; Direct Binding chuyển payload nguyên vẹn. PduR không đọc signal/U, không biết CAN ID/HTH/HRH/controller, không thêm GlobalPduId vào wire data. GlobalPduId trong route chỉ để đối chiếu model và trace.

Tip phase 10 ms/offset 1..9 của người giao bài không tạo field hay validation trong PduR. PduR route ngay từng request COM đưa xuống; period, offset, nominal phase và retry vẫn hoàn toàn thuộc COM/config hệ thống.

Hiện tại `drivers/can/comm/pdur/pdur_types.h` và `drivers/can/config/pdur/pdur_cfg.c/.h` định nghĩa profile Tx COM0→CanIf7, route11, Global0x0010. Chưa có `pdur.c/.h` hoặc runtime state. `API_SPEC.md` đã phác thảo direct route nhưng chứa cả CanTp/Transfer ngoài Part 1.

Phạm vi: route COM→CanIf, CanIf→COM và TxConfirmation ngược về COM. Một route một destination; không fan-out/gateway/multiplexing/queue/retry/deadline monitor. PduR không cần MainFunction vì không có việc deferred riêng trong baseline.

## 2. Contract trước khi code

```text
Tx: COM IPduId → PduR source lookup → CanIfTxPduId
Rx: CanIfRxPduId → PduR source lookup → COM RxIPduId
Confirmation: CanIfTxPduId → reverse Tx route → COM TxIPduId
```

- Tx source handle ở `PduR_ComTransmit()` chính là COM I-PDU ID theo COM plan, không phải array index PduR. Route ID nội bộ có thể khác số.
- Rx source ID chính là CanIf Rx ID. Tx/Rx lookup ở hai namespace theo direction.
- Chỉ `COMM_OK` nghĩa accepted. BUSY và các non-OK đều phải trả failure cho COM để COM quyết định bounded retry. Assignment minh họa E_NOT_OK; local API giữ mã BUSY chi tiết nhưng semantics không đổi.
- PduR không giữ payload pointer sau call. CanIf/CAN phải hoàn thành copy trước Tx return OK; COM có thể clear U ngay sau đó.
- Rx callback borrowed: route ngay sang COM, COM copy/decode trước return.
- Static route không thay runtime. Mỗi CanIf Tx destination chỉ có một COM source trong baseline để confirmation reverse không mơ hồ.
- PduR không cần lưu request snapshot: trạng thái hardware outstanding thuộc CanIf/CanDrv. Duplicate/stale confirmation được CanIf chặn; PduR vẫn validate route/ID trước dispatch.

## 3. File inventory

**11 file được chạm: 3 có sẵn, 8 mới so với snapshot lập plan.** `comm_types.h` và `pdur_com.h` dùng chung với COM plan, không tạo bản thứ hai.

| # | File | Loại | Nhiệm vụ |
|---:|---|---|---|
| 1 | `drivers/can/comm/pdur/pdur.h` | Mới | Init, CanIf Rx/confirmation callbacks, stats API |
| 2 | `drivers/can/comm/pdur/pdur_types.h` | Mới | Tx/Rx route config, counts, stats/events |
| 3 | `drivers/can/comm/pdur/pdur_com.h` | Mới, shared COM plan | PduR_ComTransmit declaration do PduR sở hữu |
| 4 | `drivers/can/comm/pdur/pdur.c` | Mới | Validate, lookup, forward/reverse dispatch, counters |
| 5 | `drivers/can/config/pdur/pdur_cfg.h` | Sửa rỗng | Named route IDs, const export |
| 6 | `drivers/can/config/pdur/pdur_cfg.c` | Sửa rỗng | Direct Tx/Rx route tables |
| 7 | `drivers/can/comm/common/comm_types.h` | Sửa/shared | PduIdType, GlobalPduIdType, Comm_ReturnType; đồng bộ với COM |
| 8 | `tests/host/pdur/fake_endpoints.h` | Mới | Điều khiển fake CanIf và quan sát fake COM |
| 9 | `tests/host/pdur/fake_endpoints.c` | Mới | Capture calls/copy bytes, return codes; không route hộ PduR |
| 10 | `tests/host/pdur/test_pdur.c` | Mới | Config, Tx, Rx, confirmation, IDs/range/lifetime cases |
| 11 | `tests/host/pdur/run_tests.ps1` | Mới | Build/run từng case process mới, lưu log và exit code |

Validation report sau thực thi: `docs/implement/pdur-part1-validation.md`. Source root `drivers` tự thu source mới: chỉ enable firmware source khi CanIf/COM thật có symbol, hoặc compile object trước; không link fake vào firmware để né lỗi unresolved.

## 4. Data model và API

Tx route config: `{routeId, globalPduId, comTxIPduId, canIfTxPduId}`. Rx route: `{routeId, globalPduId, canIfRxPduId, comRxIPduId}`. Dùng hai bảng để direction và reverse mapping dễ review.

Local limit đề xuất 8 Tx routes, 8 Rx routes; static const config, không heap. Không thêm bit layout, CAN ID hoặc HOH vào struct. Local IDs uint16 không nhất thiết liên tiếp.

```c
Comm_ReturnType PduR_Init(const PduR_ConfigType *config);
Comm_ReturnType PduR_ComTransmit(PduIdType sourceIPduId,
                                const PduInfoType *pdu);
void PduR_CanIfRxIndication(PduIdType rxPduId, const PduInfoType *pdu);
void PduR_CanIfTxConfirmation(PduIdType txPduId, Comm_ReturnType result);
Comm_ReturnType PduR_GetStats(PduR_StatsType *out);
```

Init chỉ kiểm tra tính hợp lệ local: pointers/count/limit, duplicate route/source/destination, direction và reverse mapping. Tồn tại destination ở CanIf/COM và GlobalPduId consistency toàn graph được validator hệ thống kiểm trước build; PduR không include private config struct của cả stack để tự validate xuyên tầng.

Packet validation: pdu khác NULL, data khác NULL nếu length>0; không tự sửa length hoặc bytes. Exact I-PDU length thuộc COM/CanIf config, không nhân bản policy ở PduR. Shared contract cho phép zero length; COM Part 1 có group không rỗng nên không tạo PDU rỗng.

Counters tối thiểu: txCalls, txAccepted, txRejected, rxRouted, confirmationsRouted, invalidInput, routeMiss. Event record routeId/globalPduId/sourceId/destId/result; không gọi UART trực tiếp.

## 5. Các giai đoạn viết code

### Giai đoạn 1 — Contract và route tables

Viết files #1–3,5–7; thống nhất chữ ký với COM/CanIf plan. Config ví dụ COM Tx ID3 → CanIf Tx ID7, Global0x0010; route local ID11. ID chỉ là fixture, không phải mapping ba ECU đã được chốt.

Viết fake endpoint capture và runner (#8–11). **Exit:** headers/const config compile, endpoint stubs chỉ ở test target.

### Giai đoạn 2 — Init và validate

Viết private ValidateConfig/FindTxRoute/FindRxRoute/FindReverseTxRoute trong #4. Validate toàn bảng trước publish INIT; invalid config giữ UNINIT. Init lặp trả INVALID_STATE và không reset stats/config đang hoạt động.

Test count0 hợp lệ cho direction không dùng, cả hai count0 bị từ chối để tránh node cấu hình nhầm; nonzero count phải có pointer. **Exit:** tất cả negative graph cases pass, ID thưa không đọc ngoài mảng.

### Giai đoạn 3 — Forward Tx

PduR_ComTransmit: check init/input → resolve source → gọi CanIf_Transmit đúng một lần → cập nhật counters → trả nguyên kết quả local. Không retry, không set Update Bit, không copy dài hạn.

**Exit:** fake thấy đúng destination/length/bytes với các payload chứa U=0/U=1; BUSY không gây call lần hai, return không đổi thành OK.

### Giai đoạn 4 — Rx và reverse confirmation

Rx: validate input → lookup Rx route → Com_RxIndication(destination,pdu). Confirmation: lookup reverse Tx route → Com_TxConfirmation(source,result). Không dùng HRH làm PduId, không route Tx callback qua Rx table.

**Exit:** source/destination IDs khác nhau vẫn đúng; unknown ID drop/count; callback payload lifetime được endpoint copy trong lời gọi.

### Giai đoạn 5 — Tích hợp COM + PduR + fake CanIf

Thay fake PduR của COM test bằng real PduR; giữ fake CanIf ở đáy. Test BUSY sequence 10/11 rồi OK12 và kiểm COM clear U sau acceptance, không sau route lookup.

**Exit:** direct payload-transparent đúng từng byte; no schedule/retry duplicated trong PduR; host + ARM object compile sạch; report ghi đầy đủ test IDs.

## 6. Test matrix

| ID | Trường hợp | Expected |
|---|---|---|
| PDUR-01 | Null/count/duplicate source/dest/reverse ambiguity | Init reject trước publish |
| PDUR-02 | COM source3 → CanIf7, route11 | Fake nhận ID7, bytes nguyên vẹn |
| PDUR-03 | COMM_BUSY/NOT_OK từ CanIf | Trả failure, đúng một call |
| PDUR-04 | Null data với length>0/unknown ID/uninit | Không gọi endpoint; error/counter |
| PDUR-05 | CanIf Rx9 → COM Rx5 | Callback đúng ID5 và bytes |
| PDUR-06 | Confirmation Tx7 → COM Tx3 | Đúng source/result, không qua Rx route |
| PDUR-07 | Caller sửa payload sau call | PduR không dùng pointer muộn |
| PDUR-08 | Same numeric ID thuộc Tx/Rx namespace | Route theo direction, không nhầm |
| PDUR-09 | Global0x0010 | Trace có identity, wire không thêm hai byte |
| PDUR-10 | COM real + PduR real, fake CanIf | Retry/drop chỉ COM thực hiện |

## 7. Điều kiện hoàn thành

Tất cả test pass, real route không biết phần cứng/signal, lower failure được truyền lên đúng, reverse path không mơ hồ, interfaces đồng bộ với COM/CanIf và validation report có bằng chứng. PduR unit pass chưa chứng minh mapping toàn hệ thống hoặc CAN bus đã chạy.
