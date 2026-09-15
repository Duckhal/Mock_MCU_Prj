# Codebase Map

Snapshot: 2026-09-15, sau triển khai driver CAN0 và CanIf. Quy tắc tương tác và mục tiêu: [context.md](context.md). Map này phản ánh source và kiểm chứng host/ARM object; không chứng minh firmware chạy được trên board.

## Tổng quan

CanDrv validation (2026-09-15): Can.c đã validate toàn bộ controller/HOH trước register access: shared unique Tx/Rx IDs, đúng một controller reference, valid object types và unique physical resources. Lookup theo ID độc lập index; Rx trả configured HRH. Hardware vẫn CAN0/500 kbit/s/Tx MB8/Rx MB9; profile khác bị từ chối. Config production HOH0/1 → controller0 được giữ nguyên.

Project C bare-metal S32 Design Studio/Eclipse cho S32K144. Mục tiêu là mock ba ECU giao tiếp CAN với hai đầu UART nối PC. Người dùng đã xóa stack CAN mới và docs/implement để triển khai lại.

can_driver và CanIf đã triển khai bốn API mỗi module với comments tiếng Anh. CanStack_Types.h có common PDU types và Std_ReturnType/E_OK/E_NOT_OK. Entrypoint hiện chứa board loopback harness và hai PduR capture callbacks; PduR production chưa triển khai. Firmware FLASH harness đã compile/link từ source hiện tại, nhưng chưa chạy trên board.

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
  adc/, common/, gpio/, lpit/, nvic/, rtc/, systick/, uart/
middlewares/                 ring_buffer.c/h
src/                         main.c — board loopback CanDrv+CanIf và PduR capture callbacks
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
| Entrypoint | [src/main.c](../src/main.c) | Board harness: BSP/Can_Init/Freeze LPB+clear SRXDIS/CanIf_Init; Can_Write và CanIf_Transmit DLC 0..8 (18 case), BUSY/copy/exactly-once callbacks. PduR captures và volatile g_CanLoopbackTestResult nằm trong main; green LED báo PASS. |
| Startup/linker | Project_Settings/Startup_Code, Project_Settings/Linker_Files | Reset/vector/memory layout và vendor headers trong include; chưa audit toàn bộ ở lượt này. |
| CAN common mới | [CanStack_Types.h](../drivers/can/common/CanStack_Types.h), [CanStack_Cfg.h](../drivers/can/common/CanStack_Cfg.h), [CanStack_Cfg.c](../drivers/can/common/CanStack_Cfg.c) | Common PDU types đã có; header config định nghĩa ba GlobalPduId vehicle/engine/climate 0x0010..0x0012. CanStack_Cfg.c còn rỗng; chưa có binding qua các tầng. |
| CAN driver mới | [Can.c](../drivers/can/can_driver/Can.c), [Can.h](../drivers/can/can_driver/Can.h), [Can_Types.h](../drivers/can/can_driver/Can_Types.h), [Can_cfg.c](../drivers/can/can_driver/Can_cfg.c), [Can_Cfg.h](../drivers/can/can_driver/Can_Cfg.h) | Bốn API, hardware CAN0/8 MHz oscillator/500 kbit/s, standard Classical data, Tx MB8/Rx MB9. Validate toàn bộ config và resolve HOH→object→controller theo ID. Production HTH0/HRH1; logical IDs có thể sparse. Snapshot Tx, saved swPduHandle, bounded waits và debugger logs; callbacks nối sang CanIf. |
| CanIf Part 1 | [CanIf.c](../drivers/can/canif/CanIf.c), [CanIf.h](../drivers/can/canif/CanIf.h), [types](../drivers/can/canif/CanIf_Types.h), [config](../drivers/can/canif/CanIf_Cfg.c) | Init validate static tables/HOH refs; Tx local ID → CAN ID+HTH; Rx HRH+CAN ID → local Rx ID; callbacks tới PduR extern. Không queue/retry hoặc sửa payload/U. Config VehicleStatus Tx/Rx ID 0, CAN ID 0x321, HTH0/HRH1; có debugger logs. |
| CanIf tests | [README](../tests/canif/README.md), [runner](../tests/canif/run_tests.ps1) | 7 nhóm fixtures unit tests và production-config smoke test đạt; ARM production object compile và relocatable CanDrv+CanIf link đạt. Chỉ còn PduR callbacks undefined; không chứng minh board runtime. |
| CAN0 host tests | [test_can_driver.c](../tests/can_driver/test_can_driver.c), [run_tests.ps1](../tests/can_driver/run_tests.ps1), [README](../tests/can_driver/README.md) | Real vendor types/masks + fake MMIO W1C/handshakes và capture callbacks; 11 nhóm fixtures (14 malformed configs/sparse IDs/9 regressions) và 9 regressions với config production đạt. Runner compile production driver ARM Cortex-M4; relocatable CanDrv+CanIf link đạt, còn PduR callbacks. Chưa full firmware/board test. |
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

