# Kế hoạch triển khai communication stack — Part 1

Trạng thái: **PLAN đang được thực thi theo từng giai đoạn**. Bộ plan được lập từ [assignment của người giao bài](../../requirements/assignment_part1_com_signal.md), [architecture notes](../../requirements/part1_architecture_notes.md) và tip timing bổ sung do người giao bài cung cấp ngày 2026-09-14. Types/config có thể hoàn thành trước trong khi logic và validation report vẫn còn ở trạng thái kế hoạch.

## 1. Danh sách plan

| Thứ tự đọc/viết | Plan | Nội dung | Edit surface dự kiến |
|---:|---|---|---:|
| 1 | [COM](com-part1-plan.md) | Signal/Group/Slot, periodic1 ms, bounded retry/drop, Rx, fake PduR | 17 file: 6 sửa + 11 mới |
| 2 | [PduR](pdur-part1-plan.md) | Direct route Tx/Rx và reverse confirmation, payload-transparent | 11 file: 3 sửa + 8 mới |
| 3 | [CanIf](canif-part1-plan.md) | Logical CAN mapping, HRH+ID lookup, no queue, callback adapter | 12 file: 3 sửa + 9 mới |
| 4 | [CAN Driver](can-driver-part1-plan.md) | Tái sử dụng driver, multi-controller/lifecycle/event/Rx regressions | 12 file: 11 có sẵn + 1 mới |
| 5 | [BSP/timebase/scheduler](bsp-scheduler-part1-plan.md) | Bounded board init, SysTick, main-context cadence1 ms | 11 file: 4 có sẵn + 7 mới |
| 6 | [Cấu hình/tích hợp toàn stack](stack-integration-part1-plan.md) | Matrix/validator, full paths, firmware/board evidence, 19 deliverables | 17 file: 7 có sẵn + 10 mới |

Các số là phạm vi từng plan tại snapshot khảo sát, không phải tổng file khác nhau: common header, pdur_com.h và config C được tham chiếu ở nhiều plan. Mỗi file shared chỉ có một định nghĩa, một phiên bản; plan tích hợp không tạo thêm bản sao.

Tiến độ 2026-09-15: type/config foundation cho COM, PduR, CanIf, system matrix và node root đã có source; host conformance test đã chạy. Runtime logic của COM/PduR/CanIf, scheduler và board integration vẫn chưa hoàn thành.

## 2. Thứ tự viết code khác thứ tự init

Viết **COM trước** theo mong muốn người dùng, test với fake PduR; sau đó thay dần fake bằng PduR thật, CanIf thật và CAN Driver. Không phải đợi hardware stack hoàn chỉnh mới viết codec/scheduler COM.

```text
COM + fake PduR
    → COM + PduR + fake CanIf
    → COM + PduR + CanIf + fake CAN
    → CAN/BSP regressions + timebase
    → real stack trên board
```

Thứ tự init khi tích hợp: BSP/timebase → CAN STOPPED → CanIf/register callbacks → PduR → COM → start controller → bắt đầu tick scheduler. Init fail phải dừng bước phụ thuộc.

## 3. Contract chung giữa các plan

1. Assignment là yêu cầu chính Part 1; notes giải thích thiết kế. API_SPEC/skeleton cũ phải đồng bộ khi code, đặc biệt triggered Tx/deadline monitor không thuộc Part 1.
2. COM chỉ gọi PduR. `Com_SendSignal` không làm pending; `Com_MainFunctionTx(void)` được gọi mỗi1 ms, xử lý I-PDU theo config order.
3. Lower accepted là `COMM_OK`; mọi non-OK là failure cho bounded retry COM. PduR/CanIf không tự retry hoặc queue payload.
4. CanDrv copy payload trước OK; COM clear U ngay khi PduR trả OK. TxConfirmation không clear U lần nữa.
5. GlobalPduId chung logical message, local module/ECU IDs có thể khác; Direct Binding không serialize Global ID.
6. Little-endian, unsigned types và Rx U=0 giữ value là lựa chọn local **đề xuất**, cần thống nhất wire profile trước physical tests.
7. Runtime/const ownership rõ, caller payload chỉ borrowed trong call; callbacks không mutate reentrant module; COM hoạt động main context.
8. Module state và physical controller state độc lập. Bounded retry COM không thay nghĩa vụ terminal event của request đã được driver accepted.
9. Comment mỗi hàm, input validation, static bounds, deterministic tests và logs thật trước khi ghi PASS. Debug hardware theo reproduce/log/manual rồi mới sửa.
10. Giữ `bsp/can` ngoài `drivers/can`; firmware không link host fakes, legacy can_task hoặc hai entrypoints.
11. Profile timing của project dùng cửa sổ phase 10 ms: Tx I-PDU có period là bội của 10 ms và offset riêng trong 1..9 ms. Đây là rule cấu hình/validator để phân tán **nominal due time**; COM core vẫn dùng tick 1 ms theo assignment, còn retry có thể sử dụng tick kế tiếp và trùng với nominal slot khác.

## 4. Tài liệu ngoài Part 1

CanTp, Transfer/file/image, UART gateway/framing và app nghiệp vụ **chưa có plan implementation trong bộ này** vì assignment loại bulk communication khỏi Part 1. Khi có yêu cầu Part 2 cần plan riêng về wire protocol, segmentation/flow control, storage và integrity, không suy từ skeleton/API_SPEC cũ.

GPIO/LED/UART/timebase drivers hiện có là dependencies; chỉ thay phần phục vụ Part 1 có lý do cụ thể. Không mở task viết lại các peripheral không liên quan.

## 5. Cách sử dụng plan

Thực hiện từng giai đoạn trong từng file, bắt đầu COM giai đoạn1. Cuối mỗi giai đoạn ghi files thực thay đổi, command/test IDs, kết quả và limitations vào report tương ứng; cập nhật core memory. Khi đang ASK chỉ giải thích, không sửa report/code. Khi người dùng yêu cầu implementation ở AGENT mới thực hiện các giai đoạn đã lập.
