# Mock Multi-ECU Project trên S32K144

## 1. Mục đích tài liệu

Tài liệu này giải thích cách dùng bài tập CAN độc lập trong [`can_task/`](../can_task/) làm nguồn tham khảo cho mock project ba ECU trong thư mục này.

API contract sơ bộ cho từng module được mô tả tại [`API_SPEC.md`](API_SPEC.md).

Hai phần phải được hiểu là hai bài tập khác nhau:

- `can_task/` là một CAN stack nhỏ đã có source, cấu hình và test harness mẫu;
- `requirements/` mô tả mock project tổng quát sẽ tích hợp các driver của repository để tạo hệ thống ba ECU hoàn chỉnh.

Không xem `can_task` là implementation hiện tại của mock project. Source trong đó chỉ cung cấp các khối có thể tái sử dụng hoặc refactor.

Trạng thái tài liệu: thiết kế sơ bộ. Các mục được đánh dấu **Chưa chốt** phải được quyết định trước khi đóng băng protocol và viết application cuối cùng.

## 2. Đầu vào thiết kế hiện có

### 2.1 Sơ đồ mạng tổng quan

![Tổng quan mạng ba ECU](<Mock_MCU-Overview.png>)

Sơ đồ hiện tại xác định:

- ba ECU sử dụng board S32K144 cùng kết nối vào một Classic CAN bus;
- PC1 kết nối UART với ECU1;
- PC2 kết nối UART với ECU3;
- mỗi ECU có thể chứa nhiều application;
- CAN_H và CAN_L là đường truyền chung, vì vậy ECU1 có thể phát frame trực tiếp để ECU2 và ECU3 cùng quan sát. ECU2 không cần chuyển tiếp frame ở tầng vật lý.

### 2.2 Sơ đồ Tx/Rx dự kiến

![Luồng Tx và Rx dự kiến](<Mock_MCU-Tx-Rx Flow.png>)

Sơ đồ lấy cảm hứng từ AUTOSAR nhưng project không đặt mục tiêu sao chép đầy đủ AUTOSAR. Các ranh giới quan trọng cần giữ là:

```text
Application
    |
    v
COM hoặc Stream/Gateway service
    |
    v
PduR
    |--------------------|
    v                    v
Direct route           CanTp route
    |                    |
    +---------+----------+
              v
            CanIf
              |
              v
          CAN Driver
              |
              v
        S32K144 FlexCAN
```

Direct route dành cho PDU nhỏ vừa trong một Classic CAN frame. CanTp route dành cho text, ảnh hoặc dữ liệu khác vượt quá 8 byte.

## 3. Yêu cầu sơ bộ của mock project

### 3.1 Yêu cầu chức năng đã biết

1. Ba ECU phải truyền và nhận dữ liệu với nhau qua Classic CAN.
2. PC1 phải gửi được text hoặc ảnh qua ECU1, CAN bus và ECU3 đến PC2.
3. PC2 phải gửi ngược text hoặc ảnh qua ECU3, CAN bus và ECU1 đến PC1.
4. Dữ liệu nhận tại PC đích phải giống từng byte với dữ liệu PC nguồn đã gửi.

### 3.2 Ràng buộc kỹ thuật suy ra từ yêu cầu

- Classic CAN chỉ mang tối đa 8 byte data trong mỗi frame đối với implementation hiện tại, nên ảnh và phần lớn chuỗi text cần được chia frame rồi ghép lại.
- Luồng UART cho file phải binary-safe: không được xem `0x00` là kết thúc chuỗi, không đổi CR/LF và không dùng một byte dữ liệu hợp lệ làm marker nội bộ.
- Buffer hữu hạn có thể đầy. Mỗi tầng phải trả trạng thái `BUSY`, từ chối toàn bộ đơn vị dữ liệu hoặc áp dụng flow control; không được ghi đè âm thầm.
- Hai chiều truyền phải có CAN ID hoặc channel riêng đủ để phân biệt direction/source. Không để nhiều node đồng thời phát payload khác nhau bằng cùng một CAN ID.
- ECU nhận cần biết độ dài toàn bộ object, thứ tự segment và kết quả kiểm tra toàn vẹn trước khi chuyển object hoàn chỉnh ra UART.
- ISR chỉ nên di chuyển byte/frame và cập nhật trạng thái ngắn. Parsing, routing, ghép object và gửi UART dài phải chạy ngoài ISR.

