# API Specification cho Mock Multi-ECU Project

## 1. Document control

| Thuộc tính | Giá trị |
|---|---|
| Tên tài liệu | Mock Multi-ECU Module API Specification |
| Trạng thái | **DRAFT — chưa dùng làm contract implementation chính thức** |
| Phiên bản | 0.2 — review tầng thấp, 2026-09-13 |
| Nguồn yêu cầu | [`requirements/README.md`](README.md) |
| Phạm vi | ECU1, ECU2, ECU3; Classic CAN; UART gateway tại ECU1 và ECU3 |

Tài liệu này liệt kê API dự kiến cho từng module và giải thích lý do cần từng API. Đây là thiết kế tối giản lấy cảm hứng từ AUTOSAR, không phải bản sao API AUTOSAR đầy đủ.

Ba ECU do **ba người phát triển độc lập**. API C trong tài liệu là đề xuất cho workspace này; không yêu cầu firmware khác có cùng source, folder, tên hàm, PDU ID hoặc HTH/HRH. Điểm tích hợp bắt buộc là **contract trên đường truyền** và hành vi có thể kiểm thử (mục 16). Hai sơ đồ [overview](Mock_MCU-Overview.png) và [Tx/Rx](<Mock_MCU-Tx-Rx Flow.png>) lần lượt là mô hình hoàn thiện và luồng tạm thời, không phải bằng chứng stack đã được hiện thực.

**Ưu tiên hiện tại:** BSP/timebase, CAN Driver, CanIf, PduR direct và kiểm thử single-frame. Các mục application, COM nghiệp vụ, CanTp, Transfer Service và UART framing là **DEFERRED / DRAFT**; chỉ triển khai sau khi các contract tương ứng được chốt. Có thể dùng test stub làm upper consumer ngay từ đầu, không cần hoàn thiện app để kiểm thử tầng thấp.

### 1.1 Hiện trạng đã đối chiếu với source

| Bằng chứng | Kết luận cho spec |
|---|---|
| [`src/main.c`](../src/main.c) | Entrypoint hiện tại là demo counter/SVC, chưa khởi tạo communication stack. |
| [`Can.c`](../can_task/driver/src/Can.c), [`Can_Cfg.c`](../can_task/driver/src/Can_Cfg.c) | Driver tham khảo có một TX MB, một RX MB filter `0x123`; init trả `void`, bit timing cố định và callback trực tiếp tới CanUpper. |
| [`CanUpper.c`](../can_task/upper/src/CanUpper.c) | Một Tx PDU, một Rx PDU, confirmation bằng HTH, RX buffer một frame. Chưa có CanIf/PduR độc lập. |
| [`can_task/test/main.c`](../can_task/test/main.c) | TC-001..004 bị bao bởi `#if 0`; báo cáo lịch sử không thay thế raw log chạy lại. |
| [`Driver_UART.c`](../drivers/uart/Driver_UART.c), [`ring_buffer.c`](../middlewares/ring_buffer.c) | Có callback byte RX/TX và queue byte; chưa có gateway packet/session implementation. |

`EXISTING` chỉ xác nhận sự tồn tại của API; không có nghĩa contract mới đã pass test. Khi README/sơ đồ cũ khác chi tiết contract ở bản 0.2, dùng bản này cho đề xuất API trong workspace; yêu cầu nghiệp vụ chưa chốt vẫn phải được thống nhất riêng.

Các tag sử dụng trong tài liệu:

- **EXISTING**: API đã tồn tại trong repository và có thể giữ tên/signature.
- **MODIFY**: API đã tồn tại nhưng signature hoặc contract phải thay đổi.
- **NEW**: API cần tạo cho mock project.
- **OPTIONAL**: chỉ cần khi feature liên quan được chọn.

## 2. Phạm vi và nguyên tắc contract

### 2.1 Trong phạm vi

- Application lifecycle cho gateway node và ECU2.
- Control PDU một frame qua COM -> PduR -> CanIf -> CAN Driver.
- Bulk data nhiều frame qua Transfer Service -> PduR -> CanTp -> CanIf -> CAN Driver.
- UART framing binary-safe tại ECU1 và ECU3.
- Buffering, timeout, cancellation, status và statistics cần để chứng minh truyền đúng.
- Callback giữa các tầng theo cả chiều Tx và Rx.

### 2.2 Ngoài phạm vi phiên bản 0.2

- API AUTOSAR chuẩn đầy đủ hoặc code generation AUTOSAR.
- Diagnostic protocol chuẩn hóa.
- Dynamic memory allocation.
- CAN FD.
- API phía PC.
- Security/authentication cho file transfer; chỉ kiểm tra integrity.

### 2.3 Quy tắc bắt buộc

- API public **MUST** kiểm tra null pointer, ID, length và trạng thái module.
- API truyền dữ liệu **MUST** trả `BUSY` hoặc lỗi rõ ràng khi không còn capacity; không được ghi đè âm thầm.
- API gọi từ main context **MUST NOT** busy-wait vô hạn.
- ISR **MUST NOT** parse UART packet, route PDU hoặc ghép toàn bộ transfer.
- Pointer payload input chỉ được mượn trong thời gian lời gọi; callee muốn dùng sau khi return phải copy trước khi return. Không chuyển quyền sở hữu bằng suy đoán. Riêng `const` configuration và storage cấp cho ring buffer phải sống suốt thời gian module sử dụng.
- Module dưới không được include header application. Callback lên trên phải đi qua interface được định nghĩa tại đây.
- Milestone đầu dùng CAN polling. Khi chuyển sang interrupt, driver phải queue event để upper-layer callback vẫn chạy ở main context.

### 2.4 Ngữ nghĩa dùng chung

- Public API của communication stack chạy tuần tự trong main context, không reentrant. Callback không được gọi lồng lại API transmit/init/mode của cùng module; ghi công việc chờ để xử lý ở vòng main kế tiếp. UART byte callbacks là ngoại lệ ISR đã ghi ở mục 11–13.
- `COMM_OK` của transmit nghĩa là **đã nhận trách nhiệm xử lý**. Sau đó phải có đúng một terminal callback cho request được nhận (trừ mất nguồn/reset CPU). Return lỗi/BUSY nghĩa là không nhận request, không callback về sau và caller vẫn sở hữu dữ liệu.
- `COMM_BUSY` là thiếu tài nguyên tạm thời; caller retry ở lần chạy main tiếp theo trong deadline. Không spin. `COMM_OVERFLOW` là dữ liệu không thể chứa theo giới hạn cấu hình hoặc đã mất dữ liệu; không giả vờ thành công.
- Direct PDU/frame là all-or-nothing. Chỉ API ghi rõ partial acceptance mới được nhận một phần.
- `PduInfoType *` phải khác NULL. `data == NULL` chỉ hợp lệ nếu `length == 0`; driver/CanIf hỗ trợ frame rỗng, upper route có thể từ chối theo cấu hình. ID/DLC không hợp lệ phải bị từ chối, không mask/truncate để biến thành hợp lệ.
- Output pointer hợp lệ phải được kiểm tra trước khi ghi. API có length/count output đặt output đó về 0 khi thất bại; buffer payload giữ nguyên, trừ partial acceptance được ghi rõ ở mục 10.4. Getter status giữ output nguyên khi thất bại. Callback `void` gặp input/state lỗi phải bỏ event và tăng counter có nguyên nhân.
- Init thành công mới publish trạng thái initialized. Init lặp khi đang initialized bị từ chối, không xóa request đang chạy. Init thất bại giữ module không nhận traffic và cho phép retry sau khi phần cứng đã được đưa về trạng thái xác định.

## 3. Common communication types

Các type này nên đặt trong header dùng chung hiện có tại `drivers/can/comm/common/comm_types.h`.

```c
typedef uint16_t PduIdType;
typedef uint16_t Com_SignalIdType;
typedef uint16_t Can_HwHandleType;
typedef uint32_t Can_IdType;
typedef uint32_t TransferIdType;
typedef uint32_t TransferLengthType;

typedef enum
{
    COMM_OK = 0,
    COMM_NOT_OK,
    COMM_BUSY,
    COMM_TIMEOUT,
    COMM_OVERFLOW,
    COMM_INVALID_PARAM,
    COMM_NOT_INITIALIZED,
    COMM_NOT_FOUND,
    COMM_CANCELLED,
    COMM_BUS_OFF,
    COMM_INVALID_STATE
} Comm_ReturnType;

typedef struct
{
    const uint8_t *data;
    uint16_t length;
} PduInfoType;

typedef struct
{
    uint8_t *data;
    uint16_t capacity;
    uint16_t length;
} MutableBufferType;
```

