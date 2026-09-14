# Bài tập: CAN Driver & Upper Layer — S32K144EVB

## 1. Bối cảnh

Trong hệ thống embedded automotive, CAN bus là giao thức giao tiếp phổ biến nhất. Trên **S32K144EVB**, phần cứng CAN được gọi là **FlexCAN**. Trong bài tập này, bạn sẽ viết một CAN software stack gồm hai tầng:

```text
+---------------------------------------+
|           Application (main.c)        |
+---------------------------------------+
|         CAN Upper Layer               |  <-- Bài tập phần 2
|  (CanUpper — quản lý PDU qua HOH)     |
+---------------------------------------+
|         CAN Driver Layer              |  <-- Bài tập phần 1
|  (Can — điều khiển FlexCAN register)  |
+---------------------------------------+
|         FlexCAN0 Hardware             |
+---------------------------------------+
```

Luồng xử lý end-to-end:

```text
Application
  -> CanUpper_Transmit()
  -> Can_Write()
  -> FlexCAN TX
  -> Can_MainFunction_Write()
  -> CanUpper_TxConfirmation()

FlexCAN RX
  -> Can_MainFunction_Read()
  -> CanUpper_RxIndication()
  -> [lưu vào RX buffer]
  -> CanUpper_GetRxData()
  -> Application
```

Điểm quan trọng của kiến trúc này:

```text
- Application chỉ biết PDU ID — không biết MB index
- CanUpper biết HOH ID và CAN ID của từng PDU — không biết register
- Can driver chỉ biết MB index và register — không biết PDU hay upper layer
- CAN ID là thuộc tính của PDU (upper layer), không phải của hardware object (driver)
```

Mục tiêu chính của bài là luyện:

```text
Đọc và hiểu datasheet register-level (FlexCAN)
Thiết kế API theo tư duy AUTOSAR
Tách biệt tầng driver và tầng upper layer qua HOH abstraction
Polling và interrupt-driven design
Modular C design
```

---

## 2. Thông tin bài tập

| Mục | Chi tiết |
|-----|----------|
| **Deadline** | 7 ngày sau buổi training |
| **Ngôn ngữ** | C **không** dùng HAL/SDK driver |
| **Target** | S32K144EVB, FlexCAN0, Standard CAN ID 11-bit, DLC 0–8 |
| **Tiêu chí** | Compile được + chạy được + **giải thích được thiết kế và luồng xử lý** |

---

## 3. Project structure bắt buộc

```text
can_stack/
├── driver/
│   ├── inc/
│   │   ├── Can.h           # Driver API declarations
│   │   ├── Can_Types.h     # Type definitions
│   │   └── Can_Cfg.h       # HOH ID definitions (extern declarations)
│   ├── src/
│   │   ├── Can.c           # Driver implementation
│   │   └── Can_Cfg.c       # HOH configuration table (definitions)
├── upper/
│   ├── inc/
│   │   ├── CanUpper.h      # Upper layer API declarations
│   │   ├── CanUpper_Types.h# Upper layer type definitions
│   │   └── CanUpper_Cfg.h  # PDU-to-HOH mapping definitions
│   └── src/
│       └── CanUpper.c      # Upper layer implementation
├── test/
│   └── main.c              # Demo TX/RX end-to-end
└── README.md               # Mô tả thiết kế, giải thích từng module
```

Không được viết toàn bộ logic trong `main.c`.

---

## 4. Khái niệm HOH — Hardware Object Handle

HOH là khái niệm trung tâm của bài tập này. Thay vì upper layer biết trực tiếp MB index nào trên FlexCAN, upper layer chỉ biết một **handle số nguyên** (HOH ID). Driver sẽ dùng HOH ID để tra cứu MB index thực tế.

```text
Application          CanUpper                Can Driver         FlexCAN HW
    |                    |                       |                   |
    | Transmit(PDU_TX_0) |                       |                   |
    |------------------->|                       |                   |
    |                    | Can_Write(HTH_0, pdu) |                   |
    |                    | pdu.id = 0x123        |                   |
    |                    |---------------------->|                   |
    |                    |                       | MB[0] TX trigger  |
    |                    |                       |------------------>|
    |                    |                       |    TX done        |
    |                    |                       |<------------------|
    |                    | TxConfirmation(HTH_0) |                   |
    |                    |<----------------------|                   |
```

Có hai loại HOH:

```text
HTH (Hardware Transmit Handle) : dùng cho TX, map đến một TX Message Buffer
HRH (Hardware Receive Handle)  : dùng cho RX, map đến một RX Message Buffer + ID filter
```

Phân biệt trách nhiệm:

