# API Specification cho Mock Multi-ECU Project

## 1. Document control

| Thuộc tính | Giá trị |
|---|---|
| Tên tài liệu | Mock Multi-ECU Module API Specification |
| Trạng thái | **DRAFT — chưa dùng làm contract implementation chính thức** |
| Phiên bản | 0.1 |
| Nguồn yêu cầu | [`requirements/README.md`](README.md) |
| Phạm vi | ECU1, ECU2, ECU3; Classic CAN; UART gateway tại ECU1 và ECU3 |

Tài liệu này liệt kê API dự kiến cho từng module và giải thích lý do cần từng API. Đây là thiết kế tối giản lấy cảm hứng từ AUTOSAR, không phải bản sao API AUTOSAR đầy đủ.

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

### 2.2 Ngoài phạm vi phiên bản 0.1

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
- Pointer input chỉ hợp lệ trong thời gian lời gọi, trừ khi API ghi rõ dữ liệu đã được copy hoặc ownership đã được chuyển.
- Module dưới không được include header application. Callback lên trên phải đi qua interface được định nghĩa tại đây.
- Milestone đầu dùng CAN polling. Khi chuyển sang interrupt, driver phải queue event để upper-layer callback vẫn chạy ở main context.

## 3. Common communication types

Các type này nên đặt trong một header dùng chung, ví dụ `communication/common/Comm_Types.h`.

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
    COMM_NOT_FOUND
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

ECU2 là CAN participant/diagnostic node trong baseline; không relay bulk frame.

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
| `PduR_CanIfRxIndication()` **NEW** | CanIf | Phân phối single-frame PDU tới COM hoặc transport consumer theo routing table. |
| `PduR_CanIfTxConfirmation()` **NEW** | CanIf | Chuyển confirmation về đúng COM hoặc CanTp source. |
| `PduR_CanTpStartOfReception()` **NEW** | CanTp | Hỏi Transfer Service có chấp nhận object và có capacity trước khi nhận payload. |
| `PduR_CanTpCopyRxData()` **NEW** | CanTp | Stream segment vào Transfer Service mà PduR không sở hữu buffer ảnh. |
| `PduR_CanTpRxIndication()` **NEW** | CanTp | Báo kết thúc thành công/thất bại để Transfer Service commit hoặc hủy object. |
| `PduR_CanTpCopyTxData()` **NEW** | CanTp | Cấp đúng chunk kế tiếp từ Transfer Service khi CanTp sẵn sàng gửi. |
| `PduR_CanTpTxConfirmation()` **NEW** | CanTp | Kết thúc transfer Tx và giải phóng source state đúng thời điểm. |

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
| `CanTp_RxIndication()` **NEW** | PduR từ CanIf route | Nhận Single/First/Consecutive/Flow-Control frame và validate sequence. |
| `CanTp_TxConfirmation()` **NEW** | PduR từ CanIf route | Chỉ gửi frame kế tiếp sau khi frame hiện tại đã được CAN xác nhận. |
| `CanTp_GetStats()` **NEW** | Status/diagnostic | Cung cấp timeout, sequence error, abort và byte/frame counter cho test evidence. |

`CanTp_Transmit()` không nhận pointer tới toàn bộ ảnh vì thiết kế mục tiêu là streaming, không giữ toàn bộ object trong RAM.

## 8. CanIf module — CAN abstraction

CanIf sở hữu mapping `TxPduId -> CAN ID/HTH/controller` và `HRH + CAN ID -> RxPduId`. Đây là phần cần tách khỏi `CanUpper.c`.

```c
Comm_ReturnType CanIf_Init(const CanIf_ConfigType *config);
Comm_ReturnType CanIf_SetControllerMode(uint8_t controllerId,
                                        Can_ControllerModeType mode);
Comm_ReturnType CanIf_Transmit(PduIdType txPduId,
                               const PduInfoType *pduInfo);
Comm_ReturnType CanIf_GetStats(CanIf_StatsType *statsOut);

void CanIf_TxConfirmation(PduIdType swPduHandle,
                          Comm_ReturnType result);
void CanIf_RxIndication(const Can_HwType *mailbox,
                        const PduInfoType *pduInfo);
void CanIf_ControllerBusOff(uint8_t controllerId);
void CanIf_ControllerModeIndication(uint8_t controllerId,
                                    Can_ControllerModeType mode);
```

