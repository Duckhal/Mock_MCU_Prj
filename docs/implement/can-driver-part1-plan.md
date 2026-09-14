# Plan hoàn thiện CAN Driver theo Part 1

Trạng thái: **PLAN trên driver đã có source**, không phải yêu cầu viết lại toàn bộ. Căn cứ: [assignment](../../requirements/assignment_part1_com_signal.md) §19–27,33,36–39, [notes](../../requirements/part1_architecture_notes.md) §19,23–29; đối chiếu [Can.c](../../drivers/can/driver/Can.c), [types](../../drivers/can/driver/Can_Types.h) và [init guide](../can-init-sequence.md).

## 1. Phần tái sử dụng và khoảng trống

| Có sẵn | Cần hoàn thiện/kiểm chứng |
|---|---|
| Init/STOPPED/STARTED/FAULT và bounded poll | Tách initialized khỏi mode FAULT sau partial init; không cho recovery sử dụng MB chưa cấu hình đầy đủ |
| Can_Write, copy bytes, swPduHandle, busy tracking | Regression tests accepted/rejected, stale completion và stop timeout |
| MainFunction_Write/Read/Error | Review Rx CODE/overrun/unlock/flag sequence với reference manual trước sửa hardware logic |
| Tx/Rx/bus-off callbacks trong main | Can_RxPduType cho adapter CanIf; giữ một nguồn phát event |
| Pure config/pack helpers | Mở rộng config từ một controller sang controller list, HOH unique toàn driver |
| Config MB0/MB1, normal/loopback 0x123 | Profile Part 1 mapping 0x321 hoặc matrix chính thức; hỗ trợ filter BasicCAN nếu test yêu cầu |
| Fake-register host tests và board harness | Fake hiện chỉ một CAN0, W1C/hardware transitions cần mô phỏng đúng; board result chưa có log trong context |

Host tests đã báo pass ở lượt trước chỉ là bằng chứng phạm vi tests khi đó. Không dùng nó để khẳng định mọi recovery/multi-controller/hardware path đều đúng.

## 2. Phạm vi và quyết định

- Giữ API Tx `Can_Write(HTH,pdu)`, không thêm ControllerId vì HTH→controller.
- HTH/HRH cùng namespace, unique trong một driver instance; MB index chỉ unique **trong controller**, hai controller được dùng cùng MB0.
- Controller config và hardware register ownership thuộc driver; BSP cung cấp clock/pins/transceiver.
- Baseline polling, không vừa ISR vừa main đọc/clear cùng flag.
- Giữ default CAN0/500 kbit/s chạy regression trước; thêm CAN1 như profile có điều kiện board clock/pin đã được xác minh. Không suy ví dụ CAN1 trong đề là board hiện đã wiring CAN1.
- Đề yêu cầu mô hình multi-controller; hoàn thành CAN0-only không được ghi là đã đáp ứng mục này.
- CAN deadline theo monotonic clock trong API_SPEC cũ là extension ngoài trọng tâm COM Part 1; bounded hardware wait và không accept traffic sau lỗi vẫn cần. Nếu chưa có Tx deadline, ghi giới hạn request thiếu ACK có thể giữ hardware resource; bounded COM retry không giải phóng request đã accepted.

## 3. Danh sách file

**12 file: 11 file có sẵn dự kiến sửa/kiểm chứng và 1 file mới.** Không sửa `can_task/` hoặc generated `Debug_FLASH` makefiles.

| # | File | Nhiệm vụ |
|---:|---|---|
| 1 | `drivers/can/driver/Can_Types.h` | Controller config/list, HOH namespace, Can_RxPduType, explicit init state và diagnostics cần bổ sung |
| 2 | `drivers/can/driver/Can.h` | Contract multi-controller/lifecycle, typed callbacks, comments |
| 3 | `drivers/can/driver/Can.c` | Resolve controller base/PCC, per-controller runtime, pending/terminal events, polling |
| 4 | `drivers/can/driver/Can_Internal.h` | Pure validation/helpers contracts |
| 5 | `drivers/can/driver/Can_Internal.c` | Validate controller list/HOH/MB, pack/unpack reuse |
| 6 | `drivers/can/config/can/Can_Cfg.h` | Export CAN0-only regression và multi-controller profiles |
| 7 | `drivers/can/config/can/Can_Cfg.c` | Bit timing, controller list, global HOH IDs, per-controller MB/filter |
| 8 | `tests/host/can/fakes/S32K144.h` | Fake CAN0/CAN1/PCC, các masks dùng bởi driver |
| 9 | `tests/host/can/test_can_driver.c` | Regression và mới multi-controller/fault/stale/Rx cases |
| 10 | `tests/host/can/run_tests.ps1` | **Mới**: case runner/process isolation/logs, không dựa global exe cũ |
| 11 | `drivers/can/test/Can_LoopbackTest.h` | Kết quả/test profile interface |
| 12 | `drivers/can/test/Can_LoopbackTest.c` | Migration config API và board evidence cho controller được hỗ trợ |