```text
HTH chỉ biết MB nào để TX — không quan tâm CAN ID là bao nhiêu
HRH biết MB nào để RX và filter CAN ID nào — vì filter là cấu hình hardware
CAN ID khi TX là thuộc tính của PDU, do upper layer quyết định
```

Quy tắc đơn giản hóa trong bài này:

```text
Mỗi HTH chỉ được dùng bởi đúng một TX PDU
Mỗi HRH chỉ được dùng bởi đúng một RX PDU
```

---


## 5. Type definitions — `Can_Types.h`

Học viên định nghĩa các type sau. Đây là **gợi ý** — có thể điều chỉnh miễn là logic đúng.

### 5.1 Return type

```c
typedef enum {
    CAN_OK      = 0,   /* Thành công */
    CAN_NOT_OK  = 1,   /* Lỗi tham số hoặc trạng thái */
    CAN_BUSY    = 2    /* Message Buffer đang bận */
} Can_ReturnType;
```

### 5.2 Loại HOH

```c
typedef enum {
    CAN_HOH_TYPE_TX = 0,   /* Hardware Transmit Handle */
    CAN_HOH_TYPE_RX = 1    /* Hardware Receive Handle  */
} Can_HohType;
```

### 5.3 Cấu hình một HOH

```c
typedef struct {
    uint8_t      hohId;    /* Handle ID, dùng để tra cứu từ upper layer */
    Can_HohType  type;     /* TX hoặc RX */
    uint8_t      mbIndex;  /* Message Buffer index trên FlexCAN (0–15) */
    uint32_t     rxCanId;  /* Chỉ dùng cho RX: CAN ID filter (Standard 11-bit) */
    uint32_t     rxIdMask; /* Chỉ dùng cho RX: 0x7FF = exact match */
} Can_HohConfigType;
```

> HTH (TX): `rxCanId` và `rxIdMask` không có ý nghĩa, set = 0.
> HRH (RX): `rxCanId` và `rxIdMask` dùng để cấu hình ID filter cho MB.

### 5.4 Cấu hình toàn bộ CAN controller

```c
typedef struct {
    uint32_t                 baudrate;      /* Baud rate, ví dụ: 500000 */
    uint8_t                  loopbackEnable;/* 1 = bật Loopback Mode, 0 = normal */
    const Can_HohConfigType *hohList;       /* Trỏ đến mảng HOH config */
    uint8_t                  hohCount;      /* Số lượng HOH trong mảng */
} Can_ConfigType;
```

### 5.5 PDU — đơn vị dữ liệu truyền/nhận

```c
/* Macro giới hạn Standard CAN ID */
#define CAN_STANDARD_ID_MAX    0x7FFU   /* Max Standard 11-bit CAN ID */
#define CAN_STANDARD_ID_SHIFT  18U      /* Shift để encode vào MB register */

typedef struct {
    uint32_t        id;     /* CAN ID sẽ dùng khi TX (Standard 11-bit, max 0x7FF) */
    uint8_t         length; /* Số byte data, 0–8 */
    const uint8_t  *sdu;    /* Pointer đến data buffer (const: driver chỉ đọc) */
} Can_PduType;
```
`Can_PduType` chứa thông tin cần thiết để Driver tạo hoặc chuyển giao một CAN
frame. Không nhầm `Can_PduType.id` là PDU ID: trong bài tập này, trường `id`
là CAN ID xuất hiện trên bus.

### 5.6 Thông tin frame nhận được (dùng cho callback)

```c
typedef struct {
    uint32_t canId;   /* CAN ID của frame nhận được (đọc từ MB register) */
    uint8_t  hohId;   /* HRH ID của MB đã nhận frame */
} Can_HwType;
```

---

## 6. Cấu hình cố định — `Can_Cfg.h` và `Can_Cfg.c`

Cấu hình HOH được tách thành hai file để tránh tạo bản sao dữ liệu khi include.

### `Can_Cfg.h` — khai báo extern

```c
#ifndef CAN_CFG_H
#define CAN_CFG_H

#include "Can_Types.h"

/* ── HOH ID definitions ─────────────────────────────────────── */
#define CAN_HTH_0    0U   /* TX Handle: dùng MB[0] */
#define CAN_HRH_0    1U   /* RX Handle: nhận frame CAN ID 0x123, dùng MB[1] */

/* ── Extern declarations — định nghĩa thực tế nằm trong Can_Cfg.c ── */
extern const Can_HohConfigType Can_HohConfig[];
extern const Can_ConfigType    Can_Config;
extern const Can_ConfigType    Can_Config_Loopback;

#endif /* CAN_CFG_H */
```