## 4. `can_task` hiện có những gì?

### 4.1 Cấu trúc

```text
can_task/
|-- driver/
|   |-- inc/
|   |   |-- Can.h
|   |   |-- Can_Cfg.h
|   |   `-- Can_Types.h
|   `-- src/
|       |-- Can.c
|       `-- Can_Cfg.c
|-- upper/
|   |-- inc/
|   |   |-- CanUpper.h
|   |   |-- CanUpper_Cfg.h
|   |   `-- CanUpper_Types.h
|   `-- src/
|       `-- CanUpper.c
|-- test/
|   `-- main.c
`-- Can_Stack.md
```

### 4.2 CAN Driver

[`driver/src/Can.c`](../can_task/driver/src/Can.c) hiện thực các trách nhiệm phần cứng chính:

- bật clock FlexCAN0;
- vào và thoát Freeze Mode;
- cấu hình bit timing 500 kbit/s từ clock 8 MHz;
- cấu hình loopback;
- ánh xạ HOH tới FlexCAN Message Buffer;
- cấu hình RX CAN ID/mask;
- kiểm tra tham số trước khi phát;
- pack/unpack payload 0..8 byte;
- kiểm tra cờ TX/RX bằng polling;
- có ISR mẫu cho Message Buffer.

[`driver/src/Can_Cfg.c`](../can_task/driver/src/Can_Cfg.c) định nghĩa cấu hình hiện tại:

| Handle | Message Buffer | Hướng | CAN ID/filter |
|---|---:|---|---|
| `CAN_HTH_0` | MB0 | TX | CAN ID do upper layer cung cấp |
| `CAN_HRH_0` | MB1 | RX | Exact match `0x123` |

Phần này gần với **CAN Driver** trong sơ đồ mục tiêu. Việc HTH/HRH ánh xạ tới Message Buffer vẫn là trách nhiệm hợp lý của driver.

### 4.3 CanUpper

[`upper/src/CanUpper.c`](../can_task/upper/src/CanUpper.c) hiện thực:

- bảng `TxPduId -> CAN ID -> HTH`;
- bảng `RxPduId -> HRH`;
- trạng thái TX `IDLE/PENDING/DONE`;
- một buffer RX dài tối đa 8 byte;
- API application-facing để gửi PDU và lấy frame nhận;
- gọi hai main function polling của CAN Driver.

`CanUpper` không phải PduR thuần. Nó đang gộp ba nhóm trách nhiệm:

| Trách nhiệm trong `CanUpper` | Tầng gần nhất trong sơ đồ đích |
|---|---|
| Ánh xạ TxPduId sang CAN ID và HTH | CanIf |
| Ánh xạ HRH sang RxPduId | CanIf |
| Chuyển một PDU cố định giữa application và driver | PduR tối giản |
| Giữ TX status, RX data và `newDataFlag` | Application adapter/buffer |

Driver hiện gọi trực tiếp `CanUpper_TxConfirmation()` và `CanUpper_RxIndication()`. Điều này bỏ qua interface CanIf độc lập và làm driver phụ thuộc ngược vào upper layer.

### 4.4 Test harness và báo cáo

[`test/main.c`](../can_task/test/main.c) có các kịch bản:

- TC-001: init và kiểm tra tham số;
- TC-002: loopback TX -> RX;
- TC-003: truyền giữa hai board;
- TC-004: tham số không hợp lệ và biên payload 0/8 byte.

