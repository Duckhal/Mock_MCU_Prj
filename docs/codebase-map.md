# Codebase Map

Khảo sát 2026-09-14. Core memory hành vi và ưu tiên: [context.md](context.md). Trình tự khởi tạo CAN: [can-init-sequence.md](can-init-sequence.md). Scope: source nội bộ, cấu hình build, yêu cầu và hai sơ đồ; không audit toàn bộ vendor headers hoặc chứng minh hoạt động phần cứng.

## Snapshot

- Mục tiêu: mock ba ECU S32K144 giao tiếp CAN, hai đầu UART nối PC; ba firmware do ba người triển khai độc lập.
- Hiện tại: một workspace C bare-metal S32 Design Studio; CAN Driver mới và board loopback harness đã có source, các upper layer vẫn là skeleton/tài liệu.
- Entrypoint: [`src/main.c`](../src/main.c); reset/vector/runtime tại `Project_Settings/Startup_Code/`.
- Build: [`.project`](../.project), [`.cproject`](../.cproject), NXP GCC 6.3 for Arm được cấu hình; không khẳng định toolchain sẵn trong PATH.
- API dự kiến: [`requirements/API_SPEC.md`](../requirements/API_SPEC.md), DRAFT 0.2. Cấu trúc CAN cục bộ được gom dưới `drivers/can`; nhiều API upper layer vẫn chưa được triển khai.

## Module map

| Module | Trách nhiệm / điểm sửa chính | Phụ thuộc và consumer |
|---|---|---|
| Entrypoint | [`src/main.c`](../src/main.c): chạy CAN internal-loopback một lần và giữ kết quả cho debugger | `drivers/can/test/Can_LoopbackTest` |
| Startup/linker | `Project_Settings/Startup_Code/`, `Project_Settings/Linker_Files/`: reset, vectors, memory layout | Vendor definitions trong `include/`; dùng cho mọi firmware build |
| CAN Driver | [`Can.c`](../drivers/can/driver/Can.c), [`Can.h`](../drivers/can/driver/Can.h), [`Can_Types.h`](../drivers/can/driver/Can_Types.h): FlexCAN0 lifecycle, polling, Tx/Rx/bus-off callbacks, stats | S32K144; không include upper-layer header; consumer hiện tại là loopback test |
| CAN helpers/config | [`Can_Internal.c`](../drivers/can/driver/Can_Internal.c), [`Can_Cfg.c`](../drivers/can/config/can/Can_Cfg.c): validation, pack/unpack, normal/loopback HOH→MB | MB0 Tx, MB1 Rx exact `0x123`, 8 MHz/500 kbit/s |
| CAN BSP | [`board_can.c`](../bsp/can/board_can.c): watchdog, SOSC, PCC, PTE4/PTE5, transceiver GPIO | Được gọi trước `Can_Init`; nằm ngoài `drivers/can` theo ranh giới BSP |
| CAN board harness | [`Can_LoopbackTest.c`](../drivers/can/test/Can_LoopbackTest.c): frame `0x123`, payload `DE AD BE EF`, bounded poll, enum kết quả + LED xanh | Đã ARM-compile; cần flash để có kết quả board thật |
| CAN host tests | [`test_can_driver.c`](../tests/host/can/test_can_driver.c), [fake register](../tests/host/can/fakes/S32K144.h): validation/lifecycle/Tx/Rx/busy/bus-off | GCC host; pass gần nhất 2026-09-13 |
| CAN legacy reference | `can_task/driver`, `can_task/upper`, `can_task/test`: bài cũ để đối chiếu | Không còn trong source set Debug_FLASH; không sửa để ghép với driver mới |
| CAN upper/config skeleton | `drivers/can/comm/`, `drivers/can/config/{canif,cantp,com,pdur}` | Chưa có stack hoạt động; ưu tiên mới là COM Part 1 với fake PduR trước |
| COM Part 1 plan | [com-part1-plan.md](implement/com-part1-plan.md): phân tích assignment/notes, 17 file code/test dự kiến, 7 giai đoạn và test matrix | PLAN; chưa thay đổi COM implementation. [Assignment](../requirements/assignment_part1_com_signal.md) là nguồn yêu cầu chính |
| Bộ plan Part 1 | [implement/README.md](implement/README.md): COM, PduR, CanIf, CAN Driver, BSP/scheduler, integration | File inventory/ownership và contracts giữa các plan; chưa có source mới từ các plan |
| UART | [`Driver_UART.c`](../drivers/uart/Driver_UART.c), [header](../drivers/uart/Driver_UART.h): LPUART1 baud/8N1, IRQ byte callbacks, stats, blocking debug TX | NVIC/S32K144; chưa có gateway consumer hoạt động trong main |
| Byte queue | [`ring_buffer.c`](../middlewares/ring_buffer.c), [header](../middlewares/ring_buffer.h): static byte FIFO, Push/Pop/full/empty | Standard integer/bool types; dự kiến UART gateway, không phải CAN frame queue |
| Timebase/IRQ | `drivers/systick/`, `drivers/lpit/`, `drivers/nvic/`: ticks, timer callbacks, interrupt enable/priority | S32K144/system clock; phục vụ driver và timeout tương lai |
| GPIO/LED BSP | [`Driver_GPIO.c`](../drivers/gpio/Driver_GPIO.c), [`LED.c`](../bsp/LED.c), [`board.h`](../bsp/board.h): pins/events/LED | NVIC/common driver types; harness/debug consumer |
| Peripheral bổ sung | `drivers/adc/`, `drivers/rtc/`: ADC conversion/calibration, RTC seconds/alarm callbacks | ADC include RTC/SysTick/NVIC; chưa gắn nghiệp vụ mock trong main |
| Requirements | [`README.md`](../requirements/README.md), API spec và PNG | Thiết kế mục tiêu; không phải module đang chạy |
| Generated build | `Debug_FLASH/`: makefiles, objects, ELF/map | S32DS sinh; không sửa generated makefile để thay source/config |