### `Can_Cfg.c` — định nghĩa thực tế

```c
#include "Can_Cfg.h"

/*
 * HOH configuration table
 *   hohId    : handle number (dùng trong CanUpper)
 *   type     : TX hoặc RX
 *   mbIndex  : MB index vật lý trên FlexCAN0 (0–15)
 *   rxCanId  : chỉ dùng cho RX — CAN ID filter (Standard 11-bit)
 *   rxIdMask : chỉ dùng cho RX — 0x7FF = exact match
 *
 * HTH không cần rxCanId/rxIdMask vì CAN ID khi TX
 * do upper layer quyết định qua Can_PduType.id
 */
const Can_HohConfigType Can_HohConfig[] = {
    /* hohId       type              mbIndex  rxCanId  rxIdMask */
    { CAN_HTH_0,  CAN_HOH_TYPE_TX,  0U,      0x000U,  0x000U },  /* TX MB[0] */
    { CAN_HRH_0,  CAN_HOH_TYPE_RX,  1U,      0x123U,  0x7FFU },  /* RX MB[1], exact match 0x123 */
};

/* Normal mode */
const Can_ConfigType Can_Config = {
    .baudrate       = 500000U,
    .loopbackEnable = 0U,
    .hohList        = Can_HohConfig,
    .hohCount       = 2U,
};

/* Loopback mode */
const Can_ConfigType Can_Config_Loopback = {
    .baudrate       = 500000U,
    .loopbackEnable = 1U,
    .hohList        = Can_HohConfig,
    .hohCount       = 2U,
};
```

Cấu hình trên là ví dụ, học viên có thể thay đổi nếu muốn.

---

## 7. Phần 1 — CAN Driver Layer

### Mục tiêu

Viết module `Can` điều khiển trực tiếp thanh ghi FlexCAN0 trên S32K144. Không dùng bất kỳ HAL hay SDK nào.

---

### FR-01: `Can_Init()`

```c
void Can_Init(const Can_ConfigType *Config);
```

Khởi tạo toàn bộ FlexCAN0 dựa trên `Config`:

```text
1. Enable clock cho FlexCAN0 (PCC_FlexCAN0)
2. Đưa controller vào Freeze Mode (MCR[FRZ]=1, MCR[HALT]=1)
3. Cấu hình baud rate qua CTRL1 (PRESDIV, PSEG1, PSEG2, PROPSEG)
4. Nếu Config->loopbackEnable == 1: set CTRL1[LPB] = 1
   (phải thực hiện trong Freeze Mode, trước bước thoát Freeze)
5. Vô hiệu hóa tất cả 16 Message Buffers (set CODE = INACTIVE)
6. Duyệt hohList, với mỗi entry:
   - Nếu type == TX: cấu hình MB[mbIndex] là TX_INACTIVE
   - Nếu type == RX: cấu hình MB[mbIndex] là RX_EMPTY,
     ghi rxCanId vào MB[mbIndex].ID (Standard ID: shift left CAN_STANDARD_ID_SHIFT bit),
     ghi rxIdMask vào RXIMR[mbIndex] (shift left CAN_STANDARD_ID_SHIFT bit tương tự)
7. Thoát Freeze Mode (clear MCR[HALT])
8. Chờ MCR[FRZACK] = 0 để xác nhận đã thoát Freeze
```

Yêu cầu:

```text
- Nếu Config = NULL hoặc hohList = NULL → không làm gì, không crash
- Sau Can_Init(): controller sẵn sàng TX/RX theo đúng cấu hình HOH
- Loopback được set trong Freeze Mode — không set từ main.c
```

Lưu ý encode Standard ID vào register:

```c
/* Standard 11-bit ID phải được shift left 18 bit khi ghi vào MB.ID */
CAN0->RAMn[mbIndex * 4 + 1] = (rxCanId << CAN_STANDARD_ID_SHIFT) & 0x1FFFFFFFU;

/* RXIMR mask tương tự */
CAN0->RXIMR[mbIndex] = (rxIdMask << CAN_STANDARD_ID_SHIFT);
```

---

### FR-02: `Can_Write()`

```c
Can_ReturnType Can_Write(uint8_t HthId, const Can_PduType *PduInfo);
```

Load một CAN frame vào TX Message Buffer để truyền:

```text
1. Tìm HOH entry có hohId == HthId và type == TX trong hohList
2. Nếu không tìm thấy → return CAN_NOT_OK
3. Kiểm tra PduInfo != NULL
4. Kiểm tra PduInfo->id <= CAN_STANDARD_ID_MAX → CAN_NOT_OK nếu vượt quá
5. Kiểm tra PduInfo->length <= 8 → CAN_NOT_OK nếu vượt quá
6. Nếu length > 0: kiểm tra sdu != NULL → CAN_NOT_OK nếu NULL
7. Kiểm tra MB[mbIndex] có đang bận không (CODE == 0xC = TX_DATA_OR_REMOTE)
   → Nếu bận: return CAN_BUSY
8. Set CODE = TX_INACTIVE (0x8) để tạm dừng MB
9. Ghi PduInfo->id vào MB[mbIndex].ID (shift left CAN_STANDARD_ID_SHIFT bit)
10. Ghi data vào MB[mbIndex].WORD0 và WORD1
    (sdu[0] → CAN byte 0, sdu[1] → CAN byte 1, ..., sdu[7] → CAN byte 7)
11. Set CODE = TX_DATA_FRAME (0xC) và DLC = length để trigger TX
```

Return values:

```text
CAN_OK      : Frame đã được load, đang chờ TX
CAN_BUSY    : MB đang bận, thử lại sau
CAN_NOT_OK  : HthId không tồn tại hoặc tham số không hợp lệ
```

---

### FR-03: `Can_MainFunction_Write()`

```c
void Can_MainFunction_Write(void);
```

Polling-based TX confirmation. Gọi định kỳ từ vòng lặp chính:

```text
1. Duyệt hohList, với mỗi entry có type == TX:
   a. Kiểm tra IFLAG1 bit tương ứng với mbIndex
   b. Nếu bit set → TX hoàn tất:
      - Clear IFLAG1 bằng cách ghi 1 vào bit đó
      - Gọi CanUpper_TxConfirmation(hohId)
```

---

### FR-04: `Can_MainFunction_Read()`

```c
void Can_MainFunction_Read(void);
```

Polling-based RX processing. Gọi định kỳ từ vòng lặp chính:

```text
1. Duyệt hohList, với mỗi entry có type == RX:
   a. Kiểm tra IFLAG1 bit tương ứng với mbIndex
   b. Nếu bit set → có frame mới:
      - Đọc MB[mbIndex].CS để lấy DLC
      - Đọc MB[mbIndex].ID để lấy CAN ID (shift right CAN_STANDARD_ID_SHIFT bit)
      - Đọc MB[mbIndex].WORD0 và WORD1 để lấy data (đảo byte order về sdu[])
      - Đọc TIMER register để hoàn tất trình tự unlock MB
      - Clear IFLAG1 bằng cách ghi 1 vào bit đó
      - Điền Can_HwType: canId từ MB, hohId từ entry
      - Gọi CanUpper_RxIndication(&hwInfo, &pduInfo)
```

Lưu ý quan trọng:

```text
- Phải đọc TIMER sau khi đọc RX Message Buffer để hoàn tất trình tự unlock.
  Nếu không thực hiện bước này, MB có thể tiếp tục ở trạng thái locked
  và không nhận được frame tiếp theo cho đến khi thao tác unlock được hoàn tất.
- Clear IFLAG1 sau khi đọc xong, không phải trước.
```

---

### FR-05: `CAN0_ORed_Message_buffer_IRQHandler()`

```c
void CAN0_ORed_Message_buffer_IRQHandler(void);
```

Interrupt-driven thay thế cho polling. Logic giống `Can_MainFunction_Write/Read()` nhưng chạy trong ISR:

```text
TX path:
1. Duyệt TX HOH, kiểm tra IFLAG1
2. Clear IFLAG1 TRƯỚC khi gọi callback (tránh re-trigger)
3. Gọi CanUpper_TxConfirmation(hohId)

RX path:
4. Duyệt RX HOH, kiểm tra IFLAG1
5. Đọc MB data, đọc TIMER để unlock
6. Clear IFLAG1
7. Gọi CanUpper_RxIndication(&hwInfo, &pduInfo)
```

Lưu ý:

```text
- S32K144 IRQ number cho CAN0 MB
- ISR phải ngắn gọn, không có blocking operation bên trong
- Dùng #define CAN_USE_INTERRUPT để switch giữa polling và interrupt
```

---

## 8. Phần 2 — CAN Upper Layer

### Mục tiêu

Viết module `CanUpper` nằm phía trên `Can` driver. Module này thể hiện tính **abstraction**:

```text
- Không truy cập trực tiếp thanh ghi FlexCAN
- Không biết MB index — chỉ biết HOH ID
- Biết CAN ID của từng TX PDU — truyền xuống driver qua Can_PduType
- Mỗi TX PDU map đến đúng một HTH
- Mỗi RX PDU map đến đúng một HRH
- Lưu data nhận vào buffer, application tự poll khi cần
- Application chỉ cần biết PDU ID
```