| Type | Vì sao cần |
|---|---|
| `PduIdType` | Cho phép PduR và CanIf route bằng logical ID thay vì để upper layer biết CAN ID/HTH. |
| `Can_HwHandleType` | Tách hardware object logic khỏi Message Buffer vật lý. |
| `TransferIdType` | Phân biệt transfer hiện tại với transfer cũ, đặc biệt sau timeout/reset. |
| `TransferLengthType` | Object UART có thể dài hơn giới hạn 8 byte hoặc giới hạn `uint16_t`. |
| `Comm_ReturnType` | Mọi caller phân biệt được invalid input, busy, timeout và overflow. |
| `PduInfoType` | Truyền pointer + length cùng nhau; không dùng null-terminated string cho binary data. |
| `MutableBufferType` | API copy dữ liệu biết capacity thực và không ghi vượt buffer đích. |

Không dùng trực tiếp `Can_ReturnType` làm return type cho mọi module vì lỗi CAN hardware và lỗi transport/application không cùng một miền trách nhiệm.

`PduInfoType.length`/`MutableBufferType.capacity` giới hạn **một chunk** tới `UINT16_MAX`, không phải độ dài object. `TransferLengthType` là số byte toàn SDU/object; phải kiểm tra giới hạn cấu hình và tràn số trước khi cộng metadata/chunk. `MutableBufferType.length` là output, luôn `<= capacity`. Các type trên là representation trong RAM, **không serialize bằng memcpy struct**.

## 4. Application modules

### 4.1 GatewayApp — ECU1 và ECU3

GatewayApp điều phối UART Gateway và communication stack; nó không parse byte UART và không truy cập FlexCAN register.

```c
Comm_ReturnType GatewayApp_Init(const GatewayApp_ConfigType *config);
void GatewayApp_MainFunction(uint32_t nowMs);
Comm_ReturnType GatewayApp_GetStatus(GatewayApp_StatusType *statusOut);
```

| API | Caller | Vì sao cần |
|---|---|---|
| `GatewayApp_Init()` **NEW** | `main()` | Init đúng thứ tự BSP, timebase, CAN Driver, CanIf, PduR, CanTp, Transfer Service, UART Driver và UART Gateway; trả lỗi nếu một tầng init thất bại. |
| `GatewayApp_MainFunction()` **NEW** | Super-loop | Cung cấp một điểm điều phối non-blocking cho timeout, CAN polling, transport và UART processing. |
| `GatewayApp_GetStatus()` **NEW** | Debug/status application | Xuất trạng thái node mà không cho debug code đọc global nội bộ của từng module. |

### 4.2 NodeApp — ECU2

ECU2 là CAN participant; diagnostic/heartbeat chỉ là gợi ý test, vai trò nghiệp vụ chưa chốt. Shared bus không đòi ECU2 relay bulk frame.

```c
Comm_ReturnType NodeApp_Init(const NodeApp_ConfigType *config);
void NodeApp_MainFunction(uint32_t nowMs);
void NodeApp_OnControlUpdate(Com_SignalIdType signalId);
```

| API | Caller | Vì sao cần |
|---|---|---|
| `NodeApp_Init()` **NEW** | `main()` ECU2 | Khởi tạo node ID, communication stack và application state riêng của ECU2. |
| `NodeApp_MainFunction()` **NEW** | Super-loop | Phát heartbeat/telemetry định kỳ và xử lý control PDU mà không chặn CAN polling. |
| `NodeApp_OnControlUpdate()` **NEW** | COM notification | Báo chính xác signal nào mới thay đổi; ECU2 không phải polling toàn bộ signal table ở mỗi vòng lặp. |

Nếu vai trò ECU2 thay đổi, API nghiệp vụ có thể mở rộng nhưng ECU2 vẫn không cần API relay frame trên shared CAN bus.

## 5. COM module — control path

COM chỉ dùng cho signal/control PDU nhỏ như heartbeat, status hoặc LED command. Text/image không đi qua `Com_SendSignal()`.

```c
Comm_ReturnType Com_Init(const Com_ConfigType *config);
Comm_ReturnType Com_SendSignal(Com_SignalIdType signalId,
                               const void *signalData);
Comm_ReturnType Com_ReceiveSignal(Com_SignalIdType signalId,
                                  void *signalDataOut);
void Com_MainFunctionTx(uint32_t nowMs);
void Com_MainFunctionRx(uint32_t nowMs);

void Com_RxIndication(PduIdType rxIPduId,
                      const PduInfoType *pduInfo);
void Com_TxConfirmation(PduIdType txIPduId,
                        Comm_ReturnType result);
```

| API | Caller | Vì sao cần |
|---|---|---|
| `Com_Init()` **NEW** | App init | Load signal-to-I-PDU mapping, initial value, update flag và timeout state từ config. |
| `Com_SendSignal()` **NEW** | Application | Application cập nhật logical signal mà không biết byte offset, CAN ID hoặc HTH. COM pack signal vào I-PDU theo config. |
| `Com_ReceiveSignal()` **NEW** | Application | Đọc signal đã unpack mà không cho application đọc raw CAN payload. |
| `Com_MainFunctionTx()` **NEW** | Super-loop | Phát periodic/triggered I-PDU theo schedule; cần cho heartbeat và tránh gửi trực tiếp trong application callback. |
| `Com_MainFunctionRx()` **OPTIONAL** | Super-loop | Kiểm tra receive timeout/freshness nếu requirement yêu cầu phát hiện ECU im lặng. |
| `Com_RxIndication()` **NEW** | PduR | Nhận I-PDU đã route, validate length, unpack signal và phát application notification. |
| `Com_TxConfirmation()` **NEW** | PduR | Hoàn tất trạng thái transmission/notification của I-PDU; không suy đoán từ thời gian gửi. |

Trước khi freeze COM, cần khai báo kiểu/kích thước signal theo config hoặc thêm length/capacity vào Send/ReceiveSignal: chỉ `void *` không cho phép kiểm tra kích thước buffer của caller. Signal bit layout/endian phải xuất hiện trong message matrix trước khi tích hợp ECU khác.

## 6. PduR module — route ownership

PduR chỉ quyết định source PDU đi tới destination module/PDU nào. PduR không biết CAN ID, HTH, UART packet format hoặc ý nghĩa signal.

```c
Comm_ReturnType PduR_Init(const PduR_ConfigType *config);

Comm_ReturnType PduR_ComTransmit(PduIdType sourcePduId,
                                 const PduInfoType *pduInfo);
Comm_ReturnType PduR_TransferTransmit(PduIdType sourceSduId,
                                      TransferLengthType totalLength);
Comm_ReturnType PduR_TransferCancelTransmit(PduIdType sourceSduId);
Comm_ReturnType PduR_TransferCancelReceive(PduIdType sourceSduId);

void PduR_CanIfRxIndication(PduIdType rxPduId,
                            const PduInfoType *pduInfo);
void PduR_CanIfTxConfirmation(PduIdType txPduId,
                              Comm_ReturnType result);

Comm_ReturnType PduR_CanTpStartOfReception(PduIdType rxSduId,
                                           TransferLengthType totalLength);
Comm_ReturnType PduR_CanTpCopyRxData(PduIdType rxSduId,
                                     const PduInfoType *segment,
                                     TransferLengthType *remainingCapacityOut);
void PduR_CanTpRxIndication(PduIdType rxSduId,
                            Comm_ReturnType result);

Comm_ReturnType PduR_CanTpCopyTxData(PduIdType txSduId,
                                     MutableBufferType *segmentOut,
                                     TransferLengthType *remainingDataOut);
void PduR_CanTpTxConfirmation(PduIdType txSduId,
                              Comm_ReturnType result);
```

| API | Caller | Vì sao cần |
|---|---|---|
| `PduR_Init()` **NEW** | App init | Validate và giữ routing table; fail sớm nếu PDU ID trùng hoặc route thiếu destination. |
| `PduR_ComTransmit()` **NEW** | COM | Thực hiện direct route từ COM I-PDU tới CanIf mà COM không biết CAN channel. |
| `PduR_TransferTransmit()` **NEW** | Transfer Service | Chọn TP route và bắt đầu CanTp SDU bằng tổng độ dài object. |
| `PduR_TransferCancelTransmit()` **NEW** | Transfer Service | Route yêu cầu hủy Tx tới đúng CanTp SDU mà Transfer Service không gọi CanTp trực tiếp. |
| `PduR_TransferCancelReceive()` **NEW** | Transfer Service | Route yêu cầu hủy Rx tới đúng CanTp SDU khi sink lỗi, hết capacity hoặc PC ngắt kết nối. |
| `PduR_CanIfRxIndication()` **NEW** | CanIf, direct route | Phân phối I-PDU một frame tới COM hoặc test consumer đã cấu hình. |
| `PduR_CanIfTxConfirmation()` **NEW** | CanIf, direct route | Chuyển confirmation về đúng direct source PDU. |
| `PduR_CanTpStartOfReception()` **NEW** | CanTp | Hỏi Transfer Service có chấp nhận object và có capacity trước khi nhận payload. |
| `PduR_CanTpCopyRxData()` **NEW** | CanTp | Stream segment vào Transfer Service mà PduR không sở hữu buffer ảnh. |
| `PduR_CanTpRxIndication()` **NEW** | CanTp | Báo kết thúc thành công/thất bại để Transfer Service commit hoặc hủy object. |
| `PduR_CanTpCopyTxData()` **NEW** | CanTp | Cấp đúng chunk kế tiếp từ Transfer Service khi CanTp sẵn sàng gửi. |
| `PduR_CanTpTxConfirmation()` **NEW** | CanTp | Kết thúc transfer Tx và giải phóng source state đúng thời điểm. |