## Interaction map

**Luồng board loopback đang được entrypoint gọi:**

```text
main -> Board CAN init -> Can_Init(loopback) -> register callbacks -> STARTED
     -> Can_Write(HTH0, ID 0x123, DE AD BE EF) -> FlexCAN MB0
poll -> Tx confirmation(swPduHandle) + Rx indication(HRH1, ID, DLC, bytes)
     -> g_CanLoopbackTestResult; LED xanh khi dữ liệu khớp
```

- Driver mới chỉ dùng polling và dispatch callback trong main context; callback registration giữ driver độc lập với CanIf.
- RX driver trả controller + HRH + CAN ID; CanIf tương lai phải map thêm CAN ID/length/owner bằng software.
- UART ISR gọi registered RX callback/next-TX-byte callback; false từ TX callback tắt TX-empty interrupt. Main/gateway tương lai phải enqueue rồi kick TX IRQ.
- SysTick ISR tăng tick và gọi callback; `GetTicks()` chỉ là ms khi tickHz=1000. LPIT callback có CH0 và chain CH2/CH3.

**Luồng mục tiêu, DRAFT:**

```text
Control: COM/test consumer <-> PduR direct <-> CanIf <-> CAN Driver
Bulk:    UART Gateway <-> Transfer <-> PduR SDU <-> CanTp <-> CanIf
```

CanIf chọn PduR hoặc CanTp theo upper owner; N-PDU không quay lại PduR direct. CAN ID/DLC/payload/timing dùng chung giữa ECU; PDU/HTH/HRH/MB local được khác nhau. Không cần ECU2 relay trên shared bus.

## Change guide