Về cơ chế gọi callback từ driver lên upper layer:

```text
Driver (Can.c) được phép include CanUpper.h và gọi trực tiếp
CanUpper_TxConfirmation() và CanUpper_RxIndication().

Đây là simplification cho bài tập. Trong AUTOSAR thực tế,
driver không biết upper layer — callback được đăng ký qua
function pointer lúc init. Học viên nên ghi chú điều này
trong README.md.
```

---

### 8.1 Cấu hình PDU — `CanUpper_Cfg.h`

```c
#ifndef CANUPPER_CFG_H
#define CANUPPER_CFG_H

#include "Can_Cfg.h"

/* ── TX PDU IDs ─────────────────────────────────────────────── */
#define PDU_TX_LED_COMMAND     0U   /* Gửi lệnh điều khiển LED, CAN ID 0x123, dùng CAN_HTH_0 */

/* ── RX PDU IDs ─────────────────────────────────────────────── */
#define PDU_RX_LED_COMMAND     1U   /* Nhận lệnh LED, CAN ID 0x123, từ CAN_HRH_0 */

#endif /* CANUPPER_CFG_H */
```

---

### 8.2 Type definitions — `CanUpper_Types.h`

```c
#ifndef CANUPPER_TYPES_H
#define CANUPPER_TYPES_H

#include <stdint.h>

typedef uint8_t PduIdType;

/* Trạng thái TX của một PDU */
typedef enum {
    PDU_TX_IDLE    = 0,   /* Chưa có gì để gửi */
    PDU_TX_PENDING = 1,   /* Đang chờ TX */
    PDU_TX_DONE    = 2    /* TX đã hoàn tất */
} CanUpper_TxStatusType;

/* Một entry trong TX PDU table — mỗi PDU map đến đúng một HTH */
typedef struct {
    PduIdType             pduId;   /* PDU ID (từ CanUpper_Cfg.h) */
    uint32_t              canId;   /* CAN ID sẽ dùng khi TX frame này */
    uint8_t               hthId;   /* HTH ID map đến TX HOH trong driver */
    CanUpper_TxStatusType status;  /* Trạng thái TX hiện tại */
} CanUpper_TxPduType;

/* Một entry trong RX PDU table — mỗi PDU map đến đúng một HRH */
typedef struct {
    PduIdType  pduId;          /* PDU ID (từ CanUpper_Cfg.h) */
    uint8_t    hrhId;          /* HRH ID map đến RX HOH trong driver */
    uint8_t    data[8];        /* Buffer lưu data của frame nhận được */
    uint8_t    length;         /* Số byte nhận được */
    uint8_t    newDataFlag;    /* 1 = có data mới chưa đọc, 0 = chưa có */
} CanUpper_RxPduType;

#endif /* CANUPPER_TYPES_H */
```

Application tự poll bằng `CanUpper_GetRxData()` khi cần.

Luồng abstraction đầy đủ:

```text
TX:
  Application gọi CanUpper_Transmit(PDU_TX_LED_COMMAND, data, len)
  → CanUpper tra TX table → canId=0x123, hthId=CAN_HTH_0
  → Điền Can_PduType: id=0x123, length, sdu
  → Gọi Can_Write(CAN_HTH_0, &pdu)
  → Driver tra HOH config → MB[0]
  → Ghi pdu.id=0x123 vào MB register, trigger TX

RX:
  FlexCAN nhận frame CAN ID 0x123 vào MB[1] (HRH_0 filter match)
  → Driver đọc MB, gọi CanUpper_RxIndication(hohId=CAN_HRH_0, data)
  → CanUpper tra RX table → tìm entry có hrhId=CAN_HRH_0
  → Copy data vào entry->data[], set newDataFlag = 1
  → Application gọi CanUpper_GetRxData(PDU_RX_LED_COMMAND, ...) để đọc
```

---

### 8.3 API Upper Layer

#### FR-06: `CanUpper_Init()`

```c
void CanUpper_Init(const Can_ConfigType *CanConfig);
```

```text
1. Gọi Can_Init(CanConfig) để khởi tạo driver bên dưới
2. Reset toàn bộ TX PDU table về trạng thái IDLE
3. Reset toàn bộ RX PDU table: length=0, newDataFlag=0
```

---

#### FR-07: `CanUpper_Transmit()`

```c
Can_ReturnType CanUpper_Transmit(PduIdType TxPduId, const uint8_t *data, uint8_t length);
```