Entrypoint còn trên đĩa:

~~~text
src/main.c
  -> BSP watchdog/oscillator/pins/LED
  -> Can_Init -> Freeze LPB=1, SRXDIS=0 -> CanIf_Init
  -> Can_Write / CanIf_Transmit, DLC 0..8
  -> real CanDrv/CanIf -> PduR capture callbacks trong main.c
  -> PASS/FAIL debugger result -> vòng lặp vô hạn
~~~

Reference can_task độc lập:

~~~text
CanUpper -> Can_Init / Can_Write / CAN MainFunction
Can driver -> CanUpper_TxConfirmation / CanUpper_RxIndication
~~~

Reference có cả polling functions và IRQ handler. Can_Init của reference thoát freeze sau cấu hình; các hardware waits chưa có bound. Không suy lifecycle, callbacks hoặc multi-controller compliance của stack mới từ reference này.

Luồng **phải xây lại theo Part 1**:

~~~text
Signal -> Group -> COM I-PDU -> PduR route -> CanIf Tx L-PDU
       -> HTH -> CanDrv hardware object -> controller

controller -> CanDrv -> HRH + CAN ID -> CanIf Rx L-PDU
           -> PduR route -> COM I-PDU -> signal consumer
~~~

COM sở hữu packing/timing; PduR sở hữu routing; CanIf sở hữu CAN ID mapping; CanDrv sở hữu HOH/controller. CanDrv và CanIf đã có runtime mới với profile CAN0/VehicleStatus; PduR/COM chưa được triển khai. GlobalPduId là identity hệ thống qua config và implicit trên dây trong Direct Binding; common constants chưa phải binding matrix đầy đủ.

## Build và entrypoints

- Debug_FLASH source entries: Project_Settings (loại Linker_Files và Debugger), bsp, drivers, include, middlewares, src.
- Release_FLASH, Debug_RAM, Release_RAM: Project_Settings với cùng exclusions, include, src. Chưa đồng nhất source set với Debug_FLASH.
- can_task và app không nằm trong source entries của cả bốn cấu hình.
- Entrypoint firmware là src/main.c. can_task/test/main.c không phải main đang build và hiện bị disabled bằng #if 0.
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

- Lượt cập nhật này đối chiếu file inventory, dung lượng placeholder, main/BSP/reference, requirements và source entries với hai core-memory files.
- Kiểm tra tài liệu bằng local Markdown links và git diff --check cho hai file được sửa.
- Runner tests/can_driver đạt 11 nhóm fixtures và 9 nhóm production config; Can.c compile ARM với -Wall -Wextra -Werror. Log: build/can_driver/verification.log. CanIf tests và relocatable integration link cũng đạt lại. Chưa full firmware build/link/flash hoặc loopback sau reset; PASS/report của implementation đã xóa vẫn chỉ là lịch sử.
- Khi có implementation mới, thiết lập tests tương ứng với contract/config mới. Host fake registers không thay bằng chứng trên board; loopback không thay test ba ECU.
- Chưa có firmware cộng tác viên hoặc message matrix chính thức; chưa xác nhận CAN ID/bitrate/DLC/endian/scale/timing tương thích liên ECU.
- [can-init-sequence.md](can-init-sequence.md) và API_SPEC chưa được cập nhật trong lượt này; xem như guide/draft cần review, không phải mô tả implementation hiện tại.

## Board loopback harness hiện tại

Chỉ thêm logic vào src/main.c; không tạo source/header/test file mới. Host simulation C stdin dùng modules thật/MMIO transformer pass 18 case và các nhánh lỗi; ARM Cortex-M4 FLASH compile/link từ source/startup/BSP/GPIO/NVIC thật pass. ELF: build/canif/CanIf_loopback.elf; logs main_loopback_host_debug.log và main_loopback_build.log. Debugger: status=2, passedCases=18, txConfirmations=18, rxIndications=18 nghĩa là PASS; FAIL=3 giữ stage/error/dlc/register snapshot. Reset MCU để chạy lại. Chưa flash/test board; internal loopback không kiểm tra dây CAN/transceiver. NXP RM CTRL1[LPB] yêu cầu Freeze khi ghi LPB và SRXDIS=0 để self receive.