**Ranh giới route:** PduR route I-PDU/SDU giữa upper owner và CanIf/CanTp. N-PDU transport đi trực tiếp `CanIf <-> CanTp`, như sơ đồ Tx/Rx. CanIf chọn đúng một upper owner (`PDUR_DIRECT` hoặc `CANTP`) theo config; không giao một event cho cả hai. PduR không nhận rồi route lại từng N-PDU transport.

Baseline chỉ hỗ trợ route 1:1, không gateway/fan-out. ID là local theo module và direction; chỉ cấm trùng key trong cùng namespace. Bảng route phải có cả ánh xạ forward và callback về source. Một destination Tx PDU đang pending không nhận thêm request; không suy source từ HTH.

## 7. CanTp module — multi-frame transport

CanTp nhận một logical SDU dài, chia thành CAN frame tối đa 8 byte, kiểm soát sequence/flow/timeout và ghép lại ở node nhận.

```c
Comm_ReturnType CanTp_Init(const CanTp_ConfigType *config);
Comm_ReturnType CanTp_Transmit(PduIdType txSduId,
                               TransferLengthType totalLength);
Comm_ReturnType CanTp_CancelTransmit(PduIdType txSduId);
Comm_ReturnType CanTp_CancelReceive(PduIdType rxSduId);
Comm_ReturnType CanTp_GetStatus(PduIdType sduId,
                                CanTp_TransferStatusType *statusOut);
void CanTp_MainFunction(uint32_t nowMs);

void CanTp_RxIndication(PduIdType rxNPduId,
                        const PduInfoType *canFrame);
void CanTp_TxConfirmation(PduIdType txNPduId,
                          Comm_ReturnType result);
Comm_ReturnType CanTp_GetStats(CanTp_StatsType *statsOut);
```

| API | Caller | Vì sao cần |
|---|---|---|
| `CanTp_Init()` **NEW** | App init | Tạo state machine từ channel config và xác nhận timeout/block-size hợp lệ. |
| `CanTp_Transmit()` **NEW** | PduR | Bắt đầu một SDU nhiều frame; payload được lấy dần qua `PduR_CanTpCopyTxData()`. |
| `CanTp_CancelTransmit()` **NEW** | PduR | Dừng transfer khi UART source hủy, timeout hoặc node reset. |
| `CanTp_CancelReceive()` **NEW** | PduR | Giải phóng reassembly state khi sink không còn capacity hoặc integrity thất bại. |
| `CanTp_GetStatus()` **NEW** | Transfer/diagnostic | Phân biệt idle, receiving, transmitting, waiting-flow-control và failed. |
| `CanTp_MainFunction()` **NEW** | Super-loop | Tiến state machine và kiểm tra timeout bằng timebase; không dùng busy-wait. |
| `CanTp_RxIndication()` **NEW** | CanIf trực tiếp | Nhận N-PDU và validate theo transport profile đã chọn. |
| `CanTp_TxConfirmation()` **NEW** | CanIf trực tiếp | Tiến state của N-PDU đã phát; vẫn phải tuân thủ flow control và separation time. |
| `CanTp_GetStats()` **NEW** | Status/diagnostic | Cung cấp timeout, sequence error, abort và byte/frame counter cho test evidence. |

`CanTp_Transmit()` không nhận pointer tới toàn bộ ảnh vì thiết kế mục tiêu là streaming, không giữ toàn bộ object trong RAM.

**Gate trước implementation:** tên CanTp không khẳng định tương thích ISO-TP. Chốt profile, header, addressing, giới hạn SDU, sequence wrap, FC, timeout và retry giữa các ECU trước khi viết state machine. Chốt một Tx và một Rx slot riêng hoặc policy serialize; local `TransferIdType` không tự ngăn frame cũ sau reset trên bus.

Buffer callback draft dùng all-or-nothing cho mỗi chunk: `CopyRxData` chỉ trả OK sau khi copy toàn segment; BUSY không consume byte. `CopyTxData` dùng `segmentOut.capacity` làm số byte yêu cầu, OK phải cung cấp đúng số đó; BUSY đặt length=0 và không tiến cursor. CanTp yêu cầu số byte còn lại nếu segment cuối ngắn; giữ bản copy N-PDU đã tạo khi CanIf BUSY, không gọi CopyTxData lần nữa để lấy nhầm chunk kế tiếp. `remainingDataOut` là số byte chưa đọc của SDU, không đảm bảo các byte đã sẵn sàng ở source.

`StartOfReception` cần trả thêm capacity ban đầu hoặc có API query trước khi phát FC; đây là phần signature **chưa freeze**. Cancel/timeout phải kết thúc đúng một lần và cô lập confirmation/frame muộn trước khi tái dùng slot. Retry transport khác với retry `CanIf_Transmit()` khi BUSY; offset/replay policy cần được bổ sung nếu chọn retransmit dữ liệu đã consume.

## 8. CanIf module — CAN abstraction

CanIf sở hữu mapping `TxPduId -> CAN ID/HTH/controller` và `controller + HRH + CAN ID -> RxPduId`. Đây là phần cần tách khỏi `CanUpper.c`.

```c
Comm_ReturnType CanIf_Init(const CanIf_ConfigType *config);
Comm_ReturnType CanIf_SetControllerMode(uint8_t controllerId,
                                        Can_ControllerModeType mode);
Comm_ReturnType CanIf_GetControllerStatus(uint8_t controllerId,
                                          Can_ControllerStatusType *statusOut);
Comm_ReturnType CanIf_Transmit(PduIdType txPduId,
                               const PduInfoType *pduInfo);
Comm_ReturnType CanIf_GetStats(CanIf_StatsType *statsOut);

void CanIf_TxConfirmation(PduIdType swPduHandle,
                          Comm_ReturnType result);
void CanIf_RxIndication(const Can_HwType *mailbox,
                        const PduInfoType *pduInfo);
void CanIf_ControllerBusOff(uint8_t controllerId);
```

| API | Caller | Vì sao cần |
|---|---|---|
| `CanIf_Init()` **NEW** | App init | Validate PDU/CAN-ID/HOH mapping cho node hiện tại trước khi CAN bắt đầu. |
| `CanIf_SetControllerMode()` **NEW** | App/state manager | Start/stop controller mà upper layer không gọi CAN Driver trực tiếp. |
| `CanIf_GetControllerStatus()` **NEW** | Test supervisor/state owner | Đọc snapshot mode/bus-off/error qua CanIf, không bỏ qua boundary để đọc driver/register. |
| `CanIf_Transmit()` **NEW** | PduR direct hoặc CanTp | Biến logical TxPduId thành `Can_PduType`, thêm CAN ID/HTH/software handle rồi gọi `Can_Write()`. |
| `CanIf_GetStats()` **NEW** | Status/diagnostic | Đếm unknown PDU, invalid length, mapping miss và driver busy. |
| `CanIf_TxConfirmation()` **NEW** | CAN Driver | Dùng software PDU handle để xác định đúng TxPduId, kể cả nhiều PDU dùng chung HTH. |
| `CanIf_RxIndication()` **NEW** | CAN Driver | Match controller + HRH + CAN ID rồi gọi PduR direct hoặc CanTp đúng owner. |
| `CanIf_ControllerBusOff()` **NEW** | CAN Driver | Latch bus-off; supervisor đọc qua GetControllerStatus và quyết định recovery. |

### 8.1 Admission, buffer và confirmation

