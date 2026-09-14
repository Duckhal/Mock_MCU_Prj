# Plan cấu hình và tích hợp toàn stack — Part 1

Trạng thái: **PLAN**. Mục tiêu là nối các module đã có test độc lập thành đường Tx/Rx thật, hoàn thiện mapping và evidence để đánh giá toàn bộ Part 1. Xem [mục lục và thứ tự thực hiện](README.md).

Nguồn: [assignment](../../requirements/assignment_part1_com_signal.md) §3.5,17–39; [architecture notes](../../requirements/part1_architecture_notes.md). COM unit pass không đủ để đánh dấu hoàn thành 19 deliverables của bài.

## 1. Preconditions và các gap hiện tại

- COM/PduR/CanIf mới ở skeleton hoặc plan, không có real route đang chạy.
- CAN Driver có CAN0 polling/test; cần theo driver plan để xác nhận lifecycle và model nhiều controller.
- BSP đã init clock/pin nhưng chưa trả lỗi có bound đầy đủ; main chưa có scheduler1 ms.
- Shared return codes/signatures phải nhất quán giữa các plan trước khi link.
- CAN config hiện filter0x123; ví dụ bài dùng0x321. Đây là khác biệt config cần chốt, không tự gọi cả hai là cùng wire protocol.
- Ba ECU do ba người viết: project này không thể sửa firmware còn lại hoặc chứng minh chúng tương thích khi chưa có traces/config của họ.

## 2. Quyết định cấu hình hệ thống

Một logical message được mô tả một lần trong system matrix, sau đó khai báo sender/receivers. GlobalPduId unique theo message; không unique theo từng bản local config ở mọi node.

Ví dụ **đề xuất dùng cho fixture**:

| Message | Global | CAN ID | Publisher | Receiver | DLC | Slots | Timing |
|---|---:|---:|---|---|---:|---|---|
| VehicleStatus | 0x0010 | 0x321 | ECU1 | ECU2, ECU3 | 8 | Speed16, Gear8, Alive8, padding4 byte | period10, offset1, retries3 |

Không coi publisher/CAN ID/profile này đã được cả nhóm chấp nhận. Little-endian, unsigned values và Rx U=0 policy lấy từ COM plan cũng phải chốt ở matrix trước chạy physical interoperability. App thật chưa rõ, test signal giả đủ kiểm layer.

Ví dụ local mappings cố tình khác nhau:

```text
ECU1: COM Tx3 → PduR route11 → CanIf Tx7 → HTH0 → CAN0
       Global0x0010, CAN ID0x321

ECU2: CAN0 → HRH1 + CAN ID0x321 → CanIf Rx9 → PduR route12 → COM Rx5
       Global0x0010
```

ECU3 có thể có local IDs khác nữa. Tx/Rx endpoint của một message cùng global identity; Direct Binding không thêm Global ID vào tám byte payload. Không dùng HTH/HRH làm namespace chung giữa các ECU.

## 3. File inventory và ownership

**14 file code/config/test/build trong scope tích hợp: 7 có sẵn cần sửa, 7 file mới.** Nhiều config là shared với plan module; count này là edit surface của integration, không cộng thẳng thành tổng source mới toàn project.

| # | File | Loại | Chức năng |
|---:|---|---|---|
| 1 | `drivers/can/config/node_cfg.h` | Sửa rỗng | Select node/profile và const export |
| 2 | `drivers/can/config/node_cfg.c` | Mới | Profile binding các config module đã chọn cho ECU/test |
| 3 | `drivers/can/config/com/com_cfg.c` | Sửa/shared | Signal/group/PDU per-node từ matrix |
| 4 | `drivers/can/config/pdur/pdur_cfg.c` | Sửa/shared | Tx/Rx/reverse routes đúng local IDs |
| 5 | `drivers/can/config/canif/canif_cfg.c` | Sửa/shared | CAN ID/HTH/HRH/length/direct binding |
| 6 | `drivers/can/config/can/Can_Cfg.c` | Sửa/shared | Controller/HOH/filter/loopback mode |
| 7 | `drivers/can/test/Com_LoopbackTest.h` | Mới | Init/run/status API cho COM stack harness; tên dùng chung COM plan |
| 8 | `drivers/can/test/Com_LoopbackTest.c` | Mới | Test scenario, observations/results; không giấu route bằng gọi trực tiếp Can_Write |
| 9 | `src/main.c` | Sửa | Chọn một harness, init order, scheduler super-loop |
| 10 | `.cproject` | Sửa | Source/include roots và exclusions theo build config |
| 11 | `tests/host/comm_stack/test_stack.c` | Mới | Real COM/PduR/CanIf, fake CAN; end-to-end data/identity/error assertions |
| 12 | `tests/host/comm_stack/run_tests.ps1` | Mới | Link đúng real/fake set, chạy trace scenario/logs |
| 13 | `requirements/part1_message_matrix.json` | Mới | Source of truth shared wire messages + per-node logical references, version |
| 14 | `tests/config/validate_part1_matrix.py` | Mới | Standard-library validator: uniqueness, references, slot/timing/binding; exit nonzero khi sai |

