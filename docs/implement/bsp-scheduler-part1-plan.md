# Plan BSP CAN, timebase và scheduler 1 ms — Part 1

Trạng thái: **PLAN**. Nguồn hành vi tick: [assignment](../../requirements/assignment_part1_com_signal.md) §12–14,27; [notes](../../requirements/part1_architecture_notes.md) §10,20. Source hiện có: [board_can.c](../../bsp/can/board_can.c), [SysTick](../../drivers/systick/Driver_SysTick.c), [main](../../src/main.c).

## 1. Mục tiêu và hiện trạng

BSP chuẩn bị clock/pins/transceiver; timebase cung cấp tick; scheduler gọi CAN polling trước COM theo cadence 1 ms. Ba trách nhiệm này không được đưa vào COM.

`board_can.c` hiện có disable_WDOG và init_MCU trả void, vòng chờ unlock watchdog/SOSCVLD chưa có poll bound. Driver_SysTick_Init đã validate tick_hz/reload 24-bit và dùng SystemCoreClock, nhưng caller phải xác minh SystemCoreClock đúng clock thực. Main hiện chạy một CAN_LoopbackTest_Run rồi đứng, chưa có periodic scheduler.

Không tự đổi pin/wiring theo ví dụ CAN1 trong đề. Clock/pin/transceiver của board thứ hai hoặc controller1 phải kiểm sơ đồ board/reference manual trước implementation.

## 2. Ranh giới và contract

- BSP không gọi COM/PduR/CanIf hoặc đổi CAN routing.
- SysTick ISR chỉ tăng tick, không gọi COM/CanIf/CAN polling, không in UART.
- Scheduler là test/system orchestration trong `drivers/can/test`, không phải COM API mới hay RTOS.
- COM tick0 lấy mốc khi tất cả module đã init và controller STARTED; invocation đầu sau 1 ms.
- Can_MainFunction_Write trước Com_MainFunctionTx giải phóng completion resource trước retry; Can_MainFunction_Error/Read cũng chạy main context.
- Không gọi nhiều lần COM liên tiếp để bù missed ticks. Với counter-based COM, một lần gọi là một logical millisecond; task trễ phải được ghi thành lỗi cadence, không tuyên bố schedule wall-clock vẫn đúng.

Policy local đề xuất khi delta tick>1: latch scheduler overrun, dừng phát COM mới, vẫn service CAN events để drain accepted requests; yêu cầu restart test có kiểm soát. Không tự Com_Init lần hai hoặc phát burst catch-up. Phương án nâng cao scheduler skip-ahead theo timestamp nằm ngoài counter model Part 1.

## 3. Danh sách file

**11 file: 4 có sẵn dự kiến sửa và 7 mới.** BSP giữ ngoài `drivers/can`; scheduler chỉ phục vụ harness Part 1.

| # | File | Loại | Nhiệm vụ |
|---:|---|---|---|
| 1 | `bsp/can/board_can.h` | Sửa | Status/config type và init API trả lỗi |
| 2 | `bsp/can/board_can.c` | Sửa | Bounded watchdog/oscillator waits, clock/pins/transceiver setup |
| 3 | `drivers/systick/Driver_SysTick.h` | Sửa khi cần | Document clock/tick/lifetime/init contract |
| 4 | `drivers/systick/Driver_SysTick.c` | Sửa khi có evidence | Clock/reload/init ordering và regression, không đổi driver vô cớ |
| 5 | `drivers/can/test/Can_StackScheduler.h` | Mới | Init/poll/status API cho harness |
| 6 | `drivers/can/test/Can_StackScheduler.c` | Mới | Đọc tick, cadence, gọi main functions, overrun state |
| 7 | `drivers/can/config/stack_scheduler_cfg.h` | Mới | Period 1 ms và limits local, không chứa message routing |
| 8 | `tests/host/stack_scheduler/fake_platform.h` | Mới | Fake ticks và capture call order |
| 9 | `tests/host/stack_scheduler/fake_platform.c` | Mới | Stub time source/CAN/COM mainfunctions |
| 10 | `tests/host/stack_scheduler/test_scheduler.c` | Mới | Cadence, order, rollover, overrun tests |
| 11 | `tests/host/stack_scheduler/run_tests.ps1` | Mới | Compile/run/logs |

Source startup/vector và board schematic là đầu vào review, không mặc định chỉnh sửa. `src/main.c` và migration callers board init do integration plan sở hữu. Report tương lai `docs/implement/bsp-scheduler-part1-validation.md`.

## 4. API/model đề xuất

BoardCan_StatusType có OK/INVALID_PARAM/WATCHDOG_TIMEOUT/CLOCK_TIMEOUT; BoardCan_ConfigType có controller profile, poll limit và pin/clock selection được hỗ trợ. Không expose arbitrary register writes cho app.

Đề xuất API `BoardCan_DisableWatchdog(pollLimit)` và `BoardCan_Init(config)` trả status. Migrate caller cũ cùng giai đoạn tích hợp; không giữ wrapper void nuốt lỗi. Do chưa có code mới, tên API là target, không ghi như đã tồn tại.