Toàn bộ file hiện nằm trong `#if 0`, nên test không chạy trong firmware hiện tại. [`Can_Stack.md`](../can_task/Can_Stack.md) ghi nhận kết quả lịch sử nhưng repository chưa có raw board log tương ứng. Vì vậy test source có thể tái sử dụng làm test design, còn trạng thái PASS phải được đo lại.

## 5. Luồng thật sự của `can_task`

### 5.1 Tx

```text
Application
  -> CanUpper_Transmit(TxPduId, data, length)
  -> tra TxPduId để lấy CAN ID và HTH
  -> Can_Write(HTH, Can_PduType)
  -> ghi FlexCAN TX Message Buffer
  -> CAN bus
```

Khi hoàn tất:

```text
FlexCAN IFLAG
  -> Can_MainFunction_Write() hoặc ISR
  -> CanUpper_TxConfirmation(HTH)
  -> TX status = DONE
```

### 5.2 Rx

```text
CAN bus
  -> FlexCAN RX Message Buffer
  -> Can_MainFunction_Read() hoặc ISR
  -> đọc HRH + CAN ID + DLC + payload
  -> CanUpper_RxIndication()
  -> copy vào buffer RX một frame
  -> CanUpper_GetRxData(RxPduId)
  -> Application
```

Luồng này đủ để minh họa một frame LED command, nhưng chưa có COM, PduR tổng quát, CanTp hoặc UART gateway.

## 6. Kế hoạch tái sử dụng

### 6.1 Ma trận quyết định

| Thành phần | Quyết định | Nội dung tái sử dụng | Việc bắt buộc trước khi tích hợp |
|---|---|---|---|
| `Can_Types.h` | Refactor | Khái niệm HOH, HTH, HRH, CAN PDU | Bổ sung software PDU handle/controller ID và type độ dài thống nhất |
| `Can_Cfg.*` | Refactor | Bảng HOH và cấu hình normal/loopback | Tạo nhiều TX/RX object, CAN ID và filter cho ba ECU |
| `Can_Init()` | Refactor | Trình tự Freeze Mode và setup Message Buffer | Dùng `baudrate`, thêm timeout, validate toàn bộ config và trả lỗi init |
| `Can_Write()` | Tái sử dụng có chỉnh sửa | Validation, pack payload, busy check | Trả confirmation theo software PDU handle thay vì suy từ HTH |
| CAN polling | Tái sử dụng ở milestone đầu | Đường chạy đơn giản, dễ debug | Quy định chu kỳ gọi và timeout |
| CAN ISR mẫu | Hoàn thiện sau | Logic đọc cờ và Message Buffer | Cấu hình NVIC, chọn polling hoặc interrupt rõ ràng, không xử lý trùng |
| `CanUpper` mapping | Tách module | Ý tưởng bảng PDU/CAN ID/HOH | Chuyển mapping phần cứng vào `CanIf`; chuyển route sang `PduR` |
| `CanUpper` RX buffer | Không dùng cho file | Có thể dùng cho demo một frame | Thay bằng queue và transport reassembly có báo overflow |
| `CanUpper` TX status | Refactor | Ý tưởng state/confirmation | Theo dõi từng TxPdu/transfer, hỗ trợ nhiều PDU chung HTH |
| TC-001..TC-004 | Tái sử dụng làm baseline | Init, loopback, physical bus, invalid input | Tách khỏi `#if 0`, tạo log và chạy lại trên board |

### 6.2 Phần có thể giữ gần nguyên dạng

- Tên API mức driver `Can_Init()`, `Can_Write()`, `Can_MainFunction_Read()` và `Can_MainFunction_Write()`.
- Kiểm tra null pointer, CAN ID 11 bit, DLC và Message Buffer busy.
- Cách encode/decode standard CAN ID và payload trong FlexCAN Message Buffer.
- Cấu hình exact-match filter làm baseline.
- Loopback config dùng cho smoke test trước khi nối ba board.

