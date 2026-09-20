# Project Context

## CanTp Phase 1 implementation (2026-09-20)

- Phase 1 of `requirements/CanTp_Student_Guide.md` is implemented end to end for one bidirectional connection. The wire format is fixed DLC 8: SF `[00][Length][1..6 data]`, FF `[10][Length][6 data]`, CF `[20|SN][up to 7 data]`, and CTS `[30][04][05][00..]`.
- `drivers/can/cantp/Cantp.c` now performs Tx snapshot, SF/FF/CF segmentation, CTS/block handling, 5 ms STmin scheduling, Rx reassembly, and final callbacks. Tx offset/SN advance only after the matching local Data confirmation. This phase intentionally aborts when CanIf rejects a frame; retry and the four timeouts remain Phase 2.
- The static route is App Global PDU `0x0020` -> CanTp N-SDU 0 -> CanIf Data L-PDU 1 / CAN ID `0x650`; FC uses L-PDU 2 / CAN ID `0x658`. Existing COM remains L-PDU 0 / CAN ID `0x100`.
- `app/node_app.c` owns a stable Tx source and a two-slot Rx queue with `FREE -> RESERVED -> READY`. PduR routes application copy/final callbacks and keeps Data and FC confirmation namespaces separate.
- The 1 ms firmware order is Can Write, Can Read, CanTp, then COM Tx when enabled. Debug_FLASH includes the `app` source folder.
- Evidence: `tests/cantp/run_tests.ps1` passes exact T01-T03 vectors, route/ownership/FIFO queue tests and strict ARM object compilation. CanIf, COM, button and scheduler regressions pass. `build/cantp_phase1/Mock_MCU_Prj_CanTp_Phase1.elf` links for S32K144 FLASH with no undefined symbols (text 22264, data 1072, bss 5832 bytes). Physical-board behavior is not yet verified.

## CanTp guide wire-format update (2026-09-20)

- `requirements/CanTp_Student_Guide.md` v2.1 now consistently follows `cantp_wire_format.txt`: SF `[00][Length][up to 6 data]`, FF `[10][Length][6 data]`, CF `[20|SN][up to 7 data]`, FC unchanged, DLC 8 and zero Tx padding. SF covers N-SDU 1..6 and FF/CF covers 7..62.
- Configuration examples, frame vectors, pack/decode pseudocode, validation boundaries, defensive cases, T01 and the acceptance matrix were updated together. A consistency check found all required new rules, no listed obsolete rules, and balanced Markdown fences.

## Current application entry (2026-09-18)

- src/main.c now contains only the two-board COM application. It has one main() and no BOARD_MODE selector, loopback calls, raw CanIf transmission, or embedded test helpers. SW2/PTC12 toggles Rx/Tx; Tx lights red and SW3/PTC13 updates the LED command through Com_SendSignal. Rx reads through Com_ReceiveSignal after a COM indication.
- CAN0 is 500 kbit/s; production CanIf Tx/Rx use standard ID 0x100 and DLC 8. COM stores the LED command in its existing Gear slot at payload byte 2 as (command << 1) | Update Bit. GlobalPduId 0x0010 is a logical route, not a payload byte. COM Tx runs periodically at 10 ms only while in Tx.
- The 1 ms loop calls Can_MainFunction_Write, Can_MainFunction_Read, CanTp_MainFunction, then Com_MainFunctionTx in Tx. Host application/COM/CanIf mapping tests pass and the complete ARM FLASH ELF links with no undefined symbols. Board behavior and timing remain unverified. Older BOARD_MODE and direct CanIf demo notes below are historical and no longer describe the current entrypoint.

