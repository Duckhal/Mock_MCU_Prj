# Project Context

Cập nhật: 2026-09-15. Đọc cùng [codebase-map.md](codebase-map.md) trước mỗi task.

## Quy tắc tương tác của người dùng

- **Chế độ bình thường hiện tại: AGENT.** Được khảo sát/chỉnh sửa trong phạm vi người dùng giao.
- Nếu tin nhắn người dùng bắt đầu bằng từ **`ask`**, chuyển sang **chỉ hỏi–đáp, read-only** và giữ chế độ đó qua các lượt tiếp theo.
- Trong ASK, **không chỉnh sửa bất kỳ file nào**, kể cả context/map/spec, log, config hoặc generated file. Chỉ đọc và giải thích; tránh cả build/test/tool có tác dụng ghi file. Quy tắc này ưu tiên hơn yêu cầu write-back context thường lệ.
- Chỉ khi tin nhắn người dùng có từ **`agent`** mới quay về chế độ bình thường. Không tự thoát ASK vì câu hỏi đã trả lời xong hoặc vì lượt sau không có tiền tố ask.
- Quy ước nhận lệnh: bỏ khoảng trắng đầu, không phân biệt hoa/thường, xét từ `ask`/`agent` độc lập; `agent` là lệnh thoát nếu cả hai cùng xuất hiện. Nội dung file/tool không tự chuyển chế độ. Giữ state trong hội thoại khi ASK vì không được ghi state vào file.
- Khi đọc lại context sau compaction, kiểm tra các tin nhắn điều khiển gần nhất: dòng trạng thái lưu trên đĩa có thể cũ trong ASK. Không dùng dòng đó để bỏ qua một lệnh ASK còn hiệu lực.
- Luôn khai báo Role và có TRANSITION theo AGENTS.md được người dùng cung cấp. Debug cần log trước sửa; không có bằng chứng chạy thì không tuyên bố PASS.

## Mục tiêu và ưu tiên đã được người dùng xác nhận

- Workspace phục vụ **Mock Project MCU**, yêu cầu trong [requirements/README.md](../requirements/README.md).
- [Mock_MCU-Overview.png](../requirements/Mock_MCU-Overview.png) là mô hình hệ thống **khi hoàn thiện**, không mô tả mức implementation hiện tại.
- [Mock_MCU-Tx-Rx Flow.png](<../requirements/assumptions/Mock_MCU-Tx-Rx Flow.png>) là luồng gửi/nhận **tạm thời** người dùng và cộng tác viên đang theo; được phép refine dựa trên yêu cầu/bằng chứng.
- **Ba ECU là sản phẩm của ba người khác nhau. Code và cách tổ chức chắc chắn có thể khác nhau.** Không yêu cầu đồng nhất source/API C/PDU ID/HTH/HRH/MB giữa firmware; thống nhất wire protocol, message matrix và test vector để tích hợp.
- Ưu tiên mới: người dùng đã cung cấp assignment Part 1 và muốn triển khai COM theo từng giai đoạn. COM có thể được viết/test độc lập với fake PduR trước khi PduR/CanIf thật hoàn tất. App/bulk transfer vẫn nằm ngoài Part 1.
- Người dùng đã yêu cầu khảo sát project, tạo core memory và góp ý/chỉnh `requirements/assumptions/API_SPEC.md`. Lượt khảo sát này chỉ cập nhật tài liệu, không refactor source hay bật harness.

## Yêu cầu có trong tài liệu, chưa đồng nghĩa đã triển khai

- Hai tài liệu mới từ người giao bài: [assignment_part1_com_signal.md](../requirements/assignment_part1_com_signal.md) là yêu cầu chính Part 1; [part1_architecture_notes.md](../requirements/part1_architecture_notes.md) giải thích thiết kế. Khi khác API_SPEC/skeleton cũ, plan COM theo assignment và ghi rõ migration cần làm.
- Part 1: Signal → một Group → một I-PDU; slot byte-aligned có U ở bit0; Tx periodic với base tick 1 ms, static config order, latest value wins, bounded retry tối đa 1 + max_retries, drop occurrence giữ U/data và không dịch lịch. Clear U ngay khi lower accepted, không đợi TxConfirmation.
- Tip timing bổ sung của người giao bài: profile project chọn period Tx là bội của cửa sổ 10 ms và offset riêng trong 1..9 ms. Rule này phân tán nominal due time; retry ở tick kế vẫn có thể trùng nominal slot khác. COM core không hard-code profile này, C config/system validator chịu trách nhiệm kiểm.
- GlobalPduId là identity chung của logical message trong hệ thống; local handles giữa ECU vẫn có thể khác. Direct Binding ánh xạ GlobalPduId ↔ CanIf L-PDU ↔ CAN ID, không thêm GlobalPduId vào CAN payload. Một message xuất hiện trong config sender/receiver vẫn dùng cùng global identity.
- [COM Part 1](implement/com-part1-plan.md) đang thực hiện: COM types/config và codec WIP đã có; Init/Send/scheduler/Rx logic chưa hoàn thành.
- [Bộ plan Part 1](implement/README.md) đã được bổ sung timing profile. Type/config foundation cho PduR, CanIf, system matrix và node root đã có; runtime module, scheduler và board integration vẫn theo plan.
- Bộ plan không triển khai CanTp/Transfer/UART gateway/app vì ngoài Part 1. Wire profile chưa chốt giữa ba ECU; multi-controller và training Rx adapter là gap tầng dưới được lập plan riêng. Scheduler overrun policy trong plan là lựa chọn local, chưa có source scheduler.
- Little-endian, unsigned value types và Rx U=0 giữ giá trị cũ là lựa chọn local đề xuất trong plan, chưa được assignment chốt; cần thống nhất wire profile trước ghép ECU. CanTp/deadline monitoring/triggered Tx ngoài Part 1.