### 6.3 Phần không nên copy nguyên trạng

- Dependency `Can.c -> CanUpper.h`.
- Callback TX bằng HTH. Nhiều PDU có thể dùng chung HTH nên HTH không đủ để nhận diện PDU đã phát.
- Một TX PDU, một RX PDU và duy nhất CAN ID `0x123`.
- RX buffer một frame; frame mới có thể ghi đè frame chưa được application đọc.
- Bit timing hardcode dù config có field `baudrate`.
- Vòng chờ phần cứng không có timeout.
- Interrupt mode chưa cấu hình NVIC đầy đủ.
- Board clock, CAN pin mux và transceiver control nằm trong test application thay vì BSP/init flow rõ ràng.

## 7. Kiến trúc đề xuất cho mock project

### 7.1 Hai data path

Không nên ép mọi dữ liệu đi qua cùng một kiểu COM signal.

**Control path — một frame:**

```text
Application signal/state
  -> COM pack I-PDU
  -> PduR direct route
  -> CanIf
  -> CAN Driver
```

Ví dụ: heartbeat, trạng thái node, LED command, lỗi hoặc thống kê.

**Bulk path — nhiều frame:**

```text
UART byte stream / file gateway
  -> Transfer Service
  -> PduR TP route
  -> CanTp
  -> CanIf
  -> CAN Driver
```

Ở ECU nhận, luồng chạy ngược và Transfer Service chỉ chuyển dữ liệu ra UART sau khi kiểm tra length, sequence và integrity.

### 7.2 Trách nhiệm module

| Module | Chỉ được biết | Không được biết |
|---|---|---|
| Application | Signal hoặc object cần gửi/nhận | Message Buffer, HTH/HRH, register |
| UART Gateway | Byte stream, transfer metadata, UART queue | FlexCAN register |
| COM | Signal-to-I-PDU mapping cho control path | HTH/HRH, segmentation |
| PduR | Source PDU, destination module/PDU, direct hay TP route | Ý nghĩa từng signal, FlexCAN register |
| CanTp | Segment/reassembly, sequence, flow control, timeout | UART register, application semantics |
| CanIf | PDU-to-CAN-ID/HTH/HRH/controller mapping | Nội dung ảnh/text |
| CAN Driver | Controller, HOH, Message Buffer, register, event | PDU route hoặc application semantics |
| UART Driver | Baud, RX/TX byte, IRQ và hardware error | Text command hoặc image format |

### 7.3 Folder structure dự kiến

Đây là cấu trúc đề xuất, chưa phải yêu cầu tạo source ngay:

```text
mock_project/
|-- app/
|   |-- gateway_app.c/.h
|   `-- node_app.c/.h
|-- communication/
|   |-- com/
|   |-- pdur/
|   |-- cantp/
|   `-- canif/
|-- config/
|   |-- node_cfg.h
|   |-- com_cfg.c/.h
|   |-- pdur_cfg.c/.h
|   |-- cantp_cfg.c/.h
|   `-- canif_cfg.c/.h
|-- bsp/
|   `-- board_can.c/.h
`-- tests/
    |-- host/
    `-- board/
```

CAN Driver và UART Driver tiếp tục nằm trong vùng driver dùng chung của repository thay vì bị copy vào application.

### 7.4 Vai trò từng ECU

| ECU | Vai trò tối thiểu |
|---|---|
| ECU1 | UART gateway cho PC1; CAN endpoint; gửi và nhận transfer |
| ECU2 | CAN participant; heartbeat/diagnostic hoặc một application độc lập |
| ECU3 | UART gateway cho PC2; CAN endpoint; gửi và nhận transfer |

Vì ba ECU cùng nằm trên một bus, ECU2 không nên nhận rồi phát lại mọi frame của ECU1. Làm như vậy chỉ tăng bus load và tạo nguy cơ duplicate. Vai trò nghiệp vụ cụ thể của ECU2 vẫn là **Chưa chốt**.