Cập nhật: 2026-09-18, sau khi đổi demo sang CAN ID 0x100/DLC8. Đọc cùng [codebase-map.md](codebase-map.md) trước mỗi task. Snapshot phải được kiểm tra lại nếu người dùng đang thay đổi repo.

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
| CAN stack mới | [can_driver/Can.c](../drivers/can/can_driver/Can.c) có bốn API, direct register access và init helpers với English comments. Hardware hardcode CAN0: oscillator 8 MHz, 500 kbit/s, standard Classical data frames, Tx MB8 và Rx MB9. Driver validate toàn bộ controller/HOH config trước register access và resolve HOH theo ID, độc lập index. Config production có 1 controller instance 0, baudRate 500000, HTH0/HRH1. [CanIf.c](../drivers/can/canif/CanIf.c) đã triển khai Init/Transmit/TxConfirmation/RxIndication; config có Tx/Rx VehicleStatus local ID 0, CAN ID 0x100, HTH0/HRH1. Common đã có Std_ReturnType/E_OK/E_NOT_OK cùng PDU types/GlobalPduIds; CanStack_Cfg.c còn rỗng. |
| Driver/upper/config/test CAN trước đây | Source CAN driver, COM, PduR/CanIf types/config, system matrix và board loopback harness trước đây đã bị xóa. Không còn profile local IDs/CAN ID đã triển khai để dùng làm baseline. |
| Plans và host tests | docs/implement không còn. [tests/can_driver](../tests/can_driver/README.md) đạt 11 nhóm fixtures (14 malformed configs và sparse ID lookup cùng 9 regressions), thêm 9 regressions với config production thật và ARM compile. Đã bỏ reference CAN1 macro cũ. [tests/canif](../tests/canif/README.md) đạt lại 7 nhóm deterministic unit tests, smoke test config thật và ARM compile. Logs: build/can_driver/verification.log, build/canif/verification.log và integration.log. Không dùng kết quả tests đã xóa làm bằng chứng. |
| CAN tham khảo | can_task vẫn còn driver, CanUpper, config và bài hướng dẫn; là bài riêng để tham khảo, không phải mock stack mới. |
| BSP CAN | [board_can.c](../bsp/can/board_can.c) và [board_can.h](../bsp/can/board_can.h) vẫn còn disable_WDOG và init_MCU. |
| App | node_app.c/h triển khai nguồn Tx ổn định và Rx queue FIFO hai slot cho CanTp; gateway_app.c/h vẫn rỗng. |
| Các phần khác | Startup/vendor headers, GPIO/LED, UART, SysTick, LPIT, NVIC, ADC, RTC và ring buffer vẫn còn. Chưa được nối thành mock CAN stack hoạt động. |

## Entrypoint và build cần lưu ý

- Review khả năng nạp (2026-09-15): CanIf_loopback.elf được readelf xác nhận ARM ELF32 executable, có vector table tại Flash 0x0, flash_config tại 0x400 và Reset_Handler entry 0x529; nm không còn undefined symbols. Có thể chọn ELF này trong debug configuration cho S32K144 để nạp, nhưng chưa xác nhận flash/board runtime. can_task/test/main.c vẫn #if 0 và ngoài source entries, không phải harness trong ELF này.

- [src/main.c](../src/main.c) runs the COM two-board application and initializes NodeApp/CanTp as part of the complete stack. Each 1 ms tick polls Can Write/Read, then CanTp, and runs COM Tx in Tx mode. SW2 changes role, SW3 updates the LED command through Com_SendSignal, and Rx uses Com_ReceiveSignal.
- [.cproject](../.cproject): Debug_FLASH lấy source từ Project_Settings, app, bsp, drivers, include, middlewares và src. Ba cấu hình Release_FLASH/Debug_RAM/Release_RAM chỉ lấy Project_Settings, include và src.
- can_task không nằm trong source entries của cả bốn cấu hình. Harness can_task/test/main.c còn bị bọc trong #if 0.
- Generated Debug_FLASH vẫn có source lists cũ trỏ tới các CAN folders đã xóa; IDE cần regenerate khi build Debug_FLASH. Firmware harness đã compile/link FLASH riêng từ source thật cùng startup, BSP, GPIO/NVIC và CanDrv/CanIf/config: build/canif/CanIf_loopback.elf. Chưa flash hoặc chạy board.

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

- Comment main.c: thêm banner /*============== ... *==============*/ cho Can Test, CanIf Test, PduR Test và shared helpers/checks để phân biệt module; thay đổi này chỉ là comments, không thay logic test.

- Board harness mốc PduR Tx ngày 2026-09-15: 27 case DLC0..8 và 4 invalid requests đã pass host, ELF cũ build/canif/CanIf_loopback.elf. Mốc này được mở rộng bởi COM/PduR Rx ngày 2026-09-16 bên dưới; callbacks không còn nằm trong main.c.