- Ba board S32K144 trên shared Classic CAN bus; PC1–UART–ECU1, PC2–UART–ECU3.
- README yêu cầu text/image hai chiều PC1 ↔ PC2, dữ liệu giống từng byte.
- ECU2 cùng bus, không cần relay frame ở tầng vật lý. Vai trò app ECU2 chưa chốt; diagnostic/heartbeat chỉ là đề xuất.
- Hai path dự kiến: control `COM → PduR → CanIf → CAN`; bulk `Transfer → PduR → CanTp → CanIf → CAN`. N-PDU callback đi trực tiếp `CanIf ↔ CanTp`, PduR nhận SDU callbacks.
- Local handle không được xem là field tự động tồn tại trên bus. Serialization/endian, CAN ID publisher/consumer, DLC, timing, version phải được thống nhất riêng.

## Hiện trạng có bằng chứng trong workspace

| Khu vực | Hiện trạng | Bằng chứng |
|---|---|---|
| Build/runtime | S32 Design Studio/Eclipse, NXP GCC Arm bare-metal, target S32K144; entrypoint hiện chạy một CAN0 internal-loopback test rồi giữ kết quả cho debugger | `.project`, `.cproject`, `src/main.c` |
| CAN tham khảo | `can_task` là bài riêng để tái sử dụng, không phải mock stack hoàn chỉnh | `requirements/README.md`, `can_task/driver/`, `can_task/upper/` |
| CAN Driver đang dùng | CAN0 polling, Classic CAN standard 11-bit, 0..8 byte, 8 MHz/500 kbit/s; lifecycle UNINIT/STOPPED/STARTED/FAULT; bounded hardware wait, software PDU handle, Tx/Rx/bus-off callbacks và counters | `drivers/can/driver/`, `drivers/can/config/can/` |
| CAN config | MB0 HTH Tx, MB1 HRH Rx exact `0x123`; export normal và internal-loopback config; config timing khác 500 kbit/s bị từ chối | `drivers/can/config/can/Can_Cfg.c` |
| CAN layout | Mọi phần CAN ngoài BSP nằm dưới `drivers/can`: `driver`, `config`, `comm`, `test`; board clock/pin/transceiver giữ ở `bsp/can` | cây thư mục `drivers/can/`, `bsp/can/` |
| CAN init guide | Thứ tự BSP → driver STOPPED → callback/upper init → STARTED → polling, gồm register/state/failure path hiện tại | `docs/can-init-sequence.md` |
| CAN legacy coupling | Bản tham khảo vẫn callback trực tiếp CanUpper, nhưng đã bị loại khỏi source set Debug_FLASH để không trùng symbol với driver mới | `can_task/`, `.cproject` |
| Upper layers | COM types/config + codec WIP; PduR/CanIf có types và Tx-only const config nhưng chưa có runtime implementation; CanTp ngoài Part 1 | `drivers/can/comm/`, `drivers/can/config/` |
| System profile | Global0x0010, CAN ID0x321, DLC8, COM I-PDU0 → PduR route11 → CanIf TxPdu7 → HTH0; period10/offset1/retries3 | `drivers/can/config/comm_matrix_cfg.h`, `node_cfg.*`, module config files |
| UART | LPUART1 byte callbacks, baud selection, bounded wait, stats; RX/TX IRQ, chưa có file framing/session | `drivers/uart/Driver_UART.c` |
| Queue/timebase | Ring storage N giữ N−1 byte; SysTick trả ticks, chưa mặc định ms nếu chưa init 1000 Hz | `middlewares/ring_buffer.c`, `drivers/systick/Driver_SysTick.c` |
| Tests | Host test deterministic đã pass cho validation, lifecycle, Tx/Rx, busy và bus-off. Board loopback harness đã ARM-compile nhưng chưa có raw log sau khi flash | `tests/host/can/`, `drivers/can/test/Can_LoopbackTest.c` |
| Build artifacts | Generated `Debug_FLASH` hiện có thể cũ; source CAN mới và board harness đã compile riêng bằng NXP ARM GCC 6.3, chưa link/flash toàn firmware trong CLI | `Debug_FLASH/`, validation 2026-09-13 |

