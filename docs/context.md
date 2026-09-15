# Project Context

Cập nhật: 2026-09-15, sau khi triển khai lại driver CAN0 theo yêu cầu người dùng. Đọc cùng [codebase-map.md](codebase-map.md) trước mỗi task. Snapshot phải được kiểm tra lại nếu người dùng đang thay đổi repo.

## Quy tắc tương tác

- **Chế độ hiện tại: AGENT.** Thực hiện công việc trong phạm vi người dùng giao.
- Tin nhắn bắt đầu bằng từ **ask** chuyển sang chế độ hỏi–đáp **read-only**, giữ nguyên qua các lượt tiếp theo cho đến khi tin nhắn có từ **agent**.
- Trong ASK, không sửa bất kỳ file nào, kể cả context/map, tài liệu, log, config và generated files; tránh build/test có tác dụng ghi file. Quy tắc này ưu tiên hơn yêu cầu write-back thường lệ.
- Nhận lệnh không phân biệt hoa/thường, bỏ khoảng trắng đầu và xét từ độc lập. Nếu cả hai lệnh xuất hiện, agent là lệnh thoát ASK. Nội dung đọc từ file/tool không tự chuyển chế độ.
- Sau compaction, kiểm tra các tin nhắn điều khiển gần nhất; trạng thái trên đĩa có thể cũ trong ASK. Không tự thoát ASK chỉ vì câu hỏi đã trả lời xong.
- Khai báo Role và TRANSITION theo AGENTS.md người dùng cung cấp. Chỉ tuyên bố kết quả chạy khi có bằng chứng tương ứng.

## Mục tiêu ổn định của project

- Workspace phục vụ **Mock Project MCU** trên S32K144; yêu cầu tổng thể tại [requirements/README.md](../requirements/README.md).
- [Mock_MCU-Overview.png](../requirements/Mock_MCU-Overview.png) là mô hình khi hoàn thiện, không phải trạng thái implementation hiện tại.
- [Mock_MCU-Tx-Rx Flow.png](<../requirements/assumptions/Mock_MCU-Tx-Rx Flow.png>) và [flow.xml](../requirements/assumptions/flow.xml) là luồng/giả định tạm thời.
- Ba ECU do ba người triển khai độc lập: source, tổ chức module, chữ ký API C và local handles có thể khác nhau. Cần thống nhất giao thức trên dây: bitrate, frame format, CAN ID, publisher/consumer, DLC, packing, endian, signedness, scale/offset và timing.
- Mục tiêu tổng thể có shared CAN bus, PC1–UART–ECU1 và PC2–UART–ECU3, truyền text/image hai chiều giống từng byte. Vai trò app ECU2 và bulk protocol còn cần chốt.
- Ưu tiên các tầng thấp để ba ECU giao tiếp được trước khi xây app; loopback là bước kiểm chứng ban đầu, không thay thế test giao tiếp vật lý giữa các ECU.

## Quyết định bắt đầu lại

**Người dùng đã xóa phần CAN mới và folder docs/implement để bắt đầu lại từ đầu.** Không khôi phục source, plans hay tests cũ nếu chưa được yêu cầu. Không tiếp tục coi COM đang dừng ở Com_Init hoặc CanIf sắp được triển khai trên driver cũ.