- CanDrv validation đã bổ sung (2026-09-15) sau review: Can_ValidateConfig kiểm tra toàn bộ controller/HOH, unique controller IDs và HOH IDs xuyên Tx/Rx, mỗi HOH resolve đúng một controller, object type và unique controller/physical MB pair. Từ chối profile khác 1 instance-0 controller/500000 baud/2 HOH Tx MB8 và Rx MB9. Can_GetController/Can_GetHardwareObject dùng ID lookup, không giả định index; Can_Write resolve Tx HOH, Rx dùng object đã resolve để trả configured HRH. Không thay đổi config production hoặc register stages. CAN_LOG_CONFIG_ERROR.detail chứa reason ở 16 bit cao và table index ở 16 bit thấp. Tests fixture/production và ARM pass; CanDrv+CanIf relocatable link chỉ còn hai PduR callbacks undefined. Baseline runner lỗi CAN_HTH_CAN1_TX trước thay đổi được lưu trong build/can_driver/config_validation_baseline_compile.log. Chưa full firmware/board test.

- CanIf Part 1 đã triển khai đúng bốn APIs trong CanIf.h. Init validate CAN ID/unique Tx ID+CAN ID, unique Rx ID+HRH/CAN ID key và HOH đúng loại với controller tồn tại. Transmit dùng local ID lookup, giữ swPduHandle, gọi driver một lần và chỉ CAN_OK trở thành E_OK; không queue/retry/clear U. Callback Tx chuyển local ID; exactly-once thuộc driver. Rx chuyển bytes đồng bộ tới PduR và không giữ pointer. Structured diagnostics: CanIf_LogRecords/CanIf_LogSequence, chạy tuần tự main context. Source và helpers đều có comments tiếng Anh.

- Verification mới: tests/canif/run_tests.ps1 đạt 7 nhóm unit tests với malformed fixtures/sparse IDs và production-config smoke test; compile CanIf/configs ARM với -Wall -Wextra -Werror. Compile driver hiện tại và relocatable link CanDrv+CanIf+configs đạt; chỉ còn undefined PduR_CanIfTxConfirmation và PduR_CanIfRxIndication. Chưa full firmware link hoặc board test.

- Refactor theo yêu cầu người dùng: đúng file drivers/can/can_driver/Can.c (đã xác nhận lại sau link ban đầu tới can_task). Đã bỏ CAN_MMIO_READ32/WRITE32 và Can_ModifyRegister; source production truy cập thanh ghi volatile trực tiếp. Can_Init gọi static helpers DisableController (kèm chọn clock), EnableController, EnterFreezeMode, ResetController, ConfigureController, InitMessageBuffers và ExitFreezeMode. Mỗi helper có comment tiếng Anh; bounded waits và timeout stage IDs 1..8 giữ nguyên. Tests dùng instrument_driver.py tạo bản sao host-only trong build/can_driver; 9 nhóm regression tests và original-source ARM compile đạt. Baseline trước refactor lưu tại build/can_driver/refactor_baseline.log. Không sửa can_task hoặc .gitignore.

- Implementation CAN0 mới thay thế các đề xuất chưa triển khai trong [can-driver-design.md](can-driver-design.md) đối với profile tạm. Can_Init đã thống nhất return type Can_ReturnType. Driver copy payload trước CAN_OK, giữ swPduHandle, release trước TxConfirmation và chuyển Rx theo HRH + CAN ID. Init có bounded waits; bus-off/fatal fault latch ERROR và request Freeze; gọi Init lại sau khi sửa nguyên nhân sẽ reset CAN0 và bỏ request lỗi không success-confirm. Rx acknowledge IFLAG trước TIMER unlock, không ép RX_EMPTY sau khi service. Structured logs nằm trong Can_LogRecords/Can_LogSequence để xem bằng debugger.

- CanIf cung cấp hai callbacks cho CanDrv. PduR_ComTransmit, PduR_CanIfRxIndication và PduR_CanIfTxConfirmation đã nối tới COM theo route Tx/Rx; không có PduR Init API. COM có Com_Init, Com_SendSignal, Com_ReceiveSignal, Com_MainFunctionTx, Com_RxIndication và Com_TxConfirmation. Config production có một Tx I-PDU và một Rx I-PDU cùng GlobalPduId VehicleStatus, ba Signal mỗi chiều. BSP/Can_Init chạy trước CanIf_Init và Com_Init; harness bật loopback qua Freeze. Không chạy API đồng thời hoặc xử lý cùng MB bằng IRQ.

## COM/PduR Rx integration (2026-09-16)