Tài liệu ngoài count: cập nhật `docs/can-init-sequence.md`, `requirements/assumptions/API_SPEC.md`, core memory; tạo `docs/implement/can-driver-part1-validation.md` khi chạy tests. BSP CAN1 work thuộc [BSP plan](bsp-scheduler-part1-plan.md).

## 4. Model/API dự kiến

Root Can_ConfigType có `{controllerList, controllerCount, hohList, hohCount}`. Controller entry có controllerId, bitrate/profile, loopbackEnable, hardwareTimeoutCount. HOH giữ controllerId/type/mbIndex/rxId/mask.

Private controller runtime có configured/initialized, mode, busOff, lastError và Tx MB state riêng. Callback table có thể dùng chung driver nhưng registration chỉ khi mọi configured controller STOPPED và không có terminal event. Không để register callback lần thứ hai xóa pending của controller khác.

Hardware base lookup là bảng private controllerId→register base/PCC index được xác minh với header/manual S32K144. Không truy cập register từ public helper hoặc CanIf. Can_GetControllerStatus(id) đọc đúng controller; Can_GetStats hiện có thể giữ aggregate để tương thích, nếu thêm per-controller API phải cập nhật callers/tests cùng giai đoạn.

Root init validate toàn bộ config trước hardware writes. Nếu controller thứ hai init lỗi sau controller thứ nhất: return failure, cô lập các controller đã chạm hardware, không publish root initialized/start. Reset/retry cần policy rõ; không dùng STOP/START như bằng chứng init đã hoàn tất.

Training Rx representation thêm vào Can_Types.h: `{CanId, Length, const DataPtr}`. Giữ callback driver cũ và adapter CanIf theo CanIf plan là phương án đầu tiên; adapter giữ training API public mà không đổi mọi consumer CAN trong cùng bước.

## 5. Giai đoạn thực hiện

### Giai đoạn 1 — Capture baseline và review hardware contract

Chạy tests CAN hiện có, lưu log; compile ARM; nếu có board, ghi loopback result hiện tại trước refactor. Đối chiếu manual/errata đúng part/revision về clock selection, Freeze, MB CODE, Tx abort, IFLAG W1C, Rx lock/unlock và bus-off.

Source hiện truy cập CAN0 cố định; stop timeout có nhánh return trước finalize; init timeout giữ s_config trong FAULT. Đây là đường cần tái hiện/test, chưa mặc định coi đã được bảo đảm bởi host test cũ.

**Exit:** evidence baseline + danh sách invariant, không sửa register dựa suy đoán. Nếu bug tái hiện, theo log-first debugging trước minimal fix.

### Giai đoạn 2 — Tách config/runtime theo controller

Viết controller-list types, validation và lookup; migrate CAN0 config/harness. Dùng HOH global IDs0/1 cho CAN0 và2/3 cho CAN1 trong fixture; MB0 Tx/MB1 Rx có thể lặp ở mỗi controller.

Validate duplicate controller/HOH, invalid reference, duplicate MB cùng controller, invalid role/filter/timing; cho phép MB index giống nhau khác controller. Runtime static có limit controller count rõ, không cấp phát động.

**Exit:** CAN0 regression pass không đổi wire bytes; fake hai controller chứng minh HTH2 ghi CAN1 và không đụng CAN0.

### Giai đoạn 3 — Init và start/stop trên từng controller

Refactor helper Freeze/Exit nhận context đã resolve. Init thành công vẫn STOPPED; start chỉ khi initialized thật và callbacks đủ. Từng hardware wait hữu hạn, timeout latch fault cụ thể; không publish STARTED khi NOTRDY còn set.