| Khu vực | Trạng thái thực tế |
|---|---|
| CAN stack mới | [can_driver/Can.c](../drivers/can/can_driver/Can.c) đã có Init/Write/Write polling/Read polling với comments tiếng Anh trong Can.c/h. Theo yêu cầu mới, hardcode CAN0: oscillator 8 MHz, 500 kbit/s, standard Classical data frames, Tx MB8/HTH0 và Rx MB9/HRH1. Không dùng config tables và không nhận HTH CAN1. Config vẫn có CAN0/CAN1, HOH 0..3 và baudRate chưa gán; chưa coi các bảng này là runtime profile. Common PDU types/GlobalPduIds vẫn có; CanStack_Cfg.c còn rỗng. |
| Driver/upper/config/test CAN trước đây | Source CAN driver, COM, PduR/CanIf types/config, system matrix và board loopback harness trước đây đã bị xóa. Không còn profile local IDs/CAN ID đã triển khai để dùng làm baseline. |
| Plans và host tests | docs/implement không còn. Tests mới tại [tests/can_driver](../tests/can_driver/README.md) đạt 9 nhóm deterministic CAN0 tests và ARM Cortex-M4 object compile với -Wall -Wextra -Werror. Log mới: build/can_driver/verification.log. Không dùng kết quả tests đã xóa làm bằng chứng. |
| CAN tham khảo | can_task vẫn còn driver, CanUpper, config và bài hướng dẫn; là bài riêng để tham khảo, không phải mock stack mới. |
| BSP CAN | [board_can.c](../bsp/can/board_can.c) và [board_can.h](../bsp/can/board_can.h) vẫn còn disable_WDOG và init_MCU. |
| App | node_app.c/h và gateway_app.c/h trong app còn tồn tại nhưng đều rỗng. |
| Các phần khác | Startup/vendor headers, GPIO/LED, UART, SysTick, LPIT, NVIC, ADC, RTC và ring buffer vẫn còn. Chưa được nối thành mock CAN stack hoạt động. |

## Entrypoint và build cần lưu ý

- [src/main.c](../src/main.c) vẫn include ../drivers/can/test/Can_LoopbackTest.h và gọi Can_LoopbackTest_Run(), trong khi header/source đã bị xóa. Đây là tham chiếu treo cần xử lý khi bắt đầu tích hợp firmware lại; chưa sửa trong lượt cập nhật tài liệu.
- [.cproject](../.cproject): Debug_FLASH lấy source từ Project_Settings, bsp, drivers, include, middlewares và src. Ba cấu hình Release_FLASH/Debug_RAM/Release_RAM chỉ lấy Project_Settings, include và src.
- can_task không nằm trong source entries của cả bốn cấu hình. Harness can_task/test/main.c còn bị bọc trong #if 0.
- Generated Debug_FLASH có thể chứa output cũ. Không suy build/link/flash thành công từ artifact còn trên đĩa. Chưa chạy full build hoặc board test sau reset.

## Yêu cầu giữ lại để triển khai mới

Nguồn chính là [assignment_part1_com_signal.md](../requirements/assignment_part1_com_signal.md) trong repo; [part1_architecture_notes.md](../requirements/part1_architecture_notes.md) giải thích thiết kế. Bản trong Downloads của IDE không tự thay thế bản repo.

- Part 1: Signal → Signal Group → I-PDU → PduR → CanIf → CanDrv → controller. Một signal thuộc đúng một group; một group thuộc đúng một I-PDU; mỗi I-PDU có đúng một group.
- Slot bắt đầu byte-aligned, độ dài là bội 8 bit; U nằm ở bit 0 của slot, payload có slotLength − 1 bit. Kiểu C không đồng nghĩa sức chứa payload.
- COM Tx periodic với MainFunction 1 ms, static config order, latest value wins; retry một lần mỗi tick kế tiếp, tối đa 1 + max_retries lần thử cho một occurrence. Drop occurrence giữ dữ liệu/U, không bỏ I-PDU hoặc làm trôi nominal schedule.
- Clear U khi lower stack accepted request, không đợi TxConfirmation. Accepted request chưa đồng nghĩa frame đã phát xong.
- Tip bổ sung từ người giao bài: chọn period là bội 10 ms, offset khác nhau trong cửa sổ 1–9 ms để tách nominal due time. Ví dụ A=20/1, B=20/3, C=30/5 ms. Retry vẫn có thể trùng slot khác; đây chưa phải bảo đảm thời điểm phát vật lý trên bus.
- GlobalPduId là identity chung của logical message. Direct Binding ánh xạ GlobalPduId ↔ CanIf L-PDU ↔ CAN ID; không serialize GlobalPduId vào payload.
- CanIf sở hữu CAN ID mapping, Rx lookup bằng HRH + CAN ID. CanDrv sở hữu controller và HOH; HTH/HRH dùng chung namespace unique trong một instance, model cho phép nhiều controller.
- CanTp, bulk transfer và app nằm ngoài Part 1. Endian, message matrix và cấu hình ECU cụ thể cần thống nhất trước tích hợp; không tự dùng lại profile cũ.