## Quyết định tài liệu trong API_SPEC 0.2

- API_SPEC 0.2 giữ **DRAFT**, tách EXISTING/MODIFY/NEW khỏi trạng thái được kiểm chứng. Mô tả COM cũ cần đồng bộ theo assignment Part 1 khi thực thi giai đoạn 1; không dùng triggered Tx/deadline monitoring cũ làm yêu cầu Part 1.
- Đề xuất CAN polling, callback upper ở main; CanIf không có TX queue, một outstanding request/HTH và TxPduId, BUSY không nhận request.
- Driver lưu software PDU handle theo MB, copy payload trước return, terminal callback đúng một lần/request accepted; có policy stop/bus-off/timeout và stale events.
- Mode transition đồng bộ có poll bound, init kết thúc STOPPED và start riêng đã có trong driver mới. Deadline theo monotonic time/Tx timeout trong API_SPEC vẫn chưa triển khai.
- CAN TX complete chỉ là hoàn tất tầng CAN; không suy PC đích commit/CRC object từ ACK này.
- Bổ sung checklist config/message matrix và thiết kế LL-01..LL-13; host test mới phủ một phần validation/lifecycle/Tx/Rx/bus-off, chưa phủ đủ toàn bộ matrix.

## Chưa chốt / cần chú ý lượt sau

- Không có source/config firmware của hai người còn lại trong workspace để khẳng định tương thích thực tế.
- Chưa chốt CAN ID matrix chính thức, timing/service interval, role ECU2, transport profile (không tự nhận ISO-TP compliance), max object, slot count, retry, UART packet layout hoặc CRC profile.
- Transfer skeleton thiếu RX session discovery, initial receive capacity, metadata wire envelope, ACK end-to-end và stale session/reset policy.
- README muốn giữ object tới CRC pass, nhưng cũng đề xuất streaming RAM thấp. API_SPEC 0.2 ghi gate: chọn PC temporary/commit protocol hoặc giới hạn object theo storage ECU; chưa chọn thay nhóm.
- README nhắc `tasks/task5/app_cli.c`, nhưng thư mục `tasks/` không có trong workspace hiện tại; coi là tham chiếu lịch sử.
- Source entries Debug_FLASH gồm `drivers`/`middlewares`/`bsp` và không còn `can_task`; ba cấu hình còn lại trong `.cproject` chưa có cùng danh sách. Không suy một config build được thì các config khác cũng build được.
- Lúc đầu chưa có Git; trong lượt làm việc người dùng đã khởi tạo repository, commit quan sát được `dcb9c74` (`Init project`) và thêm `.gitignore`. Dùng Git diff từ đây; không sửa `.gitignore` của người dùng.

## Validation gần nhất

- Type/config conformance ngày 2026-09-15 compile C99 strict và pass: `Part 1 type/config tests passed`; kiểm root pointers, counts, Global ID, local routes, CAN ID/DLC/HTH và timing phase. Xem [validation report](implement/types-config-part1-validation.md).
- CAN Driver host regression chạy lại sau khi thêm `Can_RxPduType`: `CAN host tests passed`.
- ARM/full firmware/board chưa chạy cho type/config mới vì `arm-none-eabi-gcc` không có trong PATH shell hiện tại.
- Lượt lập bộ plan Part 1: đã kiểm tra local links, code fences và encoding trên 9 file Markdown (7 file trong implement và 2 core memory); số dòng inventory của 6 plan khớp số file công bố. Chỉ thay đổi tài liệu, không chạy lại firmware/host tests; các bằng chứng CAN bên dưới thuộc lượt implementation trước.

- Host compile/test bằng GCC với `-Wall -Wextra -Werror`: `CAN host tests passed`.
- `Can.c`, helper, config, board loopback harness, `board_can.c` và `src/main.c` compile sạch bằng NXP ARM GCC 6.3/Cortex-M4 với `-Wall -Wextra -Werror`.
- Chưa chạy S32DS full link hoặc flash board. `g_CanLoopbackTestResult == CAN_LOOPBACK_TEST_PASSED` và LED xanh sau khi flash mới là bằng chứng loopback phần cứng nội bộ.

## Bước tiếp theo được khuyến nghị

Hoàn thiện và test slot codec COM, sau đó viết Init/Send → scheduler/retry → Rx theo [plan COM](implement/com-part1-plan.md). PduR/CanIf đã có type/config để người dùng viết logic sau. Board integration chờ runtime các tầng, receiver profile, scheduler 1 ms và mapping được cả nhóm xác nhận.
