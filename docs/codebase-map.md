# Codebase Map

Snapshot: 2026-09-18, sau khi tách loopback test khỏi main. Quy tắc tương tác và mục tiêu: [context.md](context.md). Map này phản ánh source và kiểm chứng host/ARM; không chứng minh firmware chạy được trên board.

## Tổng quan

CanDrv validation (2026-09-15): Can.c đã validate toàn bộ controller/HOH trước register access: shared unique Tx/Rx IDs, đúng một controller reference, valid object types và unique physical resources. Lookup theo ID độc lập index; Rx trả configured HRH. Hardware vẫn CAN0/500 kbit/s/Tx MB8/Rx MB9; profile khác bị từ chối. Config production HOH0/1 → controller0 được giữ nguyên.

Project C bare-metal S32 Design Studio/Eclipse cho S32K144. Mục tiêu là mock ba ECU giao tiếp CAN với hai đầu UART nối PC. Stack CAN Part 1 hiện đã triển khai lại cho profile VehicleStatus/CAN0.

CanDrv, CanIf, PduR và COM đã nối Tx/Rx cho profile VehicleStatus. PduR có route và callbacks thật cả hai chiều. COM có Signal packing, Update Bit, periodic Tx scheduler và Rx decode. `src/can_loopback_test.c` chứa 27 case cũ cộng COM full-stack test; `src/main.c` chọn mode và giữ TC-003 hai board. Firmware FLASH đã compile/link, chưa chạy board.

## Cây thư mục còn lại

~~~text
app/                         node_app.c/h, gateway_app.c/h — đều rỗng
bsp/
  can/                       board_can.c/h — watchdog, clock, pin, transceiver
  LED.c/h, board.h            LED và thông tin board
can_task/                    bài CAN tham khảo riêng
  driver/inc/, driver/src/   Can, types và config
  upper/inc/, upper/src/     CanUpper, types và config
  test/main.c                harness bị bọc trong #if 0
drivers/
  can/common/                CanStack_Types.h, CanStack_Cfg.h/c
  can/can_driver/            Can.c/h, Can_Types.h, Can_cfg.c, Can_Cfg.h
  can/canif/                 CanIf.c/h, CanIf_Types.h, CanIf_Cfg.c/h
  can/pdur/                  PduR.c/h, PduR_Types.h, PduR_Cfg.c/h
  can/com/                   Com.c/h và types/config; có Tx/Rx runtime
  adc/, common/, gpio/, lpit/, nvic/, rtc/, systick/, uart/
middlewares/                 ring_buffer.c/h
src/                         main.c — chọn mode và TC-003; can_loopback_test.c/h — test cũ
requirements/                assignment, architecture notes, README, overview
  assumptions/               API_SPEC, flow.xml, Tx-Rx Flow PNG
docs/                        context.md, codebase-map.md, can-init-sequence.md
Project_Settings/            startup, linker, debugger
include/                     vendor/device headers
Debug_FLASH/                 generated build output có thể cũ
~~~

docs/implement không tồn tại. Tests CAN0 mới nằm tại tests/can_driver; build/can_driver chứa executable/object và verification.log (generated, ignored). CAN driver/comm/config/test trước reset không còn trong drivers/can.

## Module map