```text
1. Kiểm tra data != NULL nếu length > 0 → CAN_NOT_OK nếu vi phạm
2. Tìm entry trong TX PDU table theo TxPduId
3. Nếu không tìm thấy → return CAN_NOT_OK
4. Nếu status == PDU_TX_PENDING → return CAN_BUSY
5. Điền Can_PduType: id = entry->canId, length, sdu = data
6. Gọi Can_Write(entry->hthId, &pdu)
7. Nếu CAN_OK → set status = PDU_TX_PENDING
8. Return kết quả của Can_Write
```

---

#### FR-08: `CanUpper_TxConfirmation()` — Callback từ Driver

```c
void CanUpper_TxConfirmation(uint8_t HthId);
```

```text
1. Tìm entry trong TX PDU table có hthId == HthId
2. Set status = PDU_TX_DONE
```

---

#### FR-09: `CanUpper_RxIndication()` — Callback từ Driver

```c
void CanUpper_RxIndication(const Can_HwType *Mailbox, const Can_PduType *PduInfoPtr);
```

```text
1. Duyệt RX PDU table
2. Tìm entry có hrhId == Mailbox->hohId
3. Nếu tìm thấy:
   - Copy PduInfoPtr->sdu vào entry->data[] (tối đa 8 byte)
   - Set entry->length = PduInfoPtr->length
   - Set entry->newDataFlag = 1
```

---

#### FR-10: `CanUpper_GetTxStatus()`

```c
CanUpper_TxStatusType CanUpper_GetTxStatus(PduIdType TxPduId);
```

```text
1. Tìm entry trong TX PDU table theo TxPduId
2. Nếu tìm thấy → return entry->status
3. Nếu không tìm thấy → return PDU_TX_IDLE
```

---

#### FR-11: `CanUpper_GetRxData()`

```c
Can_ReturnType CanUpper_GetRxData(PduIdType RxPduId, uint8_t *dataOut, uint8_t *lengthOut);
```

```text
1. Kiểm tra dataOut != NULL và lengthOut != NULL → CAN_NOT_OK nếu vi phạm
2. Tìm entry trong RX PDU table theo RxPduId
3. Nếu không tìm thấy → return CAN_NOT_OK
4. Nếu entry->newDataFlag == 0 → return CAN_NOT_OK (chưa có data mới)
5. Copy entry->data[] vào dataOut
6. Set *lengthOut = entry->length
7. Clear entry->newDataFlag = 0
8. Return CAN_OK
```

Application có thể dùng như sau:

```c
uint8_t rxBuf[8];
uint8_t rxLen;

while (1) {
    CanUpper_MainFunction();

    if (CanUpper_GetRxData(PDU_RX_LED_COMMAND, rxBuf, &rxLen) == CAN_OK) {
        /* Có data mới — xử lý tại đây */
    }
}
```

---

#### FR-12: `CanUpper_MainFunction()`

```c
void CanUpper_MainFunction(void);
```

```text
1. Gọi Can_MainFunction_Write()
2. Gọi Can_MainFunction_Read()
```

Khi dùng interrupt mode (`#define CAN_USE_INTERRUPT`), hàm này có thể để trống.

---

## 9. Coding rules

```text
1. Không viết toàn bộ logic trong main.c
2. Can_Cfg.c là nơi duy nhất chứa giá trị cụ thể về MB index và RX filter ID
3. CanUpper_Cfg.h là nơi duy nhất chứa CAN ID của từng TX PDU
4. Mọi pointer input phải kiểm tra NULL trước khi dereference
5. Không dùng magic number — dùng #define hoặc enum
6. Mỗi function chỉ làm một việc rõ ràng
7. Tên hàm theo convention: Module_Action(), ví dụ Can_Write(), CanUpper_Transmit()
8. Comment giải thích tại sao, không chỉ giải thích cái gì
9. README.md phải giải thích tại sao driver gọi trực tiếp CanUpper thay vì
   dùng function pointer, và sự khác biệt so với AUTOSAR thực tế
```

---

## 10. Test scenarios

### TC-001 — Khởi tạo và kiểm tra HOH setup

**Mục đích:** Verify `Can_Init()` setup đúng từng MB theo `Can_HohConfig`.

**Setup:** Một board, không cần kết nối bus.

**Steps:**

```text
1. Gọi Can_Init(&Can_Config)
2. Verify MCR[FRZACK] = 0 (đã thoát Freeze Mode)
3. Verify MB[0] (HTH_0): CS.CODE = TX_INACTIVE (0x8)
4. Verify MB[1] (HRH_0): CS.CODE = RX_EMPTY (0x4),
   RXIMR[1] = (0x7FF << 18), MB[1].ID = (0x123 << 18)
5. Gọi Can_Write(CAN_HTH_0, NULL) → expect CAN_NOT_OK, không crash
6. Gọi Can_Write(CAN_HTH_0, &pdu) với pdu.id = 0x800 → expect CAN_NOT_OK
```