- COM slot dùng little-endian byte order trong mỗi slot byte-aligned; U là bit0. Com_Init từ chối hierarchy/ID/slot/timing sai. Tx offset1/period10/maxRetries3; mỗi tick là một lần gọi Com_MainFunctionTx do caller cung cấp, không có timer tự động trong module. COM giữ vòng 16 structured log records cho init/config/Tx retry/drop/Rx, cộng debugger counters.
- PduR route Tx từ COM IPDU0 sang CanIf Tx0, route Rx từ CanIf Rx0 sang COM Rx IPDU1; cả hai cùng GlobalPduId 0x0010. TxConfirmation/RxIndication đi qua callbacks thật. Debug counters ở PduR và COM giữ kết quả cho main.
- Loopback module tiếp tục 27 case cũ, sau đó chạy COM: giá trị đầu 100/gear3/alive5, truyền t=1; t=11 CAN MB bận, t=12 retry với speed120; nhận lại qua COM và kiểm tra U. PASS=2, FAIL=3, LED xanh khi pass. Đây là tick mô phỏng trong harness, không phải kiểm định 1 ms thực trên board.
- `tests/com/test_com.c` pass pack/Rx/offset/retry/latest/drop; `test_com_config.c` pass 12 malformed fixtures. Host full-stack simulation pass. CanDrv 11+9 nhóm tests và CanIf 7 nhóm + production smoke pass. ARM GCC `-Wall -Wextra -Werror` compile/link FLASH ELF `build/com/Com_full_loopback.elf` không còn undefined symbol. Logs: `build/com/build.log`, `build/com/main_host.log`. Chưa flash/chạy board.
- `.gitignore` giữ các test cũ bị ignore và cho phép `tests/com/` hiện trong thay đổi để review cùng COM implementation.

## TC-003 hai board và UART (2026-09-17)

- `src/can_loopback_test.c` giữ test cũ trong `CanLoopbackTest_Run()` với các hàm riêng `Loopback_TestCanDriver`, `Loopback_TestCanIf`, `Loopback_TestPduR`, `Loopback_RunComTest`; `src/main.c` chọn image bằng `BOARD_MODE=0/1/2` và giữ cả hai mode TC-003. Mode 1 dùng SW2/PTC12 debounce 20 ms, phát `CA command sequence`, command lần lượt 1=blue, 2=red, 3=green, 0=off. Mode 2 kiểm tra DLC/magic/command/sequence trùng rồi bật LED tương ứng. Hai mode vật lý không bật FlexCAN internal loopback.

## Tách loopback test khỏi main (2026-09-18)

- Chỉ chuyển 627 dòng logic test cũ sang `src/can_loopback_test.c`, đổi hàm entry thành `CanLoopbackTest_Run()` và khai báo trong `src/can_loopback_test.h`; phần TC-003 Tx/Rx trong main giữ nguyên. So sánh source trước/sau xác nhận logic loopback và khối TC-003 không đổi.
- ARM FLASH `-Wall -Wextra -Werror` build/link lại cả `BOARD_MODE=0/1/2` với source mới trong `build/loopback_split/`; `nm -u` không còn symbol thiếu. Host tests Tx/Rx vẫn pass. S32DS cần regenerate source list để thêm `src/can_loopback_test.c`. Chưa xác minh runtime trên board.
- Bản demo dùng CanIf_Transmit trực tiếp cho frame 3 byte TC-003 và PduR debug Rx snapshot có sẵn để đọc frame; COM VehicleStatus hiện cấu hình 8 byte nên không được dùng cho frame này. CAN ID thực là 0x321 theo CanIf_Cfg.c, khác 0x123 của bài tham khảo. UART LPUART1 PTC7 TX/PTC6 RX, 115200 8N1; code in payload và trạng thái Tx/Rx, không nhận lệnh PC.
- `tests/board_demo/test_tx.c` và `test_rx.c` pass host test debounce/frame/LED/validation/duplicate/UART. ARM GCC `-Wall -Wextra -Werror` compile/link cả ba `BOARD_MODE` ra `build/board_demo/{0,1,2}/board_mode_{0,1,2}.elf`, không có undefined symbols. Chưa flash/chạy trên hai board. Cách nạp và nối dây ở `tests/board_demo/README.md`.

- Lịch sử driver: 9 nhóm host tests và ARM compile từng đạt, baseline compiler error ở build/can_driver/baseline.log; lượt validation sau đạt 11 nhóm và production regression. Harness FLASH hiện đã compile/link riêng; chưa flash hoặc chạy CAN vật lý. Host tests không chứng minh bitrate/clock/pins/transceiver trên board hoặc toàn stack compliance.
- Chưa có firmware của hai cộng tác viên, message matrix chính thức hoặc test vector liên ECU trong workspace để xác nhận tương thích.
- Khi người dùng yêu cầu triển khai tiếp: đối chiếu assignment, xác định types/config/API mới, tham khảo can_task và BSP còn lại. Mọi phần CAN mới ngoài BSP vẫn đặt dưới drivers/can theo yêu cầu tổ chức project.
- Xử lý entrypoint và source entries khi tích hợp lại; không coi guide, config hoặc kết quả test của source đã xóa là trạng thái hiện hành.