| Module | File/ranh giới còn tồn tại | Phụ thuộc và trạng thái |
|---|---|---|
| Entrypoint | [src/main.c](../src/main.c) | `BOARD_MODE=0` gọi `CanLoopbackTest_Run()`; `1` phát TC-003 từ SW2 qua CanIf; `2` nhận qua PduR snapshot và điều khiển RGB LED. Mode 1/2 log bằng UART LPUART1. |
| Loopback board test | [can_loopback_test.c](../src/can_loopback_test.c), [header](../src/can_loopback_test.h) | 27 CanDrv/CanIf/PduR cases + COM test, giữ debugger `g_CanLoopbackTestResult`. Main chỉ dispatch mode 0. |
| TC-003 host tests | [test_tx.c](../tests/board_demo/test_tx.c), [test_rx.c](../tests/board_demo/test_rx.c), [README](../tests/board_demo/README.md) | Debounce, payload CA/command/sequence, Rx validation/duplicate, LED mapping và UART log pass; ARM FLASH build ba mode pass. |
| PduR Tx/Rx | [PduR.c](../drivers/can/pdur/PduR.c), [PduR_Cfg.c](../drivers/can/pdur/PduR_Cfg.c) | PduR_ComTransmit route COM→CanIf; CanIf callbacks route confirmation và Rx về COM. Config VehicleStatus global0x0010; debug counters và Rx byte snapshot phục vụ main. |
| COM Part 1 | [Com.c](../drivers/can/com/Com.c), [Com.h](../drivers/can/com/Com.h), [Com_Cfg.c](../drivers/can/com/Com_Cfg.c) | Validate hierarchy/slot/timing; pack Signal + Update Bit, scheduler 1 ms với bounded retry/latest value, receive decode. Tx và Rx I-PDU có group riêng, cùng logical GlobalPduId. |
| COM host tests | [test_com.c](../tests/com/test_com.c), [test_com_config.c](../tests/com/test_com_config.c) | Deterministic pack/Rx/offset/retry/drop tests và 12 malformed config fixtures đều pass. |
| Startup/linker | Project_Settings/Startup_Code, Project_Settings/Linker_Files | Reset/vector/memory layout và vendor headers trong include; chưa audit toàn bộ ở lượt này. |
| CAN common mới | [CanStack_Types.h](../drivers/can/common/CanStack_Types.h), [CanStack_Cfg.h](../drivers/can/common/CanStack_Cfg.h), [CanStack_Cfg.c](../drivers/can/common/CanStack_Cfg.c) | Common PDU types đã có; header config định nghĩa ba GlobalPduId vehicle/engine/climate 0x0010..0x0012. CanStack_Cfg.c còn rỗng; chưa có binding qua các tầng. |
| CAN driver mới | [Can.c](../drivers/can/can_driver/Can.c), [Can.h](../drivers/can/can_driver/Can.h), [Can_Types.h](../drivers/can/can_driver/Can_Types.h), [Can_cfg.c](../drivers/can/can_driver/Can_cfg.c), [Can_Cfg.h](../drivers/can/can_driver/Can_Cfg.h) | Bốn API, hardware CAN0/8 MHz oscillator/500 kbit/s, standard Classical data, Tx MB8/Rx MB9. Validate toàn bộ config và resolve HOH→object→controller theo ID. Production HTH0/HRH1; logical IDs có thể sparse. Snapshot Tx, saved swPduHandle, bounded waits và debugger logs; callbacks nối sang CanIf. |
| CanIf Part 1 | [CanIf.c](../drivers/can/canif/CanIf.c), [CanIf.h](../drivers/can/canif/CanIf.h), [types](../drivers/can/canif/CanIf_Types.h), [config](../drivers/can/canif/CanIf_Cfg.c) | Init validate static tables/HOH refs; Tx local ID → CAN ID+HTH; Rx HRH+CAN ID → local Rx ID; callbacks tới PduR extern. Không queue/retry hoặc sửa payload/U. Config VehicleStatus Tx/Rx ID 0, CAN ID 0x321, HTH0/HRH1; có debugger logs. |
| CanIf tests | [README](../tests/canif/README.md), [runner](../tests/canif/run_tests.ps1) | 7 nhóm fixtures unit tests và production-config smoke test đạt; ARM production object compile đạt. Full firmware với PduR/COM đã link; chưa chứng minh board runtime. |
| CAN0 host tests | [test_can_driver.c](../tests/can_driver/test_can_driver.c), [run_tests.ps1](../tests/can_driver/run_tests.ps1), [README](../tests/can_driver/README.md) | Real vendor types/masks + fake MMIO W1C/handshakes và capture callbacks; 11 nhóm fixtures (14 malformed configs/sparse IDs/9 regressions) và 9 regressions với config production đạt. Runner compile production driver ARM Cortex-M4; full stack FLASH ELF link đạt. Chưa board runtime. |
| CAN BSP | [board_can.c](../bsp/can/board_can.c), [board_can.h](../bsp/can/board_can.h) | Main gọi disable_WDOG/init_MCU trước driver init; sử dụng S32K144.h và LED BSP. BSP waits cũ unbounded. |
| CAN reference driver | [Can.c](../can_task/driver/src/Can.c), [Can.h](../can_task/driver/inc/Can.h), [Can_Cfg.c](../can_task/driver/src/Can_Cfg.c) | FlexCAN0; config normal/loopback, MB0 Tx và MB1 Rx exact 0x123. Driver include/callback trực tiếp CanUpper; không phải driver mới. |
| Reference upper/test | [CanUpper.c](../can_task/upper/src/CanUpper.c), [test/main.c](../can_task/test/main.c) | CanUpper gọi driver, giữ PDU data/status; harness #if 0. Chưa nối với firmware main. |
| App | app/node_app.c/h, app/gateway_app.c/h | File rỗng, chưa có application logic hoặc gateway. |
| UART | [Driver_UART.c](../drivers/uart/Driver_UART.c), [Driver_UART.h](../drivers/uart/Driver_UART.h) | LPUART1, RX/TX byte callbacks, IRQ và blocking TX; chưa có file/session protocol tích hợp. |
| Byte queue | [ring_buffer.c](../middlewares/ring_buffer.c), [ring_buffer.h](../middlewares/ring_buffer.h) | Byte FIFO dùng storage caller cung cấp; không phải CAN frame queue. |
| Timebase/IRQ | drivers/systick, drivers/lpit, drivers/nvic | Tick/timer/interrupt utilities còn lại; chưa có scheduler CAN/COM mới. |
| GPIO/LED | [Driver_GPIO.c](../drivers/gpio/Driver_GPIO.c), [LED.c](../bsp/LED.c), [board.h](../bsp/board.h) | Pin/LED utilities còn lại; init_MCU khởi tạo LED xanh. |
| Ngoại vi khác | drivers/adc, drivers/rtc, drivers/common | Source còn tồn tại; không coi là logic mock communication đã tích hợp. |
| Yêu cầu | [README.md](../requirements/README.md), [assignment](../requirements/assignment_part1_com_signal.md), [architecture notes](../requirements/part1_architecture_notes.md) | README mô tả mục tiêu tổng thể; assignment là spec chính Part 1, notes là giải thích. |
| Tài liệu cũ | [API_SPEC.md](../requirements/assumptions/API_SPEC.md), [can-init-sequence.md](can-init-sequence.md) | API draft/guide có nội dung về source đã xóa; inventory/lifecycle cần kiểm tra lại trước dùng. |
| Build metadata/output | [.project](../.project), [.cproject](../.cproject), Debug_FLASH | Metadata S32DS còn lại; generated output không chứng minh source hiện tại build được. |