**Pass criteria:**

```text
- MB được cấu hình đúng theo HOH config
- NULL input và ID vượt 0x7FF đều bị reject
```

---

### TC-002 — Loopback self-test TX→RX (một board)

**Mục đích:** Test end-to-end TX→RX trên một board duy nhất bằng Loopback Mode.

**Setup:** Một board, không cần kết nối bus ngoài.

**Steps:**

```text
1. Gọi CanUpper_Init(&Can_Config_Loopback)
   (Can_Init() sẽ tự set CTRL1[LPB]=1 vì loopbackEnable=1)
2. Gọi CanUpper_Transmit(PDU_TX_LED_COMMAND,
                          data=[0xDE,0xAD,0xBE,0xEF], length=4)
3. Verify return = CAN_OK
4. Verify CanUpper_GetTxStatus(PDU_TX_LED_COMMAND) = PDU_TX_PENDING
5. Gọi CanUpper_MainFunction() trong vòng lặp tối đa 100ms
6. Verify CanUpper_GetTxStatus(PDU_TX_LED_COMMAND) = PDU_TX_DONE
7. Gọi CanUpper_GetRxData(PDU_RX_LED_COMMAND, rxBuf, &rxLen)
8. Verify return = CAN_OK
9. Verify rxLen = 4
10. Verify rxBuf = {0xDE, 0xAD, 0xBE, 0xEF}
```

**Pass criteria:**

```text
- Status chuyển IDLE → PENDING → DONE đúng thứ tự
- Data nhận lại khớp 100% với data gửi đi (kiểm tra byte order)
- Không cần node thứ hai
```

> **Tip:** Bắt đầu từ TC-002 — test được ngay trên board mà không cần thiết bị thêm.

---

### TC-003 — LED Control giữa hai board (có logic analyzer)

**Mục đích:** Verify TX/RX hoạt động đúng trên CAN bus thực. Board A dùng button để gửi lệnh điều khiển LED sang Board B qua CAN.

**Setup:**

```text
- Hai board S32K144EVB kết nối CAN bus (CANH–CANH, CANL–CANL, GND chung)
- Logic analyzer kẹp vào CANH/CANL để quan sát frame
- Board A: nhận input từ button SW2 (PTD3), gửi lệnh LED qua CAN ID 0x123
- Board B: nhận frame CAN ID 0x123, giải mã lệnh, bật LED tương ứng
- Cả hai board dùng Can_Config (loopbackEnable=0)
```

**Giao thức lệnh LED:**

```text
DLC = 3 bytes cố định
Byte[0] : Magic byte = 0xCA  (để phân biệt frame hợp lệ)
Byte[1] : Command code
          0x01 = Bật LED xanh  (PTD0)
          0x02 = Bật LED đỏ    (PTD15)
          0x03 = Bật LED vàng  (PTD16)
          0x00 = Tắt tất cả LED
Byte[2] : Sequence number (tăng dần mỗi lần nhấn, dùng để debug)
```

**Logic Board A — xử lý button:**

```text
- Mỗi lần nhấn SW2: tăng biến đếm press_count (1→2→3→4→1→...)
- press_count == 1 → gửi lệnh 0x01 (LED xanh)
- press_count == 2 → gửi lệnh 0x02 (LED đỏ)
- press_count == 3 → gửi lệnh 0x03 (LED vàng)
- press_count == 4 → gửi lệnh 0x00 (tắt tất cả)
```

**Steps:**

```text
Board A:
1. Gọi CanUpper_Init(&Can_Config)
2. Vòng lặp chính: poll button SW2 + gọi CanUpper_MainFunction()
3. Nhấn SW2 lần 1:
   - Gửi data=[0xCA, 0x01, 0x01], length=3
   - Verify CanUpper_GetTxStatus(PDU_TX_LED_COMMAND) = PDU_TX_DONE
   - Logic analyzer: thấy frame ID=0x123, data=CA 01 01

Board B:
4. Gọi CanUpper_Init(&Can_Config)
5. Vòng lặp: gọi CanUpper_MainFunction()
6. Gọi CanUpper_GetRxData(PDU_RX_LED_COMMAND, rxBuf, &rxLen)
7. Nếu CAN_OK và rxBuf[0]==0xCA:
   - rxBuf[1]=0x01 → bật LED xanh (PTD0 = LOW)
8. Verify LED xanh sáng trên Board B

Board A (tiếp):
9.  Nhấn SW2 lần 2 → data=[0xCA, 0x02, 0x02]
    Board B: rxBuf[1]=0x02 → tắt LED xanh, bật LED đỏ (PTD15 = LOW)
10. Nhấn SW2 lần 3 → data=[0xCA, 0x03, 0x03]
    Board B: rxBuf[1]=0x03 → tắt LED đỏ, bật LED vàng (PTD16 = LOW)
11. Nhấn SW2 lần 4 → data=[0xCA, 0x00, 0x04]
    Board B: rxBuf[1]=0x00 → tắt tất cả LED
```