### 7.5 Tái sử dụng các driver ngoài `can_task`

- [`Driver_UART`](../drivers/uart/Driver_UART.h): dùng làm lớp byte RX/TX cho ECU1 và ECU3.
- [`ring_buffer`](../middlewares/ring_buffer.h): dùng làm queue byte hoặc queue descriptor; phải trả lỗi khi đầy.
- `Driver_NVIC`: cấu hình interrupt cho UART và CAN khi chuyển sang interrupt mode.
- `Driver_SysTick` hoặc LPIT: cung cấp tick cho timeout, retry, separation time và heartbeat.
- GPIO/LED BSP: hiển thị trạng thái node, transfer active hoặc error trong giai đoạn debug.

Không tái sử dụng nguyên `tasks/task5/app_cli.c` cho ảnh. CLI hiện xử lý theo line và dùng logic dành cho ASCII; bulk path phải bảo toàn mọi giá trị byte.

## 8. Yêu cầu tối thiểu cho transport dữ liệu

CanTp hoặc transport tự thiết kế phải có contract rõ ràng cho:

- transfer/session ID;
- loại object: text, image hoặc raw binary;
- tổng số byte;
- thứ tự segment;
- phát hiện segment trùng, thiếu hoặc sai thứ tự;
- timeout khi transfer dừng giữa chừng;
- flow control/backpressure;
- hủy transfer và giải phóng buffer;
- kiểm tra toàn vẹn toàn object, ví dụ CRC đặt trong metadata;
- thông báo success/failure cho UART gateway;
- truyền hai chiều độc lập.

Nên stream dữ liệu qua các block nhỏ thay vì giữ toàn bộ ảnh trong RAM. Kích thước queue và block chỉ được chốt sau khi đo RAM còn lại và tốc độ UART/CAN thực tế.

## 9. CAN ID và PDU configuration

Không dùng duy nhất ID `0x123` cho toàn bộ hệ thống. Cần tạo configuration table, ít nhất phân biệt:

- control/heartbeat của từng ECU;
- data channel ECU1 -> ECU3;
- data channel ECU3 -> ECU1;
- flow-control/confirmation theo từng direction nếu transport cần;
- diagnostic/error reporting.

CAN ID biểu diễn loại message và priority trên bus, không phải địa chỉ point-to-point tuyệt đối. Mỗi ECU chỉ giao PDU lên trên khi CanIf configuration xác định node đó là consumer.

Ví dụ range để thảo luận, **không phải protocol đã chốt**:

| Range | Mục đích dự kiến |
|---|---|
| `0x100..0x10F` | Heartbeat/network status |
| `0x200..0x20F` | Control PDU nhỏ |
| `0x300..0x30F` | Transport channel ECU1 -> ECU3 |
| `0x310..0x31F` | Transport channel ECU3 -> ECU1 |

Mỗi CAN ID, TxPduId, RxPduId, HTH, HRH và Message Buffer phải có đúng một entry cấu hình có thể truy vết.

## 10. Lộ trình phát triển

### Milestone 0 — Đóng băng yêu cầu đo được

- Chốt vai trò ECU2.
- Chốt kích thước file tối đa hoặc xác nhận thiết kế streaming không giới hạn theo object buffer.
- Chốt baud UART, CAN bitrate và traffic nền.
- Chốt transport profile, timeout, retry và tiêu chí integrity.
- Chốt hành vi khi ECU reset, PC ngắt UART hoặc CAN bus-off.

Kết quả: requirements và acceptance criteria được đánh version.

### Milestone 1 — Làm sạch CAN Driver từ `can_task`

- Tách callback của driver khỏi `CanUpper`.
- Validate config đầy đủ và dùng `Config->baudrate`.
- Thêm timeout cho Freeze/ready wait.
- Đưa pin mux, clock nguồn và transceiver enable vào BSP init rõ ràng.
- Chọn polling làm baseline; chỉ bật interrupt sau khi polling test pass.
- Chạy lại TC-001, TC-002 và TC-004, lưu raw log.