- Thay CAN timing/filter/MB: đọc `drivers/can/config/can/Can_Cfg.*` và `drivers/can/driver/Can.c`; profile hiện chỉ nhận 8 MHz/500 kbit/s. Xác minh board clock/pins trước test vật lý.
- Thay thứ tự khởi tạo/lifecycle CAN: cập nhật đồng thời `docs/can-init-sequence.md`, driver/config và loopback harness để tài liệu không lệch code.
- Thêm CanIf: đặt implementation dưới `drivers/can/comm/canif`, config dưới `drivers/can/config/canif`, rồi đăng ký ba callback với CAN Driver.
- Viết COM: theo `docs/implement/com-part1-plan.md`; giữ code trong `drivers/can/comm/com` và config trong `drivers/can/config/com`. Host tests ở `tests/host/com` với fake PduR. Plan thay header Tx tick có nowMs bằng invocation 1 ms, bỏ triggered/Rx deadline khỏi Part 1; source hiện vẫn là skeleton cũ.
- Các tầng còn lại: theo mục lục `docs/implement/README.md`; dùng chung `comm_types.h`/`pdur_com.h` và module config, không duplicate definitions theo mỗi plan. Integration plan có validator system matrix và trace 19 deliverables của assignment.
- Thêm route: freeze schema CanIf/PduR và reverse confirmation; skeleton config hiện ở `drivers/can/config`, chưa có implementation route hoạt động.
- Thay UART binary I/O: giữ driver byte callbacks, xây gateway parser/queue riêng; chú ý loss/error và TX kick. Không tìm CLI Task 5 vì không có `tasks/` trong bản này.
- Thêm timeout: inject clock ở module logic; xác định tick→ms và rollover. Không đưa state machine vào SysTick ISR.
- Thay build source set: sửa cấu hình S32DS/`.cproject`, regenerate output; kiểm tra config đang chọn. Debug_FLASH có nhiều source roots hơn Release_FLASH/Debug_RAM/Release_RAM.
- Cập nhật kiến trúc/API: sửa requirements/spec và cả hai core memory; khi ở ASK chỉ trả lời, không write-back.

## Validation guide

- Tài liệu: `git diff --check`, `git diff --stat`, kiểm local Markdown links và đối chiếu tên/signature với header/source.
- Build firmware: import project vào S32DS, chọn **Debug_FLASH**, Build Project. Generated `Debug_FLASH/makefile` có target `all`; chỉ dùng `make -C Debug_FLASH all` trong môi trường đã nạp đúng S32DS/GCC tools. Chưa thực thi lệnh build trong khảo sát này.
- Host test hiện chạy bằng GCC trực tiếp và đã pass; test fake thanh ghi không thay bằng chứng phần cứng.
- Board harness hiện có một `main` duy nhất ở `src/main.c`; xem `g_CanLoopbackTestResult`, trong đó `CAN_LOOPBACK_TEST_PASSED` mới là thành công. Chưa có raw board log sau flash.
- API_SPEC mục 19 có LL-01..LL-13 làm test design: invalid config, bounded wait, PDU shared HTH, buffer lifetime, filtering, stale completion, rollover, overflow và interoperability.
- Board tests phải ghi firmware/config/node/wiring/bitrate/input/expected/actual; loopback không thay physical three-node test, LED không chứng minh file byte-correct.

## Cross-cutting concerns và unknowns

- RAM: linker flash mô tả target 64 KB SRAM nhưng vùng data được cấp trong file là `0x8000 + 0x7000`; cần đọc map thực tế trước cấp object buffer, không xem toàn SRAM là RAM còn rảnh.
- Static storage: ring N byte dùng N−1; Clear/Init cần cô lập ISR. Tính an toàn SPSC cần test/compiler review, không suy chỉ từ volatile.
- Clock/pins: setup SOSC/PTE4/PTE5/transceiver nằm trong `bsp/can/board_can.c`; SOSC wait của BSP hiện chưa có timeout. UART driver tự cấu hình FIRC/PTC6/PTC7. Wiring board khác chưa được xác nhận.
- Observability: CAN mới có status và counters; chưa có monotonic Tx deadline/timeout theo target API_SPEC và chưa có structured log sink.
- Persistence/auth: chưa thấy file/object persistence hoặc authentication layer; wire CRC/integrity chưa chốt, không đồng nghĩa authentication.
- Integration: chưa có firmware của hai cộng tác viên, message matrix, golden TP/UART vectors hoặc performance measurements.
- Git hiện đã được người dùng tạo; `.gitignore` bỏ build/log files. Board log tương lai cần chọn nơi lưu có chủ đích để không thất lạc bằng chứng do ignore.