## Checklist yêu cầu Part 1 (2026-09-18)

- [Checklist Part 1](../requirements/part1_com_signal_checklist.md) tổng hợp 78 mục từ assignment, architecture notes và scheduler tip. Review source/test ngày 2026-09-18 đánh dấu 36 mục đạt, 42 mục còn mở, mỗi mục có bằng chứng hoặc lý do thiếu. Scheduler tip vẫn là khuyến nghị.
- Đã chạy lại CanDrv/CanIf runners và hai COM host tests; log ở `build/can_driver/verification.log`, `build/canif/verification.log`, `build/com/review_verification.log`. Test sources dưới `tests/*` hiện bị `.gitignore` bỏ qua. Ba khoảng trống chính: PduR chưa validate route/global binding xuyên tầng, CanDrv từ chối cấu hình nhiều Controller, main hai-board chưa gọi COM mỗi 1 ms. Chưa kiểm chứng board/physical CAN.

## COM Signal pointer API (2026-09-18)

- Theo lựa chọn của người dùng, `Com_SendSignal(SignalId, const void *SignalDataPtr)` và `Com_ReceiveSignal(SignalId, void *SignalDataPtr)` đều nhận đúng hai tham số; trong profile hiện tại con trỏ phải trỏ tới `uint32_t`. Receive chỉ xuất giá trị, không xuất Update Bit; U vẫn được COM giữ/xóa theo chính sách Tx và giải mã nằm trong raw Rx slot. Loopback harness kiểm U qua `PduR_LastRxBytes`.
- Đã cập nhật caller trong `src/can_loopback_test.c` và hai COM host tests (test sources bị ignore). Host COM tests và 12 malformed config fixtures pass sau đổi API; `Com.c`, `can_loopback_test.c` và `main.c` compile ARM với `-Wall -Wextra -Werror`. Mode COM mặc định đã link thành ELF ARM sau thay đổi chữ ký; chưa flash/test board.

## Main loop CAN/COM 1 ms (2026-09-18)

- `BOARD_MODE=3` cho Part 1 (đã là mặc định trước khi thêm mode nút bấm): init Can→CanIf→Com, cập nhật `SystemCoreClock`, khởi tạo SysTick 1000 Hz; main xử lý từng tick theo thứ tự `Can_MainFunction_Write()` → `Can_MainFunction_Read()` → `Com_MainFunctionTx()`. Nếu main trễ, xử lý bù các tick đã trôi qua; `g_ComStackProcessedTicks`, `g_ComStackMaxBacklog`, `g_ComStackStatus` cho debugger. Mode 0 loopback và 1/2/4 demo vẫn tách biệt vì cùng CAN ID 0x100 nhưng layout payload khác nhau.
- `tests/com_stack/test_main_scheduler.c` kiểm tra thứ tự WRC, tick không đổi, bù tick và wraparound; pass. `build/com_stack/com_stack.elf` full ARM FLASH compile/link với macro CPU từ `.cproject`, không còn undefined symbol. Chưa đo jitter/tần số thực trên board nên checklist T02 vẫn mở; I04 đã đạt. Thất bại build thử ban đầu do thiếu CPU macro/sysroot được lưu ở `build/com_stack/build_failure.log` và `build/com_stack/build.log`; bản link thành công ở `build/com_stack/build_result.log`.

## Hai nút điều khiển vai trò board (2026-09-18)

- Current src/main.c has no BOARD_MODE selector or embedded test branch. It starts in Rx, SW2/PTC12 toggles Rx/Tx, and SW3/PTC13 updates the COM LED command only in Tx. The Tx role lights red; Rx waits for a COM indication before changing LEDs. UART logs logical mode and command.
- Current main uses the 1 ms Can Write -> Can Read -> COM Tx order in Tx. COM owns packing, so the prior raw byte-1 LED demo format no longer applies. The production frame is CAN ID 0x100, DLC8, with the LED command and Update Bit in byte 2; GlobalPduId 0x0010 is a logical route.