Kết quả: một board truyền loopback ổn định với API driver độc lập.

### Milestone 2 — Tạo CanIf

- Chuyển `TxPduId -> CAN ID/HTH` và `HRH/CAN ID -> RxPduId` từ `CanUpper` sang CanIf.
- Thêm software PDU handle vào TX confirmation path.
- Hỗ trợ nhiều PDU dùng chung HTH.
- Tạo config riêng theo node thay vì hardcode application trong source.

Kết quả: application gửi PDU ID mà không biết HTH, HRH hoặc Message Buffer.

### Milestone 3 — PduR direct route và giao tiếp ba ECU

- Tạo routing table source/destination PDU.
- Thêm heartbeat hoặc diagnostic PDU cho từng ECU.
- Chạy physical CAN test giữa cả ba board.
- Xác minh không node nào phát lại frame chỉ vì cùng nhìn thấy frame trên bus.

Kết quả: mỗi ECU gửi và nhận ít nhất một PDU có thể phân biệt bằng log.

### Milestone 4 — CanTp

- Hiện thực segment/reassembly và flow control theo profile đã chốt.
- Unit test các biên 0, 1, 7, 8, 9 byte và nhiều frame.
- Fault-injection test cho frame thiếu, trùng, sai sequence và timeout.
- Test hai transfer ngược chiều theo policy đã chọn.

Kết quả: truyền buffer nhiều frame qua CAN loopback và giữa hai board, byte-for-byte đúng.

### Milestone 5 — UART Gateway

- Dùng UART callback và ring buffer ở dạng binary-safe.
- Định nghĩa framing giữa PC và ECU.
- Nối UART input với Transfer Service, không nối trực tiếp ISR vào `Can_Write()`.
- Nối object hoàn chỉnh từ Transfer Service sang UART TX queue.
- Báo rõ BUSY, timeout, CRC error và aborted transfer.

Kết quả: truyền raw binary PC -> ECU -> CAN -> ECU -> PC.

### Milestone 6 — End-to-end và robustness

- Chạy PC1 -> PC2 và PC2 -> PC1.
- So sánh hash và kích thước file nguồn/đích trên PC.
- Test text UTF-8, binary chứa `0x00`/`0xFF`, ảnh nhỏ/lớn và nhiều lần liên tiếp.
- Test reset một ECU, CAN disconnect, UART disconnect, queue full và bus-off.
- Đo throughput, latency, RAM high-water mark, số retry và error counter.

Kết quả: có test report và raw log đủ tái lập.

## 11. Acceptance criteria sơ bộ

| ID | Điều kiện pass |
|---|---|
| AC-CAN-01 | ECU1, ECU2 và ECU3 đều phát được một PDU riêng và hai ECU còn lại nhận đúng PDU theo configuration |
| AC-CAN-02 | CAN ID, DLC và payload 0..8 byte qua driver đúng với dữ liệu nguồn |
| AC-TP-01 | Payload lớn hơn 8 byte được ghép đúng khi mọi frame hợp lệ |
| AC-TP-02 | Thiếu/sai/trùng segment không tạo ra object được báo thành công |
| AC-TP-03 | Transfer timeout giải phóng state và cho phép transfer mới |
| AC-UART-01 | Chuỗi byte chứa `0x00`, `0x0A`, `0x0D` và `0xFF` được bảo toàn |
| AC-E2E-01 | File PC2 nhận có cùng byte length và hash với file PC1 gửi |
| AC-E2E-02 | File PC1 nhận có cùng byte length và hash với file PC2 gửi |
| AC-LOAD-01 | Khi queue đầy, hệ thống báo lỗi/BUSY xác định; không ghi đè dữ liệu chưa xử lý |
| AC-RESET-01 | Sau reset hoặc transfer bị ngắt, node trở về trạng thái cho phép bắt đầu transfer mới |