## Luồng thực tế và luồng mục tiêu

Entrypoint mode loopback:

~~~text
src/main.c
  -> BSP watchdog/oscillator/pins/LED
  -> Can_Init -> Freeze LPB=1, SRXDIS=0 -> CanIf_Init -> Com_Init
  -> PduR invalid requests (4)
  -> Can_Write / CanIf_Transmit / PduR_ComTransmit, DLC 0..8
  -> Com_SendSignal/Com_MainFunctionTx -> PduR/CanIf/CanDrv -> real PduR callbacks -> Com_ReceiveSignal
  -> PASS/FAIL debugger result -> vòng lặp vô hạn
~~~

Mode vật lý `BOARD_MODE=1/2`: `SW2 -> CanIf_Transmit(0x321,DLC3) -> CAN0 bus -> CanIf Rx -> PduR snapshot -> RGB LED`, cùng LPUART1 log ở hai board. COM không tham gia đường 3-byte TC-003; mode 0 vẫn kiểm thử COM 8-byte.

Mode 0 gọi `src/can_loopback_test.c`; hai mode vật lý vẫn hoàn toàn trong `src/main.c`.

Reference can_task độc lập:

~~~text
CanUpper -> Can_Init / Can_Write / CAN MainFunction
Can driver -> CanUpper_TxConfirmation / CanUpper_RxIndication
~~~

Reference có cả polling functions và IRQ handler. Can_Init của reference thoát freeze sau cấu hình; các hardware waits chưa có bound. Không suy lifecycle, callbacks hoặc multi-controller compliance của stack mới từ reference này.

Luồng **đã nối cho VehicleStatus Part 1**:

~~~text
Signal -> Group -> COM I-PDU -> PduR route -> CanIf Tx L-PDU
       -> HTH -> CanDrv hardware object -> controller

controller -> CanDrv -> HRH + CAN ID -> CanIf Rx L-PDU
           -> PduR route -> COM I-PDU -> signal consumer
~~~

COM sở hữu packing/timing; PduR sở hữu routing; CanIf sở hữu CAN ID mapping; CanDrv sở hữu HOH/controller. VehicleStatus đã có runtime Tx/Rx qua CAN0. GlobalPduId là identity qua config, không serialize payload; các GlobalPduId khác trong common vẫn chưa có binding runtime.

## Build và entrypoints

- Debug_FLASH source entries: Project_Settings (loại Linker_Files và Debugger), bsp, drivers, include, middlewares, src.
- Release_FLASH, Debug_RAM, Release_RAM: Project_Settings với cùng exclusions, include, src. Chưa đồng nhất source set với Debug_FLASH.
- can_task và app không nằm trong source entries của cả bốn cấu hình.
- Entrypoint firmware là src/main.c với ba mode biên dịch; mode 0 link thêm src/can_loopback_test.c. can_task/test/main.c chỉ là bài tham khảo disabled bằng #if 0.
- Main không còn tham chiếu header loopback đã xóa. Generated Debug_FLASH makefiles vẫn trỏ tới các CAN folders cũ; regenerate trong IDE khi build Debug_FLASH.
- Khi được yêu cầu tích hợp/build: sửa entrypoint/source entries trong project metadata và regenerate bằng S32DS. Không dùng generated makefiles/ELF cũ làm nguồn sự thật.

## Điểm sửa cho công việc tiếp theo