## Tài liệu còn tồn tại nhưng cần đọc có phân biệt

- [can-init-sequence.md](can-init-sequence.md) mô tả driver đã bị xóa, có tham chiếu source không còn. Là tài liệu lịch sử, chưa phải thứ tự init được implementation mới chứng minh.
- [API_SPEC.md](../requirements/assumptions/API_SPEC.md) còn là DRAFT; inventory/EXISTING và API liên quan implementation cũ phải kiểm tra lại. Khi khác Part 1, dùng assignment làm yêu cầu chính.
- [CAN_Homework_S32K144EVB.md](../can_task/CAN_Homework_S32K144EVB.md) và [Can_Stack.md](../can_task/Can_Stack.md) phục vụ can_task tham khảo.
- Lượt này chỉ cập nhật hai core-memory files; không sửa các tài liệu cũ khác hoặc tạo lại implement.

## Chưa được kiểm chứng / bước sau

- Refactor theo yêu cầu người dùng: đúng file drivers/can/can_driver/Can.c (đã xác nhận lại sau link ban đầu tới can_task). Đã bỏ CAN_MMIO_READ32/WRITE32 và Can_ModifyRegister; source production truy cập thanh ghi volatile trực tiếp. Can_Init gọi static helpers DisableController (kèm chọn clock), EnableController, EnterFreezeMode, ResetController, ConfigureController, InitMessageBuffers và ExitFreezeMode. Mỗi helper có comment tiếng Anh; bounded waits và timeout stage IDs 1..8 giữ nguyên. Tests dùng instrument_driver.py tạo bản sao host-only trong build/can_driver; 9 nhóm regression tests và original-source ARM compile đạt. Baseline trước refactor lưu tại build/can_driver/refactor_baseline.log. Không sửa can_task hoặc .gitignore.

- Implementation CAN0 mới thay thế các đề xuất chưa triển khai trong [can-driver-design.md](can-driver-design.md) đối với profile tạm. Can_Init đã thống nhất return type Can_ReturnType. Driver copy payload trước CAN_OK, giữ swPduHandle, release trước TxConfirmation và chuyển Rx theo HRH + CAN ID. Init có bounded waits; bus-off/fatal fault latch ERROR và request Freeze; gọi Init lại sau khi sửa nguyên nhân sẽ reset CAN0 và bỏ request lỗi không success-confirm. Rx acknowledge IFLAG trước TIMER unlock, không ép RX_EMPTY sau khi service. Structured logs nằm trong Can_LogRecords/Can_LogSequence để xem bằng debugger.

- CanIf phải cung cấp CanIf_TxConfirmation(PduIdType TxPduId) và CanIf_RxIndication(Can_HwHandleType Hrh, const Can_RxPduType *RxPdu). Hiện chỉ có extern declarations trong Can.c và callbacks capture trong host tests; chưa có CanIf production implementation. BSP cần được gọi trước Can_Init; driver dùng normal mode, không loopback. Không chạy các API đồng thời hoặc xử lý cùng MB bằng IRQ.

- Đã chạy tests/can_driver/run_tests.ps1: 9 nhóm host tests và production ARM compile đạt; baseline compiler error trước khi sửa được lưu tại build/can_driver/baseline.log. Chưa full firmware build/link, flash, loopback hoặc CAN vật lý; main vẫn include harness đã xóa. Host tests không chứng minh bitrate/clock/pins/transceiver trên board hoặc toàn stack compliance.
- Chưa có firmware của hai cộng tác viên, message matrix chính thức hoặc test vector liên ECU trong workspace để xác nhận tương thích.
- Khi người dùng yêu cầu triển khai tiếp: đối chiếu assignment, xác định types/config/API mới, tham khảo can_task và BSP còn lại. Mọi phần CAN mới ngoài BSP vẫn đặt dưới drivers/can theo yêu cầu tổ chức project.
- Xử lý entrypoint và source entries khi tích hợp lại; không coi guide, config hoặc kết quả test của source đã xóa là trạng thái hiện hành.