Các giới hạn kích thước, timeout, tốc độ và số transfer đồng thời phải được bổ sung bằng giá trị cụ thể trước khi các acceptance criteria này trở thành test case chính thức.

## 12. Các quyết định chưa chốt

### ISSUE 1: Mức độ mô phỏng AUTOSAR

1. **Option A — Module tách rõ, API tối giản:** giữ COM/PduR/CanTp/CanIf/CAN Driver thành các boundary riêng nhưng chỉ hiện thực API cần cho bài. Pro: dễ học, dễ test, phù hợp sơ đồ. Con: nhiều file hơn.
2. **Option B — Gộp upper stack:** gộp COM/PduR/CanTp/CanIf trong một module. Pro: code ban đầu ngắn. Con: khó thay đổi protocol và dễ lặp lại vấn đề của `CanUpper`.
3. **Agent Rec:** Option A, vì project tham khảo AUTOSAR nhưng không cần mang toàn bộ độ phức tạp cấu hình/code generation của AUTOSAR.

### ISSUE 2: Transport nhiều frame

1. **Option A — CanTp-like profile tối giản:** định nghĩa Single/First/Consecutive/Flow-Control frame, timeout và sequence. Pro: khớp sơ đồ, kiểm soát được scope. Con: phải viết và test state machine.
2. **Option B — Protocol segment riêng rất nhỏ:** header project-specific trên mỗi frame. Pro: nhanh làm demo. Con: dễ thiếu flow control, timeout hoặc recovery.
3. **Agent Rec:** Option A; ghi rõ phần nào lấy cảm hứng từ CanTp và phần nào là extension của project.

### ISSUE 3: Vai trò ECU2

1. **Option A — Participant/diagnostic node:** ECU2 có heartbeat, command và telemetry riêng nhưng không relay bulk data. Pro: chứng minh đủ ba ECU giao tiếp mà không tăng traffic vô ích. Con: cần định nghĩa một application nhỏ cho ECU2.
2. **Option B — Application endpoint thứ ba:** ECU2 cũng gửi/nhận object nghiệp vụ. Pro: bài toán multi-node đầy đủ hơn. Con: protocol và test phức tạp hơn.
3. **Agent Rec:** Option A cho milestone đầu, sau đó nâng lên Option B nếu đề bài chính thức yêu cầu.

### ISSUE 4: Framing PC–ECU qua UART

1. **Option A — Binary packet có magic/version/type/length/payload/CRC:** bảo toàn text và ảnh bằng cùng một protocol. Pro: binary-safe và dễ tự động test. Con: cần PC sender/receiver tool.
2. **Option B — Line command cho text, mode riêng cho file:** dễ thao tác terminal. Pro: demo text nhanh. Con: hai parser và dễ sai khi chuyển mode.
3. **Agent Rec:** Option A cho data path; nếu cần CLI debug thì dùng CAN ID/PDU hoặc UART channel riêng, không trộn CLI text vào file stream.

## 13. Quy tắc bằng chứng

- Build thành công chỉ chứng minh source compile/link, không chứng minh ba board giao tiếp.
- Loopback chỉ chứng minh một controller tự TX/RX, không thay thế physical-bus test.
- LED có thể dùng làm dấu hiệu debug nhưng không chứng minh payload dài đúng.
- Mọi tuyên bố “file truyền đúng” phải có byte length và hash nguồn/đích.
- Mọi test hardware phải lưu firmware revision, cấu hình node, bitrate, wiring, test input, raw output và kết quả.

## 14. Bước tiếp theo gần nhất

1. Chốt bốn decision gate trong mục 12.
2. Viết interface contract cho CAN Driver callback và CanIf trước khi copy source.
3. Tạo PDU/CAN-ID matrix cho ba ECU.
4. Refactor `can_task` thành CAN Driver độc lập và chạy lại loopback có log.
5. Chỉ bắt đầu CanTp sau khi single-frame communication giữa ba board đã pass.