**Pass criteria:**

```text
- Mỗi lần nhấn button: TX confirmed, frame xuất hiện trên logic analyzer
- Board B giải mã đúng lệnh, LED sáng/tắt đúng theo sequence
- Byte[0]=0xCA được kiểm tra — frame không có magic byte bị bỏ qua
- Sequence number tăng đúng thứ tự trên logic analyzer
```

---

### TC-004 — Robustness: tham số không hợp lệ

**Mục đích:** Verify driver và upper layer không crash với input lỗi.

**Setup:** Một board, không cần kết nối bus.

**Lưu ý:** Gọi `CanUpper_MainFunction()` để chờ TX done trước mỗi `Can_Write()` tiếp theo.

**Steps:**

```text
1. Can_Write(CAN_HTH_0, NULL)                              → expect CAN_NOT_OK
2. Can_Write(CAN_HTH_0, &pdu) với length=9                 → expect CAN_NOT_OK
3. Can_Write(CAN_HTH_0, &pdu) với id=0x800 (> 0x7FF)       → expect CAN_NOT_OK
4. Can_Write(99U, &pdu) — HTH ID không tồn tại             → expect CAN_NOT_OK
5. Can_Init(NULL)                                          → expect không crash
6. CanUpper_Transmit(99U, data, 4) — PDU không tồn tại     → expect CAN_NOT_OK
7. CanUpper_Transmit(PDU_TX_LED_COMMAND, NULL, 4)          → expect CAN_NOT_OK
   [Chờ TX done nếu cần trước step 8]
8. Can_Write(CAN_HTH_0, &pdu) với length=0                 → expect CAN_OK
   [Chờ TX done trước step 9]
9. Can_Write(CAN_HTH_0, &pdu) với length=8                 → expect CAN_OK
```

**Pass criteria:**

```text
- Steps 1–7: trả về error code, không crash, không gửi frame
- Steps 8–9: trả về CAN_OK, frame được gửi đúng
```

---

## 11. Tiêu chí đánh giá

| Tiêu chí | Điểm | Mô tả |
|----------|------|-------|
| Compile không lỗi | 15% | Build thành công trên S32 Design Studio |
| Loopback TX/RX hoạt động | 25% | TC-002 pass, byte order đúng |
| LED control 2 board | 20% | TC-003 pass — button → CAN → LED đúng |
| Robustness | 15% | TC-004 pass |
| Code quality | 15% | Naming convention, no magic numbers, comments |
| Giải thích được thiết kế và luồng xử lý | 20% | Giải thích được HOH abstraction và luồng TX/RX end-to-end |

---

## 12. Ý nghĩa bài tập

Bài này không chỉ để học CAN. Đây là bài mô phỏng kiến trúc phần mềm automotive thực tế. Trong AUTOSAR, `Can` driver và `CanIf` (CAN Interface) tách biệt hoàn toàn qua cơ chế HOH — driver chỉ biết hardware object, upper layer biết PDU và CAN ID. Tư duy abstraction này áp dụng cho mọi giao thức: LIN, SPI, UART, Ethernet.

---

## 13. Kết quả mong đợi

Sau bài này, học viên cần nắm được:

```text
1. Cách cấu hình FlexCAN0 ở mức register (Freeze Mode, baud rate, MB)
2. Khái niệm HOH: HTH chỉ quản lý MB, HRH quản lý MB + ID filter
3. Tại sao CAN ID khi TX là thuộc tính của PDU (upper layer), không phải HOH (driver)
4. Cách encode Standard CAN ID vào MB register (shift left 18 bit)
5. Cách xử lý byte order khi ghi/đọc của FlexCAN
6. Cách unlock Message Buffer sau khi đọc RX (đọc TIMER)
7. Cách CanUpper lưu data nhận vào buffer và application poll bằng CanUpper_GetRxData()
8. Cách dùng loopbackEnable trong config để switch giữa test mode và normal mode
```