Test stack có thể tái sử dụng fake CAN phần CanIf test nhưng tách fake PduR/COM bằng compile guard trong fake endpoint file; không link duplicate symbols. Nếu fake cũ quá gộp, refactor có test rồi cập nhật inventory thực tế.

Tài liệu khi thực thi: `docs/implement/stack-part1-validation.md`, `docs/implement/part1-model-views.md`, `docs/implement/part1-traces.md`; cập nhật API_SPEC/core/init guide. Những file này chưa chứa report PASS trước khi chạy.

## 4. Giai đoạn 1 — Chốt message matrix và validator

1. Bảng message định nghĩa globalId/name/CAN ID/DLC/endian/slots, publisher/consumers, period/offset/retry, version. Bảng node định nghĩa local signal/group/PDU/route/L-PDU/HOH/controller references.
2. Validate mỗi Signal một Group, Group một PDU; slot overlap/bounds/range; global unique theo logical message.
3. Validate Direct Binding một global message→một CAN ID trong profile; sender ownership không trùng trên cùng bus; receiver đúng message; route endpoints/length khớp; HOH exists/correct type/controller; Rx key HRH+CAN ID unique.
4. Validate period/offset/retry giới hạn implementation; đủ pin/clock profile để dùng controller đã chọn.
5. Ban đầu C config viết tay từ matrix, chưa cần generator. Validator JSON **không chứng minh C config đã khớp**: thêm host configuration-conformance assertions trên exported const C objects và các trường matrix/golden fixtures; report có review trace mapping. Nếu sau này generate C từ matrix, thay workflow thành regeneration/diff check.

**Exit:** matrix pass, fixture cố tình sai fail, từng ECU review wire profile; không sửa đề gốc để làm mất dấu khác biệt.

## 5. Giai đoạn 2 — Ghép stack trên host

Compile real COM/codec/config + real PduR/config + real CanIf/adapter/config + fake CAN. Không link fake COM/PduR/CanIf trong target này.

Scenario Tx: Send speed100/gear3/alive5 → tick due → Can_Write fake thấy CAN ID0x321/HTH0/swHandle7, bytes `C9 00 07 0B 00 00 00 00` theo little-endian fixture. BUSY/NOT_OK khiến COM retry đúng tick kế; mọi tầng dưới chỉ một call. Acceptance clear U ở COM; pending CanIf còn đến completion.

Scenario Rx: fake CAN emit HRH1/ID0x321 → adapter → CanIf Rx9 → PduR → COM Rx5 → ReceiveSignal đúng values. Chạy Tx-only/Rx-only COM fixture ở process khác nhau của cùng message; không sửa model bằng hai logical PDU trùng Global ID trong một config.

**Exit:** trace toàn path đúng local/global IDs, byte payload, attempts và callback count; error route/ID/DLC/lifetime tests pass.

## 6. Giai đoạn 3 — Init và scheduler thật

Thứ tự target theo các plan:

```text
BoardCan_DisableWatchdog → BoardCan_Init
    → verify core clock → SysTick_Init(1000)
    → Can_Init (tất cả controller STOPPED)
    → CanIf_Init (register driver callbacks)
    → PduR_Init
    → Com_Init
    → CanIf_SetControllerMode(controller, STARTED) cho các controller dùng
    → Can_StackScheduler_Init(currentTick)
    → super-loop: Scheduler_Poll(currentTick)
```

Mỗi API trả lỗi phải chặn bước phụ thuộc. Nếu start một controller thành công nhưng controller khác lỗi, stop controller đã start có kiểm soát và giữ test FAILED; không chạy COM một phần. Scheduler chỉ bắt đầu sau start toàn bộ.

Migrate caller BSP cũ trong Can_LoopbackTest theo BSP plan cùng patch, để đổi header không làm hỏng test cũ. Full build không được link can_task (trùng Can_*), fake modules, host main hoặc hai board mains.

Debug_FLASH ưu tiên vì source roots hiện đủ hơn. Release_FLASH/Debug_RAM/Release_RAM phải hoặc cập nhật dependencies rồi kiểm riêng, hoặc ghi rõ chưa hỗ trợ profile mới; không suy chúng build được từ Debug_FLASH pass.