| API | Caller | Vì sao cần |
|---|---|---|
| `CanIf_Init()` **NEW** | App init | Validate PDU/CAN-ID/HOH mapping cho node hiện tại trước khi CAN bắt đầu. |
| `CanIf_SetControllerMode()` **NEW** | App/state manager | Start/stop controller mà upper layer không gọi CAN Driver trực tiếp. |
| `CanIf_Transmit()` **NEW** | PduR | Biến logical TxPduId thành `Can_PduType`, thêm CAN ID/HTH/software handle rồi gọi `Can_Write()`. |
| `CanIf_GetStats()` **NEW** | Status/diagnostic | Đếm unknown PDU, invalid length, mapping miss và driver busy. |
| `CanIf_TxConfirmation()` **NEW** | CAN Driver | Dùng software PDU handle để xác định đúng TxPduId, kể cả nhiều PDU dùng chung HTH. |
| `CanIf_RxIndication()` **NEW** | CAN Driver | Dùng HRH + CAN ID để tìm RxPduId, rồi chuyển payload lên PduR. |
| `CanIf_ControllerBusOff()` **NEW** | CAN Driver | Đưa lỗi bus-off lên application/state handling thay vì để driver tự phục hồi âm thầm. |
| `CanIf_ControllerModeIndication()` **OPTIONAL** | CAN Driver | Xác nhận mode transition bất đồng bộ nếu start/stop không hoàn tất ngay. |

## 9. CAN Driver — FlexCAN hardware access

CAN Driver được refactor từ `can_task/driver`. Driver không include `CanUpper`, PduR hoặc application header.

### 9.1 Type bắt buộc

```c
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

### 9.2 API

```c
Can_ReturnType Can_Init(const Can_ConfigType *config);
Can_ReturnType Can_SetControllerMode(uint8_t controllerId,
                                     Can_ControllerModeType mode);
Can_ReturnType Can_Write(Can_HwHandleType hth,
                         const Can_PduType *pduInfo);
void Can_MainFunction_Write(void);
void Can_MainFunction_Read(void);
void Can_MainFunction_Error(void);
Can_ReturnType Can_GetControllerStatus(uint8_t controllerId,
                                       Can_ControllerStatusType *statusOut);
```

| API | Trạng thái | Vì sao cần |
|---|---|---|
| `Can_Init()` | **MODIFY** từ API hiện có | Giữ trình tự FlexCAN init nhưng phải trả lỗi cho invalid config, clock/freeze timeout hoặc hardware không ready. |
| `Can_SetControllerMode()` | **NEW** | Tách init configuration khỏi start/stop communication; cần cho reset và bus-off recovery. |
| `Can_Write()` | **MODIFY** | Giữ validation/pack/busy check hiện có, đồng thời lưu `swPduHandle` cho confirmation chính xác. |
| `Can_MainFunction_Write()` | **EXISTING** | Poll TX completion và gọi `CanIf_TxConfirmation()` trong main context. |
| `Can_MainFunction_Read()` | **EXISTING** | Poll RX Message Buffer và gọi `CanIf_RxIndication()` trong main context. |
| `Can_MainFunction_Error()` | **NEW** | Poll bus-off/error state và phát event có kiểm soát. |
| `Can_GetControllerStatus()` | **NEW** | Cung cấp mode/error counters cho test và status mà không đọc register ngoài driver. |

Driver-to-CanIf callback là một phần contract của mục 8; driver không tự lưu application payload dài.

## 10. Transfer Service — object/session ownership

Transfer Service quản lý metadata, CRC, byte count và lifecycle của text/image/raw object. CanTp chỉ quản lý transport frame.

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

Các API hiện có trong [`Driver_UART.h`](../drivers/uart/Driver_UART.h) đủ cho baseline interrupt-driven gateway.

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

Các file `node_cfg`, `com_cfg`, `pdur_cfg`, `cantp_cfg`, `canif_cfg` và `can_cfg` chủ yếu export `const` configuration; không cần setter runtime trong baseline.

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

Mỗi entry phải truy vết được chuỗi:

```text
Signal/Object
  -> Source PDU
  -> PduR route
  -> CanTp SDU/N-PDU hoặc direct CanIf PDU
  -> CAN ID
  -> HTH/HRH
  -> FlexCAN Message Buffer
```

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
  -> PduR_CanIfRxIndication()
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

## 18. Init order

`GatewayApp_Init()` phải gọi theo thứ tự:

1. `BoardCan_Init()`.
2. `Driver_SysTick_Init()`.
3. `Can_Init()` ở trạng thái STOPPED.
4. `CanIf_Init()`.
5. `PduR_Init()`.
6. `Com_Init()`.
7. `CanTp_Init()`.
8. `Transfer_Init()`.
9. Init ring buffers.
10. `UartGateway_Init()` và đăng ký UART callbacks.
11. `LPUART1_Init()`.
12. `CanIf_SetControllerMode(..., CAN_CONTROLLER_STARTED)`.

Thứ tự này bảo đảm không có RX/TX interrupt hoặc CAN frame đi vào một upper module chưa init.

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

1. Common types và configuration types.
2. Refactor CAN Driver API/callback.
3. CanIf và single-frame PDU tests.
4. PduR direct route.
5. COM control path.
6. Transfer Service storage/stream contract.
7. CanTp state machine và PduR buffer callbacks.
8. UART Gateway.
9. Application lifecycle và full end-to-end tests.

Không bắt đầu implementation CanTp trước khi `Can_Write() -> CanIf_TxConfirmation()` và `Can_MainFunction_Read() -> CanIf_RxIndication()` đã có deterministic test.