- Can.c hiện dùng direct volatile register access, không còn MMIO wrappers/Can_ModifyRegister. Các giai đoạn init là static helpers có English comments và shared bounded wait/logging. Host test runner tạo Can_instrumented.c bằng tests/can_driver/instrument_driver.py trong build directory để mô phỏng MMIO; ARM compiler kiểm tra original production source. Refactor giữ profile/runtime và đạt lại 9 nhóm tests. File can_task/driver/src/Can.c vẫn là reference riêng.

- Thiết kế và profile hiện tại: [can-driver-design.md](can-driver-design.md). Driver thực hiện Tx snapshot/completion/Rx contracts; CanIf production callbacks đã tồn tại. Bước tích hợp tiếp theo là PduR callbacks/routes và COM. Multi-controller/config-driven driver vẫn là hướng mở rộng.

- Types/config/API/logic CAN mới: dưới drivers/can/can_driver, canif và common. CanIf validate PDU references; CanDrv validate toàn bộ controller/HOH và resolve ID nhưng phần register vẫn CAN0. Multi-controller/bit timing động cần implementation và tests riêng; CAN1 macro reference cũ trong runner đã được loại bỏ.
- Clock/pin/transceiver: BSP [board_can.c](../bsp/can/board_can.c). Hiện setup SOSC ngoài 8 MHz, PORTC/PORTE/FlexCAN0 gate, PTE4 RX/PTE5 TX ALT5 và GPIO PTC14/PTE11 cho transceiver. Watchdog/SOSC waits chưa có timeout.
- Reference register/MB handling: can_task/driver và guide [CAN_Homework_S32K144EVB.md](../can_task/CAN_Homework_S32K144EVB.md). Tách coupling CanUpper và đối chiếu assignment khi tái sử dụng.
- Model ownership, slot, retry, Direct Binding: đọc assignment/notes trước; không tiếp tục các plan đã xóa hoặc tự dùng lại local IDs của profile cũ.
- Timing profile mới cần giữ tip: MainFunction COM 1 ms, period bội 10 ms, offsets khác nhau trong 1–9 ms. Offset thuộc COM I-PDU; retry next tick có thể trùng nominal slot khác.
- UART/gateway/app/bulk: giữ ở phạm vi về sau; chưa có consumer hoặc protocol hiện hành để tiếp tục.
- Trong ASK chỉ đọc/giải thích, không sửa core memory hay chạy tool ghi file.

## Validation và giới hạn bằng chứng

- Runner tests/can_driver đạt 11 nhóm fixtures và 9 nhóm production config; tests/canif đạt 7 nhóm và production smoke. COM host tests đạt pack/Rx/retry/drop và 12 malformed config fixtures. Host full-stack simulation pass; strict ARM FLASH ELF đã link với tất cả module, không còn undefined symbol.
- Host fake registers không thay bằng chứng trên board; internal loopback không thay test ba ECU. Chưa flash hoặc đo timing 1 ms thực.
- Chưa có firmware cộng tác viên hoặc message matrix chính thức; chưa xác nhận CAN ID/bitrate/DLC/endian/scale/timing tương thích liên ECU.
- [can-init-sequence.md](can-init-sequence.md) và API_SPEC chưa được cập nhật trong lượt này; xem như guide/draft cần review, không phải mô tả implementation hiện tại.

## Board loopback harness hiện tại

Main.c có comment banners Can Test/CanIf Test/PduR Test/COM Test cho init, route, Tx/Rx và shared checks.

Host simulation dùng modules thật/MMIO transformer pass 27 case DLC0..8, 4 PduR rejections và COM scheduler/Signal Rx test. COM host tests pass pack/Rx/retry/drop cùng 12 malformed config fixtures. ARM FLASH ELF `build/com/Com_full_loopback.elf` compile/link strict không còn undefined symbols; logs `build/com/build.log`, `build/com/main_host.log`. Debugger PASS=2/FAIL=3; COM speed cuối 120 với U1, gear3/alive5 với U0, Com_TxDropCount=0. Chưa flash/test board; internal loopback không kiểm tra dây/transceiver. Main gọi Com_MainFunctionTx liên tiếp như tick logic; muốn kiểm định timing thật cần timer 1 ms và đo trên board. Reset để chạy lại.

TC-003 mới: ARM FLASH build strict cả `BOARD_MODE=0/1/2` ra `build/board_demo/{0,1,2}/board_mode_{0,1,2}.elf`; host tests Tx/Rx pass. Hai mode vật lý dùng CAN ID0x321, DLC3, SW2/PTC12, RGB LED, UART 115200 PTC7 TX. Chưa thử bus vật lý hoặc terminal trên board.

Tài liệu đối chiếu Part 1: [checklist COM Signal](../requirements/part1_com_signal_checklist.md) có 78 mục với trường bằng chứng riêng; dùng để audit mô hình, hành vi, deliverables và acceptance criteria. Đây là danh sách chưa đánh giá, không phải chứng nhận source hiện tại đã đạt.