**Exit:** clean regenerate S32DS build, một main/một SysTick_Handler, no unresolved/duplicate symbols; host và firmware source sets minh bạch.

## 7. Giai đoạn 4 — CAN loopback và COM board test

1. Chạy regression harness CAN Driver cũ với profile loopback; lưu ID/DLC/payload/status.
2. COM stack harness dùng real route. Để giữ model một direction/PDU đơn giản: trên một board có thể chạy hai lần boot/profile — Tx COM đi hết stack tới CAN loopback capture, sau đó Rx COM nhận cùng golden frame qua đường CanIf/PduR. Không gọi đây là full bidirectional COM loopback trong một config nếu Rx chỉ là probe.
3. Full Tx COM→Rx COM acceptance chứng minh bằng hai node có Tx-only/Rx-only config, hoặc thiết kế explicit endpoint model bổ sung có review/test riêng. Không tự thêm GlobalPduId phụ làm message đổi identity chỉ để test loopback.
4. Đo tick1 ms và cadence của requests, phân biệt request time với physical bus timestamp vì arbitration/BUSY có thể trễ.
5. Report tất cả case NOT RUN nếu chưa có board access; không suy pass từ LED hoặc compile.

**Exit:** CAN loopback và COM portions có raw evidence; mức độ cover thực tế được ghi riêng, không gộp thành claim quá rộng.

## 8. Giai đoạn 5 — Physical multi-ECU và deliverables

- Một publisher, các receiver của cùng GlobalPduId dùng wire profile đã thống nhất; common bitrate/standard ID, transceiver/wiring/termination theo board setup đã kiểm.
- Test lower busy có kiểm soát (fake cho exact sequence, physical traces bổ sung), latest value, duplicate/no-update semantics; record các khác biệt local ID.
- Chứng minh HRH+CAN ID lookup nhiều message trên BasicCAN; multi-controller host model và board evidence đúng controller nào thực chạy.
- Trace không in blocking lên UART nếu làm miss1 ms; dùng RAM records/counters và export sau test, hoặc logger được đo timing. Log capacity full phải ghi overflow.
- Tất cả retry/drop/tick tests dùng expected độc lập; không sửa golden vector để khớp bytes sai.

**Exit:** bảng evidence từng ECU, config version, timing, bytes và counters; report đầy đủ deliverables dưới đây.

## 9. Truy vết 19 deliverables assignment §38

| Deliverable | Nguồn model/code | Evidence cần nộp |
|---|---|---|
| 1 Signal model | COM config/types | Config + range tests |
| 2 Signal Slot | COM codec | Golden bytes/U/boundary tests |
| 3 Signal Group | COM config | Ownership graph validation |
| 4 I-PDU + Global ID | COM + matrix | Unique logical messages/endpoint mapping |
| 5 Direct CAN Binding | Matrix/CanIf | Global↔L-PDU↔CAN ID trace |
| 6 Period/offset/retries | COM config | Tick timeline |
| 7 Runtime retry model | COM | DYN tests, drop events |
| 8 PduR routes | PduR | Forward/reverse route tests |
| 9 CanIf Tx L-PDU | CanIf | TxPdu→ID/HTH tests |
| 10 CanIf Rx L-PDU | CanIf | HRH+ID→RxPdu tests |
| 11 Hardware Object | Driver | HOH/MB/controller config |
| 12 Multiple Controller | Driver/BSP | Host isolation + board result cho profile thật |
| 13 Ownership View | model-views document | COM/PduR/CanIf/CanDrv boundaries |
| 14 Building Block View | model-views document | Dependencies/interfaces |
| 15 Tx Dynamic Behavior | COM trace | BUSY/nonblocking/budget/coalescing |
| 16 Rx Runtime View | stack trace | CAN→HRH→L-PDU→route→signal |
| 17 Retry/drop | COM validation | DYN-04..11 exact tick records |
| 18 End-to-end Global trace | matrix + traces document | Local IDs khác nhưng global/bytes đúng |
| 19 Validation report | per-module + stack reports | Commands/toolchain, expected/actual, PASS/FAIL/NOT RUN |

## 10. Điều kiện hoàn thành

Mỗi module có unit evidence, system matrix và C config khớp, host stack pass, firmware build đúng source set, board timing/data được kiểm, và mọi deliverable có link artifact. Giữ rõ ba mức: COM core complete, stack host complete, physical Part 1 complete. Không tự công bố giao tiếp cả ba ECU nếu chỉ có một board test.