Scheduler API `Can_StackScheduler_Init(nowTicks)`, `Can_StackScheduler_Poll(nowTicks)`, `Can_StackScheduler_GetStatus(out)`. Fake truyền tick để tests deterministic; main đọc Driver_SysTick_GetTicks rồi truyền vào. State gồm lastTick, started/overrun, executedTicks, missedTicks. Unsigned delta xử lý rollover; giả định không cách hai lần poll quá cả chu kỳ uint32.

Nếu clock không chia hết 1000, cần xác định khả năng tạo tick đúng profile; không chỉ integer-divide rồi nói đó là chính xác 1 ms. Board validation đo thực tế tick/clock, giới hạn timing lấy từ profile hệ thống chốt sau.

## 5. Các giai đoạn

### Giai đoạn 1 — Review clock/pins và baseline

Đọc board schematic/manual đúng board; kiểm pins hiện tại PTE4/PTE5, transceiver GPIO, SOSC8 MHz và core clock source. Chạy baseline/ghi lỗi vào log trước fix. Xác định bootstrap watchdog khi nào được phép disable trong startup.

**Exit:** bảng clock/pin/controller có evidence, không suy core clock bằng clock CAN.

### Giai đoạn 2 — Bounded BSP init

Viết return-status APIs; mọi wait có poll limit hợp lệ; failure dừng chuỗi init và trả stage lỗi. Không in blocking log trong wait. Không chạm pin/controller unsupported.

Caller chỉ chuyển Can_Init khi BoardCan_Init thành công. Vì đổi public API, tìm và migrate Can_LoopbackTest và stack harness cùng integration patch; không để hai đường BSP init khác logic.

**Exit:** valid board setup compile; timeout stage quan sát được qua status. Register-specific validation cần đúng hardware docs và debug fault injection có kiểm soát, không chỉ host assert mirror code.

### Giai đoạn 3 — Timebase 1 ms

Xác minh SystemCoreClockUpdate sau clock setup nếu cần; init SysTick1000 Hz, check return. Xem vector SysTick_Handler đúng driver, không duplicate handler từ test module. Callback NULL nếu không cần ISR work thêm.

**Exit:** tick tăng đúng tần số theo phép đo board; host/compile chỉ chứng minh logic, chưa thay phép đo.

### Giai đoạn 4 — Scheduler thuần logic

Init lastTick khi controller start xong. Poll cùng tick không gọi COM. Delta1 gọi Error→Write→Read→COM Tx đúng một lần; cập nhật counters. Delta>1 latch overrun như policy mục2 và không burst retry.

Không phụ thuộc volatile để bảo vệ nhiều tác vụ đồng thời: APIs dùng main context; ISR chỉ sửa tick word. Public status getter validate pointer. Record error/call order/tick để test đọc.

**Exit:** deterministic tests period/order/rollover/overrun pass; không sleep trong host test.

### Giai đoạn 5 — Ghép với stack harness và đo

Main khởi tạo BSP/timebase/CAN/CanIf/PduR/COM theo integration plan; run scheduler poll trong super-loop. Inject value update trước COM tick theo fixture; LED/report ở main ngoài ISR.

Đo worst-case loop duration gồm CAN polling và COM xử lý tất cả PDU; normal operation phải hoàn tất trong tick budget. Error path bounded dài có thể gây overrun: report là lỗi cadence, không bỏ qua để claim DYN-01.

**Exit:** có raw tick/order/overrun counters; chứng minh không có back-to-back catch-up attempts trong cùng tick.

## 6. Test matrix

| ID | Input | Expected |
|---|---|---|
| SCHED-01 | Init tick100, poll100 lặp | Không COM call |
| SCHED-02 | Poll101 | Error/Write/Read/COM theo thứ tự một lần |
| SCHED-03 | Poll102,103 | Mỗi tick một lần, không phát theo tốc độ CPU |
| SCHED-04 | Tick UINT32_MAX→0 | Delta1, một invocation bình thường |
| SCHED-05 | Tick100→103 | Latch overrun, không gọi COM ba lần |
| SCHED-06 | Overrun khi CAN request đã accepted | Vẫn drain driver events, không tạo COM request mới |
| SCHED-07 | Null status/poll trước Init | Return/counter xác định, không gọi stack |
| BSP-01 | Invalid profile/poll limit | Fail trước ghi sai hardware |
| BSP-02 | Watchdog/SOSC wait không đạt | Return lỗi hữu hạn, chưa chạy Can_Init |
| BSP-03 | Board thực sau init | Clock/pins/tick đo đúng profile |

## 7. Điều kiện hoàn thành

BSP trả lỗi rõ và có bound; timebase được xác minh với core clock thật; scheduler giữ order/cadence và nhận biết overrun; callbacks không chạy COM trong ISR. Chưa có phép đo board thì chỉ đánh dấu HOST/COMPILE PASS, BOARD NOT RUN.
