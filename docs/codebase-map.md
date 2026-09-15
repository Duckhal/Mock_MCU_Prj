# Codebase Map

Snapshot: 2026-09-15, sau reset phần CAN. Quy tắc tương tác và mục tiêu: [context.md](context.md). Map này phản ánh file còn trên đĩa và source entries; không chứng minh firmware chạy được.

## Tổng quan

Project C bare-metal S32 Design Studio/Eclipse cho S32K144. Mục tiêu là mock ba ECU giao tiếp CAN với hai đầu UART nối PC. Người dùng đã xóa stack CAN mới và docs/implement để triển khai lại.

Trong drivers/can hiện chỉ có ba file rỗng trong common. can_task và BSP CAN vẫn còn. Entrypoint hiện chứa tham chiếu đến loopback harness đã xóa, nên chưa có luồng CAN hoạt động được xác nhận.

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
  can/common/                CanStack_Types.h, CanStack_Cfg.h/c — đều rỗng
  adc/, common/, gpio/, lpit/, nvic/, rtc/, systick/, uart/
middlewares/                 ring_buffer.c/h
src/                         main.c — vẫn gọi harness CAN đã xóa
requirements/                assignment, architecture notes, README, overview
  assumptions/               API_SPEC, flow.xml, Tx-Rx Flow PNG
docs/                        context.md, codebase-map.md, can-init-sequence.md
Project_Settings/            startup, linker, debugger
include/                     vendor/device headers
Debug_FLASH/                 generated build output có thể cũ
~~~

docs/implement không tồn tại. Không còn file tests trong thư mục tests lúc khảo sát. CAN driver/comm/config/test trước reset không còn trong drivers/can.

## Module map

| Module | File/ranh giới còn tồn tại | Phụ thuộc và trạng thái |
|---|---|---|
| Entrypoint | [src/main.c](../src/main.c) | Include Can_LoopbackTest.h và gọi Can_LoopbackTest_Run từ harness đã xóa; chưa được chỉnh sau reset. |
| Startup/linker | Project_Settings/Startup_Code, Project_Settings/Linker_Files | Reset/vector/memory layout và vendor headers trong include; chưa audit toàn bộ ở lượt này. |
| CAN common mới | [CanStack_Types.h](../drivers/can/common/CanStack_Types.h), [CanStack_Cfg.h](../drivers/can/common/CanStack_Cfg.h), [CanStack_Cfg.c](../drivers/can/common/CanStack_Cfg.c) | Cả ba 0 byte; chưa có types, config, API hay consumer. |
| CAN BSP | [board_can.c](../bsp/can/board_can.c), [board_can.h](../bsp/can/board_can.h) | disable_WDOG, init_MCU; sử dụng S32K144.h và LED BSP. Chưa được main hiện tại gọi trực tiếp. |
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
  -> include Can_LoopbackTest.h      [file đã xóa]
  -> Can_LoopbackTest_Run()          [implementation đã xóa]
  -> vòng lặp vô hạn
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

COM sở hữu packing/timing; PduR sở hữu routing; CanIf sở hữu CAN ID mapping; CanDrv sở hữu HOH/controller. GlobalPduId là identity hệ thống qua config và implicit trên dây trong Direct Binding. Chưa có runtime hoặc profile mới cho luồng này.

## Build và entrypoints

- Debug_FLASH source entries: Project_Settings (loại Linker_Files và Debugger), bsp, drivers, include, middlewares, src.
- Release_FLASH, Debug_RAM, Release_RAM: Project_Settings với cùng exclusions, include, src. Chưa đồng nhất source set với Debug_FLASH.
- can_task và app không nằm trong source entries của cả bốn cấu hình.
- Entrypoint firmware là src/main.c. can_task/test/main.c không phải main đang build và hiện bị disabled bằng #if 0.
- Thiếu header loopback mà src/main.c include là trở ngại source rõ ràng sau reset. Chưa chạy full build để liệt kê thêm lỗi.
- Khi được yêu cầu tích hợp/build: sửa entrypoint/source entries trong project metadata và regenerate bằng S32DS. Không dùng generated makefiles/ELF cũ làm nguồn sự thật.

## Điểm sửa cho công việc tiếp theo

- Types/config/API/logic CAN mới: dưới drivers/can; ba file common hiện chỉ là điểm khởi đầu rỗng. Chưa có cấu trúc module hoặc chữ ký API mới được implementation xác nhận.
- Clock/pin/transceiver: BSP [board_can.c](../bsp/can/board_can.c). Hiện setup SOSC ngoài 8 MHz, PORTC/PORTE/FlexCAN0 gate, PTE4 RX/PTE5 TX ALT5 và GPIO PTC14/PTE11 cho transceiver. Watchdog/SOSC waits chưa có timeout.
- Reference register/MB handling: can_task/driver và guide [CAN_Homework_S32K144EVB.md](../can_task/CAN_Homework_S32K144EVB.md). Tách coupling CanUpper và đối chiếu assignment khi tái sử dụng.
- Model ownership, slot, retry, Direct Binding: đọc assignment/notes trước; không tiếp tục các plan đã xóa hoặc tự dùng lại local IDs của profile cũ.
- Timing profile mới cần giữ tip: MainFunction COM 1 ms, period bội 10 ms, offsets khác nhau trong 1–9 ms. Offset thuộc COM I-PDU; retry next tick có thể trùng nominal slot khác.
- UART/gateway/app/bulk: giữ ở phạm vi về sau; chưa có consumer hoặc protocol hiện hành để tiếp tục.
- Trong ASK chỉ đọc/giải thích, không sửa core memory hay chạy tool ghi file.

## Validation và giới hạn bằng chứng

- Lượt cập nhật này đối chiếu file inventory, dung lượng placeholder, main/BSP/reference, requirements và source entries với hai core-memory files.
- Kiểm tra tài liệu bằng local Markdown links và git diff --check cho hai file được sửa.
- Không còn host test runner mới để chạy lại; PASS/report từ implementation đã xóa là lịch sử. Chưa chạy build/link/flash hoặc loopback sau reset.
- Khi có implementation mới, thiết lập tests tương ứng với contract/config mới. Host fake registers không thay bằng chứng trên board; loopback không thay test ba ECU.
- Chưa có firmware cộng tác viên hoặc message matrix chính thức; chưa xác nhận CAN ID/bitrate/DLC/endian/scale/timing tương thích liên ECU.
- [can-init-sequence.md](can-init-sequence.md) và API_SPEC chưa được cập nhật trong lượt này; xem như guide/draft cần review, không phải mô tả implementation hiện tại.