- Baseline **không có software TX queue** tại CanIf. Mỗi HTH/MB chỉ có một request outstanding; PDU khác chung HTH nhận `COMM_BUSY` đến khi terminal event cũ đã được xử lý. Cùng TxPduId cũng chỉ có một request outstanding.
- `CanIf_Transmit()` OK chỉ khi `Can_Write()` OK. Driver copy 0..8 byte vào MB trước khi return; caller được sửa/free buffer payload sau đó. `swPduHandle` là CanIf TxPduId đã được kiểm tra và driver lưu riêng theo MB.
- Terminal callback mang đúng handle đã lưu, không tìm PDU bằng HTH. Xóa pending trước khi chuyển callback; callback muộn/không có pending phải bị loại và ghi counter. Không tái dùng MB trước khi cờ/event cũ được xử lý hoặc vô hiệu hóa.
- RX callback mượn mailbox/payload chỉ trong lời gọi. Upper consumer copy trước khi return nếu xử lý deferred. Filter hardware là bước sơ bộ; CanIf phải kiểm tra software mapping, length policy và owner. Mapping miss thì drop + counter.
- CAN frame TX confirmation chỉ xác nhận tầng CAN; không chứng minh đúng ECU đích đã xử lý, CRC object đã pass hay PC đích đã commit. ACK có thể do node khác trên bus phát. [Nguồn: NXP CAN ACK FAQ](https://community.nxp.com/t5/LPC-FAQs/How-does-the-acknowledge-and-error-detection-system-work/m-p/590615).

CanIf dispatch tới upper owner là lựa chọn bám sơ đồ của project; tham khảo khái niệm upper-layer confirmation trong [AUTOSAR CAN Interface, R21-11, mục 7.12](https://www.autosar.org/fileadmin/standards/R21-11/CP/AUTOSAR_SWS_CANInterface.pdf). Các return/result/lifecycle ở đây vẫn là contract riêng.

## 9. CAN Driver — FlexCAN hardware access

CAN Driver được refactor từ `can_task/driver` và hiện nằm tại `drivers/can/driver`; cấu hình nằm tại `drivers/can/config/can`. Driver không include `CanUpper`, PduR hoặc application header. `can_task` được giữ làm tham khảo và không còn thuộc source set `Debug_FLASH`.

### 9.1 Type bắt buộc

```c
/* Target types: thay Can_ReturnType cũ khi refactor, không định nghĩa trùng. */
typedef enum
{
    CAN_OK = 0,
    CAN_NOT_OK = 1,
    CAN_BUSY = 2,
    CAN_TIMEOUT,
    CAN_INVALID_PARAM,
    CAN_NOT_INITIALIZED,
    CAN_INVALID_STATE,
    CAN_BUS_OFF,
    CAN_CANCELLED
} Can_ReturnType;

typedef enum
{
    CAN_CONTROLLER_UNINIT = 0,
    CAN_CONTROLLER_STOPPED,
    CAN_CONTROLLER_STARTED,
    CAN_CONTROLLER_FAULT
} Can_ControllerModeType;

typedef struct
{
    Can_ControllerModeType mode;
    bool busOff;
    uint8_t txErrorCounter;
    uint8_t rxErrorCounter;
    Can_ReturnType lastError;
} Can_ControllerStatusType;

typedef struct
{
    Can_IdType id;
    PduIdType swPduHandle;
    uint8_t length;
    const uint8_t *sdu;
} Can_PduType;

typedef struct
{
    Can_IdType canId;
    Can_HwHandleType hohId;
    uint8_t controllerId;
} Can_HwType;
```

`swPduHandle` bắt buộc vì HTH nhận diện hardware transmit object, không nhận diện duy nhất logical PDU đang dùng HTH đó.

Header target phải include `<stdint.h>`, `<stdbool.h>` và shared ID types; không phụ thuộc header S32K144 ở CanIf/PduR để có thể test host. `busOff` là trạng thái lỗi tách khỏi mode, không phải mode truyền vào SetControllerMode. Error counters là snapshot tại lúc đọc, không hứa giá trị đồng bộ giữa ba ECU.

### 9.2 API

```c
Can_ReturnType Can_Init(const Can_ConfigType *config);
Can_ReturnType Can_RegisterCallbacks(const Can_CallbacksType *callbacks);
Can_ReturnType Can_SetControllerMode(uint8_t controllerId,
                                     Can_ControllerModeType mode);
Can_ReturnType Can_Write(Can_HwHandleType hth,
                         const Can_PduType *pduInfo);
void Can_MainFunction_Write(void);
void Can_MainFunction_Read(void);
void Can_MainFunction_Error(void);
Can_ReturnType Can_GetControllerStatus(uint8_t controllerId,
                                       Can_ControllerStatusType *statusOut);
Can_ReturnType Can_GetStats(Can_StatsType *statsOut);
void Can_ResetStats(void);
```

| API | Trạng thái | Vì sao cần |
|---|---|---|
| `Can_Init()` | **MODIFY** từ API hiện có | Giữ trình tự FlexCAN init nhưng phải trả lỗi cho invalid config, clock/freeze timeout hoặc hardware không ready. |
| `Can_RegisterCallbacks()` | **NEW, IMPLEMENTED** | Đăng ký Tx/Rx/bus-off callback khi STOPPED để driver không phụ thuộc header CanIf cụ thể. CanIf là consumer dự kiến. |
| `Can_SetControllerMode()` | **NEW** | Tách init configuration khỏi start/stop communication; cần cho reset và bus-off recovery. |
| `Can_Write()` | **MODIFY** | Giữ validation/pack/busy check hiện có, đồng thời lưu `swPduHandle` cho confirmation chính xác. |
| `Can_MainFunction_Write()` | **MODIFY** contract, giữ signature | Thay callback CanUpper bằng CanIf, hoàn tất request bằng software handle trong main context. |
| `Can_MainFunction_Read()` | **MODIFY** contract, giữ signature | Thay callback CanUpper bằng CanIf và bảo đảm RX lifetime trong main context. |
| `Can_MainFunction_Error()` | **NEW** | Poll bus-off/error state và phát event có kiểm soát. |
| `Can_GetControllerStatus()` | **NEW** | Cung cấp mode/error counters cho test và status mà không đọc register ngoài driver. |
| `Can_GetStats()` / `Can_ResetStats()` | **NEW, IMPLEMENTED** | Cho host/board test quan sát counter mà không truy cập static state hoặc FlexCAN register. |

Driver-to-CanIf callback là một phần contract của mục 8; driver không tự lưu application payload dài.

### 9.3 Lifecycle và giới hạn chờ

Contract đề xuất cho milestone tầng thấp:

| Trigger | Hành vi bắt buộc |
|---|---|
| `Can_Init()` khi UNINIT | Validate toàn bộ config trước khi truy cập MB; thành công ở STOPPED, không phát/dispatch frame. Không tự start như code tham khảo. |
| `Can_SetControllerMode(..., STARTED)` | Chỉ từ STOPPED và đã hết điều kiện bus-off; hoàn tất đồng bộ trong timeout cấu hình. OK nghĩa mode thực đã đạt. |
| Set cùng mode STARTED/STOPPED | Idempotent OK; không xóa pending hoặc tạo terminal event mới. |
| Write khi UNINIT/STOPPED/FAULT/bus-off | Trả lỗi tương ứng, không nhận request. Chỉ STARTED, không bus-off mới nhận TX. |
| STOP từ STARTED | Chặn nhận TX mới, dừng/abort phần cứng trong timeout, loại RX cũ. Request chưa có TX completion được kết thúc `COMM_CANCELLED`; completion đã xác nhận thì giữ kết quả thực. |
| Bus-off phát hiện qua Error main | Chặn TX, latch lỗi, đưa controller về trạng thái không giao tiếp; kết thúc request pending `COMM_BUS_OFF`, phát BusOff event một lần cho mỗi lần chuyển sang bus-off. |
| Chờ phần cứng timeout | Trả `CAN_TIMEOUT`, latch FAULT và chặn traffic; không publish mode thành công. Request đã nhận phải kết thúc với `COMM_TIMEOUT` qua main dispatch. |
| Recovery | Supervisor yêu cầu STOP rồi START sau khi phần cứng cho phép; driver không retry vô hạn hay tự start. FAULT chỉ thử STOP có timeout; nếu không thể cô lập phần cứng, cần reset có kiểm soát. |

Validation fail trước khi chạm hardware giữ UNINIT và cho phép sửa config rồi retry init. Init đã chạm hardware nhưng timeout thì giữ FAULT, chưa initialized; chỉ retry init sau khi BSP/reset đã cô lập hardware. Recovery STOP/START ở bảng áp dụng cho driver đã init thành công. SetControllerMode chỉ nhận target STOPPED/STARTED; UNINIT/FAULT là trạng thái quan sát, không phải yêu cầu mode hợp lệ.

Terminal callback chỉ được dispatch từ main polling, sau khi API request/mode đã return. STOP/bus-off phải giữ terminal record đến khi dispatch; không xóa mất record khi reset MB. Nếu không thể chứng minh MB/event cũ đã được vô hiệu hóa, giữ FAULT và không nhận request mới. Power reset không bảo đảm callback cho request của boot trước; tầng protocol phải phát hiện riêng.

Baseline chọn mode transition **đồng bộ, có giới hạn chờ**, nên chưa có `CanIf_ControllerModeIndication()`. Nếu sau này chọn bất đồng bộ, phải bổ sung state TRANSITION, completion/error event và deadline cùng lúc.

`Can_ConfigType` target phải chứa time source callback monotonic ms, `modeTimeoutMs`, `txTimeoutMs` hữu hạn khác 0; polling Error main kiểm tra deadline cả khi không có TX completion (ví dụ thiếu ACK). Hết TX deadline thì dừng/cô lập controller, phát một terminal TIMEOUT cho mỗi request còn pending và chờ supervisor recovery. Completion đã được hardware xác nhận trước xử lý timeout giữ kết quả thực; không ghi đè SUCCESS đã được latch. Không tái phát request đã timeout tự động. Timebase phải chạy trước init và tiếp tục chạy khi chờ phần cứng; mọi vòng chờ init/mode cần thêm giới hạn số lần poll để vẫn thoát nếu timebase không tiến. Driver hiện tại mới có `hardwareTimeoutCount` làm poll bound cho chuyển mode; time source và Tx deadline vẫn là **chưa triển khai**.

Mapping tại CanIf: `CAN_OK/BUSY/TIMEOUT/INVALID_PARAM/NOT_INITIALIZED/INVALID_STATE/BUS_OFF/NOT_OK` lần lượt sang `COMM_OK/BUSY/TIMEOUT/INVALID_PARAM/NOT_INITIALIZED/INVALID_STATE/BUS_OFF/NOT_OK`. STOP cancellation là terminal `COMM_CANCELLED`, không phải kết quả `Can_Write()`.

### 9.4 Configuration và RX policy tầng thấp

- Baseline controller 0, standard data frame 11-bit, payload 0..8 byte; extended/RTR/FD không thuộc profile. Driver phải kiểm tra và bỏ frame ngoài profile trước callback, có counter nguyên nhân.
- `Can_ConfigType` target cần controller ID, source clock Hz, bit-timing profile/baudrate, normal/loopback, polling mode, timeouts và HOH list/count. Nếu chỉ hỗ trợ profile 8 MHz/500 kbit/s, config khác phải bị từ chối; không nhận baudrate bất kỳ rồi vẫn lập trình 500 kbit/s.
- Validate HOH ID duy nhất trong controller, type TX/RX hợp lệ, MB trong giới hạn driver hỗ trợ, không hai HOH chiếm cùng MB, ID/mask hợp lệ, pointer/count nhất quán. Giới hạn **16 MB là của driver tham khảo**, không suy ra là giới hạn mọi mode của chip.
- Một HRH có thể lọc nhiều CAN ID; nhiều Tx PDU có thể dùng chung HTH theo policy BUSY. CanIf kiểm tra mapping không mơ hồ trong namespace controller/direction; cấu hình trùng/thiếu upper owner phải fail init.
- Một nơi duy nhất đọc/clear hardware flags. Baseline polling tắt MB IRQ; bản interrupt sau này capture frame/event vào storage tĩnh, main drain. Không đồng thời polling và ISR xử lý cùng cờ.
- RX overrun hoặc event queue full phải tăng counter/latch lỗi mất dữ liệu, không báo frame bị truncate như hợp lệ. Reserve terminal TX record theo MB để RX flood không làm mất TX completion.

Diagnostics tối thiểu cần quan sát: txAccepted/completed/failed/busy, rxDelivered/dropped/overrun, invalidInput, staleEvent, busOffCount và mode/txTimeoutCount. Có thể đặt các field driver trong status mở rộng trước khi freeze; CanIf stats giữ lỗi mapping của CanIf. Log main context theo record `time, node, module, event, controller, pdu/hoh, result`; không in text blocking trong ISR hoặc trộn log vào UART binary stream. Counter `uint32_t` dùng modulo, test tính delta modulo trong cửa sổ hữu hạn.

## 10. Transfer Service — object/session ownership

Transfer Service quản lý metadata, CRC, byte count và lifecycle của text/image/raw object. CanTp chỉ quản lý transport frame.

**DEFERRED:** các signature dưới đây là skeleton; chưa đủ để triển khai end-to-end. Metadata trong RAM không tự được truyền sang ECU khác. Cần chốt envelope chứa version, wire session identity, loại object, length và CRC, cùng nơi encode/decode và cách tính độ dài SDU (bao gồm header hay chỉ payload). Không dùng local transfer handle làm wire session ID mặc định.

### 10.1 Metadata

```c
typedef enum
{
    TRANSFER_OBJECT_TEXT = 0,
    TRANSFER_OBJECT_IMAGE,
    TRANSFER_OBJECT_BINARY
} Transfer_ObjectType;

typedef struct
{
    Transfer_ObjectType objectType;
    uint8_t sourceNode;
    uint8_t destinationNode;
    TransferLengthType totalLength;
    uint32_t expectedCrc;
} Transfer_MetadataType;
```

### 10.2 API cho UART Gateway

```c
Comm_ReturnType Transfer_Init(const Transfer_ConfigType *config);
Comm_ReturnType Transfer_StartTx(const Transfer_MetadataType *metadata,
                                 TransferIdType *transferIdOut);
Comm_ReturnType Transfer_WriteTxData(TransferIdType transferId,
                                     const uint8_t *data,
                                     uint16_t length,
                                     uint16_t *acceptedLengthOut);
Comm_ReturnType Transfer_EndTx(TransferIdType transferId);
Comm_ReturnType Transfer_ReadRxData(TransferIdType transferId,
                                    MutableBufferType *dataOut,
                                    TransferLengthType *remainingDataOut);
Comm_ReturnType Transfer_Abort(TransferIdType transferId,
                               Transfer_AbortReasonType reason);
Comm_ReturnType Transfer_GetStatus(TransferIdType transferId,
                                   Transfer_StatusType *statusOut);
Comm_ReturnType Transfer_GetStats(Transfer_StatsType *statsOut);
void Transfer_MainFunction(uint32_t nowMs);
```

| API | Vì sao cần |
|---|---|
| `Transfer_Init()` **NEW** | Khởi tạo session table, queue và CRC state trước khi nhận byte UART. |
| `Transfer_StartTx()` **NEW** | Validate metadata/length/destination và cấp ID để các chunk sau không lẫn transfer. |
| `Transfer_WriteTxData()` **NEW** | Nhận binary chunk từ UART với partial-accept semantics; caller biết chính xác bao nhiêu byte đã được queue. |
| `Transfer_EndTx()` **NEW** | Xác nhận UART source đã gửi đủ object; phát hiện length thiếu/thừa trước khi báo thành công. |
| `Transfer_ReadRxData()` **NEW** | Cho UART Gateway drain chunk đã nhận mà không truy cập buffer nội bộ. |
| `Transfer_Abort()` **NEW** | Hủy có lý do xác định khi PC cancel, timeout, CRC error hoặc reset. |
| `Transfer_GetStatus()` **NEW** | UART Gateway biết khi nào gửi ACK/NACK/final result cho PC. |
| `Transfer_GetStats()` **NEW** | Cung cấp byte count, CRC failure, abort và overflow cho end-to-end evidence. |
| `Transfer_MainFunction()` **NEW** | Tiến session state, timeout và gọi PduR/CanTp ngoài ISR. |

### 10.3 API được PduR gọi

```c
Comm_ReturnType Transfer_StartOfReception(PduIdType rxSduId,
                                          TransferLengthType totalLength);
Comm_ReturnType Transfer_CopyRxData(PduIdType rxSduId,
                                    const PduInfoType *segment,
                                    TransferLengthType *remainingCapacityOut);
void Transfer_RxIndication(PduIdType rxSduId,
                           Comm_ReturnType result);
Comm_ReturnType Transfer_CopyTxData(PduIdType txSduId,
                                    MutableBufferType *segmentOut,
                                    TransferLengthType *remainingDataOut);
void Transfer_TxConfirmation(PduIdType txSduId,
                             Comm_ReturnType result);
```

Các API này cần để PduR route CanTp buffer callbacks tới đúng owner mà không tự giữ ảnh trong PduR.

### 10.4 Các điều kiện phải bổ sung trước khi freeze

- `Transfer_WriteTxData`: OK khi nhận toàn bộ; BUSY có thể nhận prefix và phải trả chính xác `acceptedLengthOut` trong `0..length`; caller chỉ retry phần còn lại. Lỗi khác không nhận byte. Ngoại lệ partial này không áp dụng cho CopyRxData/CopyTxData của CanTp.
- RX cần một đường discover session cho Gateway: callback/event chứa local transferId + metadata hoặc API dequeue session. Hiện `ReadRxData(transferId)` chưa có API cung cấp ID đó cho caller; chưa được xem bộ API này là đầy đủ.
- StartOfReception hiện chỉ có length/SDU ID: phải chỉ rõ lúc nào metadata hợp lệ, cách reject metadata/CRC và capacity ban đầu trước khi cấp flow control.
- Chốt streaming sang file tạm ở PC hay giữ object ở ECU (mục 20). Nếu streaming, `ReadRxData` chỉ trả chunk tạm thời; chỉ final SUCCESS mới cho PC commit. Nếu giữ ở ECU, admission phải từ chối object vượt storage thực tế. Yêu cầu README “chỉ xuất sau integrity” chưa tương thích streaming tạm nếu chưa bổ sung PC commit protocol.
- Tách các mốc source đã gửi đủ byte (`EndTx`), transport TX xong, sink ECU kiểm CRC xong, PC đích commit xong. `Transfer_TxConfirmation(COMM_OK)` từ CanTp chỉ là transport complete; final end-to-end result cần ACK riêng và deadline/retry/idempotence riêng.
- Chốt CRC polynomial/init/reflection/xorout, byte order, phạm vi byte được tính và test vector dùng chung; chỉ có field `uint32_t expectedCrc` chưa tạo interoperability.

## 11. UART Gateway — PC packet framing

UART Gateway parse/encode packet giữa PC và ECU. Module này bảo toàn mọi byte `0x00..0xFF` và không dùng line parser của Task 5.

```c
Comm_ReturnType UartGateway_Init(const UartGateway_ConfigType *config);
void UartGateway_MainFunction(uint32_t nowMs);
void UartGateway_OnRxByte(uint8_t rxByte);
bool UartGateway_GetNextTxByte(uint8_t *txByteOut);
Comm_ReturnType UartGateway_GetStats(UartGateway_StatsType *statsOut);
```

| API | Caller/context | Vì sao cần |
|---|---|---|
| `UartGateway_Init()` **NEW** | App init | Init parser state, RX/TX ring buffer và đăng ký UART callback. |
| `UartGateway_MainFunction()` **NEW** | Super-loop | Parse header/payload, gọi Transfer Service và encode response ngoài ISR. |
| `UartGateway_OnRxByte()` **NEW** | UART RX callback/ISR | Push đúng một byte vào RX queue; không parse hoặc block trong ISR. |
| `UartGateway_GetNextTxByte()` **NEW** | UART TX callback/ISR | Pop một byte đã chuẩn bị sẵn; trả `false` để driver tắt TX-empty interrupt khi queue rỗng. |
| `UartGateway_GetStats()` **NEW** | Status/diagnostic | Báo framing error, CRC error, RX/TX bytes và queue overflow. |

Packet format cụ thể chưa được đóng băng; API không phụ thuộc magic/version/header layout cụ thể.

## 12. UART Driver — hardware byte transfer

Các API hiện có trong [`Driver_UART.h`](../drivers/uart/Driver_UART.h) cung cấp byte I/O cho baseline; gateway framing/backpressure và recovery vẫn cần thiết kế riêng.

```c
UART_Status_t LPUART1_Init(uint32_t baudRate);
void LPUART1_Deinit(void);
void LPUART1_RegisterCallbacks(UART_RxCallback_t rxCb,
                              UART_TxCallback_t txCb);
void LPUART1_EnableTxInterrupt(void);
uint32_t LPUART1_GetRxCount(void);
uint32_t LPUART1_GetTxCount(void);
uint32_t LPUART1_GetConfiguredBaudRate(void);
uint32_t LPUART1_GetActualBaudRate(void);
uint32_t LPUART1_GetErrorCount(void);
void LPUART1_ResetStats(void);
```

| API | Trạng thái | Vì sao cần |
|---|---|---|
| `LPUART1_Init()` | **EXISTING** | Cấu hình hardware từ baud đã chốt và fail nếu baud error vượt giới hạn. |
| `LPUART1_Deinit()` | **EXISTING** | Dừng IRQ/peripheral an toàn khi app init lỗi hoặc node shutdown/reset. |
| `LPUART1_RegisterCallbacks()` | **EXISTING** | Nối ISR byte path với UART Gateway mà driver không include gateway header. |
| `LPUART1_EnableTxInterrupt()` | **EXISTING** | Kick TX sau khi gateway thêm dữ liệu; không polling/blocking cả file. |
| Các API `Get*Count/Rate()` | **EXISTING** | Cung cấp observability cho status và performance test. |
| `LPUART1_ResetStats()` | **EXISTING** | Tạo test window xác định, không cộng dồn số liệu từ boot trước. |

`LPUART1_SendString_Blocking()` và `LPUART1_SendChar_Blocking()` chỉ nên dùng cho boot/debug text, không dùng trong bulk data path.

**Ràng buộc tái sử dụng:** RX callback `void` không thể trả BUSY cho PC; RX queue full phải latch loss, tăng overflow counter và yêu cầu parser hủy/resync packet ở main context. Protocol PC cần credit/ACK hoặc giới hạn packet phù hợp queue, không xem BUSY nội bộ là flow control trên dây. Driver hiện đếm hardware error chung và vẫn có thể giao byte RX; gateway phải coi thay đổi error counter là packet bị nghi ngờ, không chỉ log rồi commit. Callback registration/reset stats/deinit chỉ thực hiện khi UART IRQ đã được cô lập. Queue TX phải publish dữ liệu rồi gọi `LPUART1_EnableTxInterrupt()` để khởi động lại sau empty; byte đã ghi thanh ghi TX chưa chứng minh PC đã nhận/commit.

## 13. RingBuffer middleware

Các API hiện có trong [`ring_buffer.h`](../middlewares/ring_buffer.h) có thể tái sử dụng cho UART byte queue.

```c
RingBuffer_Status_t RingBuffer_Init(RingBuffer_t *rb,
                                    uint8_t *buffer,
                                    uint16_t capacity);
RingBuffer_Status_t RingBuffer_Push(RingBuffer_t *rb, uint8_t data);
RingBuffer_Status_t RingBuffer_Pop(RingBuffer_t *rb, uint8_t *data);
bool RingBuffer_IsEmpty(const RingBuffer_t *rb);
bool RingBuffer_IsFull(const RingBuffer_t *rb);
void RingBuffer_Clear(RingBuffer_t *rb);
uint16_t RingBuffer_GetCount(const RingBuffer_t *rb);
uint16_t RingBuffer_GetFree(const RingBuffer_t *rb);
```

| API | Vì sao cần |
|---|---|
| `RingBuffer_Init()` **EXISTING** | Gắn state vào storage tĩnh; không cần dynamic allocation. |
| `RingBuffer_Push()` **EXISTING** | ISR enqueue một byte và nhận lỗi FULL rõ ràng. |
| `RingBuffer_Pop()` **EXISTING** | Consumer dequeue một byte và nhận EMPTY rõ ràng. |
| `RingBuffer_IsEmpty()` **EXISTING** | TX callback biết khi nào tắt TX interrupt. |
| `RingBuffer_IsFull()` **EXISTING** | Dùng cho status/debug; producer vẫn phải kiểm tra return của `Push()`. |
| `RingBuffer_Clear()` **EXISTING** | Reset parser/session sau abort hoặc re-init. |
| `RingBuffer_GetCount()` **EXISTING** | Biết số byte có thể parse/drain mà không chạm head/tail nội bộ. |
| `RingBuffer_GetFree()` **EXISTING** | Admission control cho một packet/chunk trước khi bắt đầu copy. |

Ring buffer byte không thay thế queue descriptor cho CAN frame hoặc transfer. Nếu nhiều producer/consumer được thêm, concurrency contract phải được thiết kế lại.

Storage có `capacity` byte chỉ chứa được **capacity - 1** byte theo source hiện tại; init cần capacity >= 2. Dùng riêng RX queue (ISR producer/main consumer) và TX queue (main producer/ISR consumer). Đây là mục tiêu SPSC cần kiểm chứng trên compiler/target; `volatile` không tự chứng minh an toàn cho mọi concurrency. Không gọi Clear/Init khi producer/consumer còn hoạt động; dừng IRQ liên quan trước. GetCount/GetFree chỉ là snapshot, không thay kết quả Push/Pop hay reservation nhiều byte.

## 14. Timebase và scheduling

Baseline dùng SysTick hiện có:

```c
uint32_t Driver_SysTick_Init(uint32_t tickHz,
                             Driver_SysTick_Callback_t callback);
uint32_t Driver_SysTick_GetTicks(void);
void Driver_SysTick_Handler(void);
```

| API | Vì sao cần |
|---|---|
| `Driver_SysTick_Init()` **EXISTING** | Cung cấp timebase chung cho transport timeout, heartbeat và UART packet timeout. |
| `Driver_SysTick_GetTicks()` **EXISTING** | Main loop lấy timestamp monotonic và truyền `nowMs` vào các main function. |
| `Driver_SysTick_Handler()` **EXISTING** | Cập nhật tick/callback tại ISR; không chạy transport state machine trong ISR. |

Upper modules nên nhận `nowMs` qua `MainFunction(nowMs)` thay vì tự phụ thuộc trực tiếp vào SysTick driver. Cách này làm unit test timeout deterministic.

`Driver_SysTick_GetTicks()` trả **tick**, chỉ tương đương ms khi init thành công ở 1000 Hz với `SystemCoreClock` đúng. Baseline dùng 1000 Hz và kiểm tra return (`0` thành công, khác 0 thất bại); tần số khác cần adapter đổi đơn vị có xử lý rollover. Deadline kiểm tra elapsed unsigned `(uint32_t)(nowMs - startMs) >= timeoutMs`, với timeout < `2^31` ms và scheduler không bỏ qua cả chu kỳ counter. Time source của driver phải cùng đơn vị, test có thể inject fake clock.

Trong mỗi vòng super-loop: lấy timestamp cho upper modules; gọi `Can_MainFunction_Error()` trước Write/Read để lỗi controller chặn traffic sớm; dispatch terminal events rồi RX; tiếp theo PduR consumer/CanTp/Transfer/UART/COM theo các module đã bật. Mỗi lượt có budget số frame/byte hữu hạn. Cấu hình phải ghi max service interval và timeout; đo interval lớn nhất dưới tải trước khi chốt bitrate/queue. Không khẳng định polling một MB đủ nhận mọi traffic ba ECU khi chưa đo.

LPIT chỉ cần khi requirement chốt separation timing không thể đáp ứng bằng SysTick. API LPIT hiện tại gắn chặt CH0/CH2/CH3, nên chưa coi là API transport chính thức.

## 15. NVIC và BSP support

### 15.1 NVIC

CAN/UART Driver cần các API hiện có:

```c
void NVIC_EnableIRQ(IRQn_Type irqn);
void NVIC_DisableIRQ(IRQn_Type irqn);
void NVIC_ClearPendingIRQ(IRQn_Type irqn);
void NVIC_SetPriority(IRQn_Type irqn, uint32_t priority);
```

| API | Vì sao cần |
|---|---|
| `NVIC_EnableIRQ()` | Bật CAN/UART IRQ sau khi peripheral state và callback đã sẵn sàng. |
| `NVIC_DisableIRQ()` | Ngăn ISR chạy trong deinit/reconfiguration. |
| `NVIC_ClearPendingIRQ()` | Không xử lý cờ cũ ngay sau khi init. |
| `NVIC_SetPriority()` | Chốt ưu tiên CAN/UART thay vì phụ thuộc reset default. |

Chỉ driver/BSP gọi NVIC; COM, PduR, CanTp và application không gọi trực tiếp.

### 15.2 BoardCan BSP

```c
Comm_ReturnType BoardCan_Init(const BoardCan_ConfigType *config);
Comm_ReturnType BoardCan_SetTransceiverMode(BoardCan_ModeType mode);
Comm_ReturnType BoardCan_GetTransceiverMode(BoardCan_ModeType *modeOut);
```

| API | Vì sao cần |
|---|---|
| `BoardCan_Init()` **NEW** | Cấu hình SOSC, CAN pin mux và transceiver pins trước `Can_Init()`; loại bỏ hardware setup khỏi test application. |
| `BoardCan_SetTransceiverMode()` **NEW** | Chuyển transceiver giữa normal/standby trong init, reset hoặc error recovery. |
| `BoardCan_GetTransceiverMode()` **NEW** | Cho status/test xác minh software state mà không đọc GPIO trực tiếp. |

### 15.3 LED debug BSP

Các API `LED_Init()`, `LED_On()`, `LED_Off()` và `LED_Toggle()` đã tồn tại và chỉ dùng làm dấu hiệu debug. LED không được dùng làm bằng chứng duy nhất rằng payload dài truyền đúng.

## 16. Configuration modules

Các file dưới `drivers/can/config` gồm `node_cfg`, `com_cfg`, `pdur_cfg`, `cantp_cfg`, `canif_cfg` và `can/Can_Cfg` chủ yếu export `const` configuration; không cần setter runtime trong baseline.

```c
extern const Node_ConfigType Node_Config;
extern const Com_ConfigType Com_Config;
extern const PduR_ConfigType PduR_Config;
extern const CanTp_ConfigType CanTp_Config;
extern const CanIf_ConfigType CanIf_Config;
extern const Can_ConfigType Can_Config;
```

Không cung cấp runtime API sửa CAN ID/PDU route vì:

- cấu hình sai khi đang truyền có thể làm mất confirmation;
- static config dễ review và test trên ba firmware node;
- không có yêu cầu dynamic reconfiguration hiện tại.

Mỗi route local phải truy vết được chuỗi (không bắt buộc ánh xạ 1:1 giữa mọi ID):

```text
Signal/Object
  -> Source PDU
  -> PduR route
  -> CanTp SDU/N-PDU hoặc direct CanIf PDU
  -> CAN ID
  -> HTH/HRH
  -> FlexCAN Message Buffer
```

### 16.1 Contract chung giữa ba người và cấu hình riêng từng ECU

| Phải thống nhất trên bus | Được khác giữa các firmware |
|---|---|
| Bitrate và frame format; timing của node phải tương thích bus | Source clock, bit-timing register, pin mux và transceiver BSP |
| CAN ID, node duy nhất phát mỗi message type, node nhận, DLC | Tx/Rx PDU ID, HTH/HRH, MB index, bảng route nội bộ |
| Byte/bit layout, endian, signedness, unit/scale, reserved byte | Ngôn ngữ/cách viết code, tên API, struct RAM, cách pack nội bộ |
| Transport profile, sequence, flow control, timeout, session/reset behavior | Queue implementation và scheduler, miễn đáp ứng giới hạn timing/buffer chung |
| Error/result semantics và version/test vectors | Cách tổ chức module; có thể dùng adapter ở ranh giới local |

Trước physical integration, ba người phải có message matrix được đánh version với các cột: `message name, CAN ID, frame format, publisher, consumers, DLC policy, byte layout, period/event, deadline, profile version`. Với TP bổ sung data/FC ID mỗi chiều, max SDU, pacing/timeout và golden frames. Không tự cấp CAN ID chính thức từ range ví dụ trong README.

Ví dụ minh họa local mapping: ECU1 `TxPduId=7, HTH=0` và ECU3 `RxPduId=42, HRH=3` vẫn tương thích nếu cùng hiểu CAN ID/DLC/payload đã thống nhất; các số này **không phải cấu hình được cấp**. Trong một firmware, CanIf TxPduId có thể dùng trực tiếp làm `swPduHandle`; driver coi đó là token opaque.

### 16.2 Checklist freeze config tầng thấp

Các `*_ConfigType` chưa có đầy đủ khai báo C; trước coding cần tạo schema review được với tối thiểu:

- CAN: các field tại mục 9.3–9.4, rule range/duplicate và profile bitrate được hỗ trợ.
- CanIf Tx entry: local PDU ID, controller, HTH, CAN ID, length policy, upper owner và upper confirmation ID.
- CanIf Rx entry: controller, HRH, CAN ID/mask, length policy, upper owner và destination ID; reject overlap mơ hồ trong baseline.
- PduR direct entry: source module/direction/PDU, destination PDU và reverse confirmation mapping; stub consumer được cấu hình khi COM chưa có.
- BSP/timebase: nguồn clock, pin/transceiver mode và timing budget của scheduler; các node được có phần cứng wiring khác nhưng phải ghi chính xác.
- Giới hạn RAM/queue/event record; observer thống kê và format log. Config lifetime là static, init không giữ pointer tới object trên stack.

## 17. End-to-end call flow bằng API

### 17.1 Control Tx

```text
NodeApp
  -> Com_SendSignal()
  -> Com_MainFunctionTx()
  -> PduR_ComTransmit()
  -> CanIf_Transmit()
  -> Can_Write()
  -> Can_MainFunction_Write()
  -> CanIf_TxConfirmation()
  -> PduR_CanIfTxConfirmation()
  -> Com_TxConfirmation()
```

### 17.2 Control Rx

```text
Can_MainFunction_Read()
  -> CanIf_RxIndication()
  -> PduR_CanIfRxIndication()
  -> Com_RxIndication()
  -> NodeApp_OnControlUpdate()
```

### 17.3 Bulk Tx: PC1 -> ECU1 -> CAN

```text
LPUART1 RX ISR
  -> UartGateway_OnRxByte()
  -> UartGateway_MainFunction()
  -> Transfer_StartTx()
  -> Transfer_WriteTxData()
  -> PduR_TransferTransmit()
  -> CanTp_Transmit()
  -> PduR_CanTpCopyTxData()
  -> Transfer_CopyTxData()
  -> CanIf_Transmit()
  -> Can_Write()
```

### 17.4 Bulk Rx: CAN -> ECU3 -> PC2

```text
Can_MainFunction_Read()
  -> CanIf_RxIndication()
  -> CanTp_RxIndication()
  -> PduR_CanTpStartOfReception()
  -> Transfer_StartOfReception()
  -> PduR_CanTpCopyRxData()
  -> Transfer_CopyRxData()
  -> PduR_CanTpRxIndication()
  -> Transfer_RxIndication()
  -> Transfer_ReadRxData()
  -> UART TX ring buffer
  -> UartGateway_GetNextTxByte()
```

Ở bulk Rx, ReadRxData có thể được main gọi nhiều lần giữa các CopyRxData nếu chọn streaming; RxIndication cuối chỉ xác nhận transport kết thúc. Đường bulk frame confirmation là `CanIf_TxConfirmation -> CanTp_TxConfirmation`; khi SDU hoàn tất mới gọi `PduR_CanTpTxConfirmation -> Transfer_TxConfirmation`.

## 18. Init order

Test harness hiện tại chỉ cần chuỗi tầng thấp; chưa cần `GatewayApp_Init()`:

1. Cô lập CAN/UART IRQ và traffic từ peripheral trong boot setup; chuẩn bị clock chung và cập nhật `SystemCoreClock`.
2. `BoardCan_Init()` cấu hình pins/clock/transceiver ở trạng thái an toàn. Chờ nguồn clock trước timebase phải có poll-count bound riêng. Chỉ BSP chịu trách nhiệm xác nhận wiring thực tế.
3. `Driver_SysTick_Init(1000U, ...)`; callback boot chỉ cập nhật timebase. Kiểm tra return rồi cấp time-source callback cho CAN config.
4. `Can_Init()` ở STOPPED, polling, không dispatch upper callback.
5. `CanIf_Init()`, `PduR_Init()` và khởi tạo direct test consumer; validate cả forward/reverse mapping.
6. Với milestone sau: init COM/CanTp/Transfer theo route đã bật; UartGateway_Init sở hữu init queue/parser và đăng ký callback. Không init/clear queue lần hai ở app.
7. Sau khi mọi consumer sẵn sàng: đặt transceiver normal, `CanIf_SetControllerMode(..., CAN_CONTROLLER_STARTED)` và kiểm tra OK.
8. Chỉ khi gateway đã được triển khai: `LPUART1_Init()` để bật RX IRQ cuối cùng; từ đây main phải service queue và error state.

Mỗi bước kiểm tra kết quả; fail thì dừng chuỗi và đưa phần đã bật về trạng thái không nhận traffic. Nếu UART init lỗi sau CAN start, cô lập/deinit UART rồi yêu cầu CAN STOP, ghi cả lỗi gốc và lỗi cleanup nếu có. Không ghi SUCCESS hoặc retry start khi STOP thất bại. Config-only init không phát callback vào module kế tiếp chưa init. CAN controller START/STOP và transceiver normal/standby là hai việc riêng.

## 19. API validation checklist

Mỗi API implementation phải có test cho các trường hợp liên quan:

- gọi trước init;
- null input/output pointer;
- unknown signal/PDU/transfer/controller/HOH ID;
- zero length và maximum configured length;
- buffer full/empty;
- duplicate start/end/cancel;
- timeout và `uint32_t` tick rollover;
- callback đến sai state;
- multiple PDU dùng chung HTH;
- frame thiếu, trùng hoặc sai sequence;
- reset/abort rồi bắt đầu transfer mới.

### 19.1 Acceptance test tầng thấp trước COM/app

Đây là test design, **chưa phải test đã chạy/PASS**. Unit tests dùng fake driver/clock/event để kết quả xác định; register timing, wiring và arbitration cần board test có raw log.

| ID | Stimulus | Kết quả phải quan sát |
|---|---|---|
| LL-01 | Init config null, duplicate HOH/MB, ID/mask sai hoặc timing không hỗ trợ | Lỗi xác định, không publish initialized/STARTED; không ghi MB ngoài phạm vi. |
| LL-02 | Fake clock/ready không tiến; gọi Init/START/STOP | Thoát trong giới hạn time/poll, trả TIMEOUT; không báo mode thành công. |
| LL-03 | Write trước init, khi STOPPED; length 0/8/9, ID ngoài 11-bit | Chỉ frame hợp lệ ở STARTED được nhận; 0 byte không cần payload pointer; 9 byte bị từ chối. |
| LL-04 | Gửi PDU A rồi B chung HTH; phát completion A | B BUSY không callback; chỉ A có một callback đúng handle; B retry sau đó được nhận. |
| LL-05 | Sửa buffer nguồn ngay sau transmit OK; poison RX buffer sau callback | Payload TX đã copy giữ nguyên; dữ liệu RX deferred không trỏ vào storage hết lifetime. |
| LL-06 | Cùng HRH, hai CAN ID; inject ID không có route/length sai | Route tới đúng consumer hoặc drop + counter; không route chỉ bằng HRH. |
| LL-07 | RX direct và TP N-PDU với config owner khác nhau | Mỗi event tới đúng một owner; N-PDU không đi vòng qua PduR direct. |
| LL-08 | STOP/bus-off/timeout khi pending, rồi inject completion muộn | Mỗi request accepted có một terminal event; stale event không hoàn tất request mới. |
| LL-09 | Main interval sát timeout và `nowMs` rollover | Deadline đúng elapsed; không timeout sớm vì so timestamp tuyệt đối. |
| LL-10 | Queue RX đầy/MB overrun; TX completion cùng lúc | Báo loss/overflow; terminal TX vẫn được giữ và dispatch một lần. |
| LL-11 | Ring capacity 2/N; wrap, full, clear khi đã cô lập ISR | Sức chứa 1/N-1, FULL không ghi đè; byte `00 0A 0D FF` giữ nguyên. |
| LL-12 | Loopback rồi physical bus giữa ba firmware có local ID khác nhau | CAN ID/DLC/byte khớp golden vector; ghi config từng node, không đòi PDU/HTH giống nhau. |
| LL-13 | Chỉ node không phải đích ACK; đích không xử lý payload | Không biến CAN TX complete thành end-to-end SUCCESS. |

Log mỗi test ghi ID, input/config version, expected/actual, event sequence và kết quả; board log thêm firmware identity, wiring, bitrate, build configuration. Chỉ chuyển từ polling sang interrupt sau khi chứng minh cùng contract và không có callback trùng.

## 20. Refinement menu — quyết định chưa chốt

### ISSUE 1: RX object được giữ ở đâu trước khi CRC toàn object pass?

1. **Option A — Stream sang PC dưới dạng temporary transfer:** ECU gửi chunk ngay; PC chỉ commit file sau final SUCCESS/CRC. Pro: RAM ECU thấp. Con: PC protocol phải hỗ trợ discard file tạm.
2. **Option B — Giữ toàn bộ object trong ECU:** chỉ gửi UART sau CRC pass. Pro: PC protocol đơn giản. Con: không phù hợp ảnh lớn và RAM hữu hạn.
3. **Agent Rec:** Option A; giữ `Transfer_ReadRxData()` dạng streaming và bắt buộc final result packet.

### ISSUE 2: Một node hỗ trợ bao nhiêu transfer đồng thời?

1. **Option A — Một Tx và một Rx độc lập:** cho phép hai chiều đồng thời, state/buffer vẫn hữu hạn. Pro: đáp ứng hai chiều. Con: test concurrency phức tạp hơn single session.
2. **Option B — Chỉ một transfer toàn node:** đơn giản nhất. Pro: ít RAM/state. Con: PC hai bên phải serialize hoàn toàn.
3. **Agent Rec:** Option A, nhưng số slot phải là compile-time configuration.

### ISSUE 3: COM notification dùng callback hay application polling?

1. **Option A — Callback cấu hình tĩnh:** low latency, không scan signal table. Pro: rõ signal thay đổi. Con: callback phải ngắn và non-blocking.
2. **Option B — Application gọi `Com_ReceiveSignal()` định kỳ:** coupling thấp. Pro: flow đơn giản. Con: polling thừa và khó phản ứng nhanh.
3. **Agent Rec:** dùng callback cho control event và vẫn giữ `Com_ReceiveSignal()` để application đọc giá trị.

## 21. Thứ tự hiện thực API

1. Freeze common types/config tầng thấp, lifecycle/result/ownership và test vectors; thống nhất message matrix trước test nhiều board.
2. BSP/timebase và refactor CAN Driver API/callback, dùng test stub; test timeout/init/loopback có log.
3. CanIf, shared-HTH tracking và single-frame PDU tests.
4. PduR direct route tới test consumer; kiểm thử giao tiếp ba firmware khác nhau. Đây là trọng tâm hiện tại.
5. Khi yêu cầu signal đã rõ: COM control path.
6. Khi profile bulk được thống nhất: Transfer Service metadata/storage/stream contract, gồm RX session discovery và final result.
7. CanTp state machine, direct CanIf N-PDU callbacks và PduR SDU buffer callbacks.
8. UART Gateway framing/backpressure và PC temporary/commit protocol nếu chọn streaming.
9. Application nghiệp vụ và full end-to-end tests.

Không bắt đầu implementation CanTp trước khi `Can_Write() -> CanIf_TxConfirmation()` và `Can_MainFunction_Read() -> CanIf_RxIndication()` đã có deterministic test.

### 21.1 Góp ý chính của review 0.2

- Spec 0.1 trộn API tương lai với code đã có; mục 1 tách hiện trạng và ưu tiên tầng thấp, không buộc app phải hoàn thiện trước.
- Đường callback N-PDU ở 0.1 đi qua PduR, khác sơ đồ tạm; mục 6–8 và 17 chuyển về CanIf trực tiếp tới CanTp. PduR giữ routing I-PDU/SDU.
- Chỉ thêm swPduHandle chưa đủ: mục 8–9 bổ sung outstanding policy, payload lifetime, exactly-once terminal event và xử lý event muộn.
- Mode/result/config từng được tham chiếu nhưng chưa đủ contract; mục 9, 16 và 18 bổ sung baseline cùng gate còn lại trước coding.
- `Transfer_ReadRxData` thiếu nguồn RX transferId; streaming/CRC và ACK cuối chưa hoàn chỉnh. Mục 10 giữ các phần này deferred, ghi rõ việc cần quyết định.
- Ba ECU cần chung wire contract, không chung code/ID nội bộ; mục 16 và LL-12 kiểm tra điều này trực tiếp.