Root partial init failure không cho controller đã init một phần nhận traffic. Ghi trạng thái fault/init riêng thay vì suy từ s_config != NULL.

**Exit:** timeout ở controller0/1 và partial-init tests pass; status đọc đúng controller; controller khác không bị reset state khi stop một controller.

### Giai đoạn 4 — Tx ownership và terminal events

Can_Write copy payload trước OK, reserve per-MB swHandle; BUSY không giữ request mới. MainFunction_Write poll configured controllers, phát một terminal event/accepted request.

Stop/bus-off giữ event đến khi main dispatch, chặn reuse resource trước khi chắc chắn hardware event cũ không còn hiệu lực. Nếu không thể abort/freeze, giữ FAULT và state theo dõi; không giả giải phóng mailbox rồi accept request mới. Chốt terminal timeout policy bằng test và evidence hardware.

**Exit:** success/stop/cancel/busoff/timeout không callback trùng, late completion không kết thúc request mới. Callback không re-enter mutate driver.

### Giai đoạn 5 — Rx và adapter

Mở rộng polling đọc controller từ HRH owner; kiểm standard/DLC/profile, MB CODE/overrun theo manual đã xác minh. Unlock/clear theo thứ tự hardware yêu cầu; drop/overrun có counters, không báo truncated data như frame hợp lệ.

Can_RxPduType giúp adapter phục vụ CanIf training signature. Payload callback chỉ sống trong lời gọi; một nơi duy nhất dispatch, không gọi đồng thời adapter và CanUpper.

**Exit:** HRH→controller→Rx fields đúng trên fake CAN0/CAN1; cùng HRH nhận nhiều ID và CanIf filter phần còn lại; malformed/overrun paths có tests.

### Giai đoạn 6 — Board regressions và Part 1 mapping

ARM full build khi dependencies có sẵn. Chạy lại CAN0 internal loopback; CAN1 chỉ chạy sau BSP verified. Profile filter chuyển sang CAN ID được matrix chọn, không sửa COM logic.

Board evidence ghi controller, source clock/bitrate, config/firmware, Tx/Rx bytes, DLC, error status/counters và result. Physical multi-node chạy theo integration plan, internal loopback không chứng minh transceiver/wiring.

**Exit:** host cases pass, ARM compile/link phù hợp target, board case có raw evidence hoặc ghi NOT RUN.

## 6. Ma trận tests

| ID | Điều kiện cần chứng minh |
|---|---|
| CANDRV-01 | Regression CAN0 init/write/read/busy/error và byte packing |
| CANDRV-02 | HOH duplicate trên hai controller bị reject |
| CANDRV-03 | Cùng MB index khác controller hợp lệ; cùng controller bị reject |
| CANDRV-04 | HTH2→CAN1, HRH3→CAN1; không đụng CAN0 |
| CANDRV-05 | Partial init lỗi → không start từ config chưa initialized |
| CANDRV-06 | Missing clock/Freeze timeout/NOTRDY timeout có bounded exit |
| CANDRV-07 | Payload bị caller sửa sau Write OK không đổi hardware data |
| CANDRV-08 | Write BUSY không overwrite handle/data pending |
| CANDRV-09 | Stop/bus-off/timeout giữ terminal event đúng một lần |
| CANDRV-10 | Event cũ sau stop/recovery không gắn request mới |
| CANDRV-11 | W1C và Tx CODE transition được fake mô phỏng, không gán register kiểu RAM rồi suy phần cứng |
| CANDRV-12 | Rx invalid DLC/IDE/RTR/overrun đúng policy đã kiểm manual |
| CANDRV-13 | Stop controller0 không xóa pending controller1 |
| CANDRV-14 | Internal loopback board và physical mapping có log riêng |

## 7. Hoàn thành và giới hạn

Part 1 driver sẵn sàng khi hardware ownership đúng, model multi-controller validated, CAN0 regression giữ được, lifecycle/fault paths có tests và CanIf adapter nối được. Các hardware claims về CAN1, bus-off recovery hoặc overrun chỉ được xác nhận theo manual + board evidence phù hợp; không biến plan thành báo cáo PASS.
