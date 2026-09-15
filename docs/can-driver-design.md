# Thiết kế CAN driver cho Part 1

Ngày: 2026-09-15. Role: SOLUTION ARCHITECT. Đây là đề xuất để viết driver mới, chưa phải implementation đã chạy.

## Cập nhật implementation CAN0 theo yêu cầu mới

Sau lượt thiết kế, người dùng yêu cầu hardcode CAN0 và hoàn thiện Can.c/h. Driver hiện đã có bốn API với profile cố định: oscillator 8 MHz, 500 kbit/s, standard Classical data frames, Tx MB8/HTH0 và Rx MB9/HRH1. Config tables không được dùng; các mục thiết kế config-driven/multi-controller phía dưới là hướng mở rộng, không mô tả runtime hiện tại.

Đã thực hiện Tx snapshot trước CAN_OK, lưu swPduHandle, release trước confirmation, Rx HRH + CAN ID, bounded waits và structured debugger logs. Bus-off/fatal fault latch ERROR và request Freeze; Init lại sau khi sửa nguyên nhân reset CAN0 và bỏ request lỗi không success-confirm. CanIf production callbacks còn thiếu. [Tests mới](../tests/can_driver/README.md) đạt 9 nhóm deterministic tests và ARM Cortex-M4 compile; chưa full firmware link hoặc board test. Các mô tả placeholder/khác return type/chưa tests trong phần khảo sát dưới đây là hiện trạng tại thời điểm thiết kế trước implementation.

Refactor tiếp theo: production Can.c bỏ hai macro MMIO và generic modify-register helper, dùng thanh ghi trực tiếp. Can_Init gọi các static helpers cho disable/chọn clock, enable, enter Freeze, reset, controller profile, MB/mask và exit Freeze; helper giữ timeout/log stage IDs. Host tests tạo bản sao instrumented trong build directory, không cần test wrappers trong production source; original Can.c vẫn được compile ARM riêng. Đã đạt lại 9 nhóm regression tests.

## Nguồn và hiện trạng

- [Assignment](../requirements/assignment_part1_com_signal.md), mục 4, 19–27, 29, 33 và 36: nguồn yêu cầu chính.
- [Architecture notes](../requirements/part1_architecture_notes.md), mục 18–29: giải thích ownership và polling.
- [Can.h](../drivers/can/can_driver/Can.h) khai báo bốn API; Can.c mới có Can_Init rỗng và hai forward declarations. Header trả Can_ReturnType nhưng definition hiện trả void: cần thống nhất khi triển khai.
- [Can_Types.h](../drivers/can/can_driver/Can_Types.h) đã có Tx/Rx PDU, controller và HOH types. [Can_cfg.c](../drivers/can/can_driver/Can_cfg.c) có CAN0/CAN1 và HOH 0..3, nhưng chưa gán baudRate nên giá trị khởi tạo bằng 0.
- [BSP](../bsp/can/board_can.c) mới chuẩn bị clock gate/pin/transceiver cho CAN0. Không coi CAN1 đã sẵn sàng chỉ vì có config entry.

## Ranh giới module

CanDrv sở hữu controller, HOH, physical Message Buffer và trạng thái request phần cứng. CanIf sở hữu TxPduId → CAN ID + HTH và HRH + CAN ID → RxPduId. COM sở hữu packing Signal Slot, Update Bits, period/offset, pending và max_retries. Driver chuyển nguyên thứ tự byte payload, không thêm GlobalPduId vào frame.

Với mapping hiện có:

| Handle | Loại | Controller | Physical MB |
|---|---|---|---|
| 0 | HTH | CAN0 | 8 |
| 1 | HRH | CAN0 | 9 |
| 2 | HTH | CAN1 | 8 |
| 3 | HRH | CAN1 | 9 |

HOH phải unique trên toàn instance. Cùng MB index ở hai controller là hai tài nguyên khác nhau. Với model một HOH ứng với một MB, không được gán hai HOH vào cùng cặp controller/MB.

## Can.h: API và contract

Giữ bốn API hiện có cho static configuration và polling:

```c
Can_ReturnType Can_Init(void);
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);
void Can_MainFunction_Write(void);
void Can_MainFunction_Read(void);
```

Can.h cần Can_Types.h. Can_Cfg.h có thể được include trong Can.c; caller muốn dùng symbolic HTH/HRH có thể include Can_Cfg.h trực tiếp. Assignment không bắt buộc Can_Init nhận config pointer hoặc có API START riêng.

Đề xuất lifecycle tối giản: Can_Init thành công thì mọi controller được cấu hình đều sẵn sàng; thất bại trả CAN_NOT_OK, không công bố driver READY và không nhận Tx/Rx. Nếu một controller thất bại sau khi controller khác đã được cấu hình, đưa các controller đã chạm tới về trạng thái an toàn. Từ chối gọi Init lại khi driver đang READY để tránh làm mất request đang gửi. Đây là lựa chọn thiết kế, không phải rule riêng của assignment.

Callbacks thuộc interface CanIf, không phải public API của CanDrv. Contract đề xuất để thống nhất khi viết CanIf:

```c
void CanIf_TxConfirmation(PduIdType TxPduId);
void CanIf_RxIndication(Can_HwHandleType Hrh, const Can_RxPduType *RxPdu);
```

Rx signature theo assignment mục 21; Tx parameter là đề xuất dùng swPduHandle hiện có. Không truyền HTH thay cho TxPduId. Hiện repo chưa có CanIf header/implementation; không tạo callback rỗng để che thiếu tích hợp.

## Can.c: trạng thái và luồng xử lý

### Trạng thái nội bộ

- Driver: UNINIT / READY / ERROR.
- Controller: trạng thái sẵn sàng/lỗi, phần cứng tương ứng với instance.
- Tx HOH: IDLE / BUSY / ERROR và swPduHandle của request đã nhận. Runtime lưu theo config index; lookup tìm objectId/controllerId thật, không mặc định ID bằng index.
- Rx: buffer byte do driver sở hữu trong thời gian callback đồng bộ.

Model ban đầu: một request đang xử lý trên mỗi Tx HOH; không có hàng đợi phần mềm. Can_Write và hai polling functions chạy tuần tự trong main context, không cho IRQ cùng xử lý các MB đó. Nếu thêm concurrency, phải bảo vệ thao tác nhận request và completion.

### Can_Init

1. Validate toàn bộ bảng trước khi truy cập phần cứng: controller ID/instance hợp lệ và không trùng; HOH ID unique, loại TX/RX hợp lệ, ControllerRef tồn tại, MB nằm trong phạm vi chế độ đã chọn và không xung đột tài nguyên.
2. Validate cấu hình clock/bit timing, frame format và Rx acceptance policy. Baudrate 0 hoặc profile chưa hỗ trợ phải báo lỗi.
3. Với từng controller: cấu hình CAN peripheral và các HOH thuộc controller đó, reset runtime, xử lý cờ cũ rồi đưa controller sang chế độ đã chọn. Mọi hardware wait phải có giới hạn.
4. Chỉ công bố READY khi toàn bộ cấu hình thành công; ghi sự kiện init thành công hoặc lỗi có controller/stage/reason.

### Can_Write

1. Validate READY, HTH tồn tại và là TX, controller sẵn sàng, PduInfo hợp lệ, CAN ID/frame format hợp lệ, length trong giới hạn và sdu khác NULL khi length > 0.
2. Resolve HTH → HOH → controller → MB. Tài nguyên còn giữ request trước thì trả CAN_BUSY ngay, kể cả hardware đã xong nhưng polling chưa giải quyết completion cũ.
3. Sao chép payload vào tài nguyên do driver/phần cứng sở hữu, giữ swPduHandle và nhận request một cách nhất quán; sau đó trả CAN_OK. Không giữ PduInfo/sdu để đọc muộn.
4. CAN_NOT_OK dùng cho input/config/state lỗi; CAN_BUSY dùng cho tài nguyên Tx đang bận. Cả hai đều không nhận request, không ghi đè dữ liệu/handle của request trước.

Sao chép trước khi trả CAN_OK bảo vệ frame đã nhận khi COM xóa U hoặc cập nhật signal mới. Driver không xóa U, không retry trong vòng lặp và không đợi frame phát xong. CAN_OK chỉ là accepted.

### Can_MainFunction_Write

Duyệt Tx HOH, resolve đúng controller và nhận diện kết quả của request BUSY. Với completion thành công: lưu handle cũ, xử lý cờ completion, trả tài nguyên về IDLE rồi gọi CanIf_TxConfirmation(handle cũ) đúng một lần. Giải phóng trước callback cho phép callback gửi request mới mà không bị ghi đè state. Không confirm request chưa từng được nhận, frame bị abort hoặc hardware error. Lỗi phải được ghi nhận; chỉ trả IDLE sau khi xác nhận tài nguyên an toàn. COM max_retries không phải cơ chế recovery cho request đã được CAN_OK.

### Can_MainFunction_Read

Duyệt Rx HOH và controller tương ứng. Khi có frame hợp lệ: lấy CAN ID, length và bản sao payload; hoàn tất thao tác phần cứng theo manual; gọi CanIf_RxIndication(objectId, &RxPdu). CanIf tiếp tục lookup HRH + CAN ID.

DataPtr chỉ hợp lệ trong callback đồng bộ. CanIf/PduR/COM cần copy nếu giữ dữ liệu sau callback. Với profile chỉ hỗ trợ Classical CAN data frame, DLC/format ngoài profile phải bị loại và ghi reason; không tự cắt độ dài để tạo frame hợp lệ giả. Ghi nhận Rx overrun và lỗi; driver không decode signal.

## Các lựa chọn chưa chốt cho phần cứng

Các lựa chọn sau chỉ là đề xuất; chưa áp dụng vào source/config trong lượt thiết kế.

- **ISSUE: Clock/bit timing. Option A:** fixed profile đã kiểm chứng, reject baudrate khác (ít code, ít linh hoạt). **Option B:** nhiều timing profiles trong config (linh hoạt, cần thêm validation). **Agent Rec:** A cho bước đầu; xác nhận clock và bitrate chung giữa ECU trước khi chọn giá trị.
- **ISSUE: Rx acceptance. Option A:** BasicCAN nhận rộng các data frame thuộc format hỗ trợ và CanIf lọc HRH + CAN ID (đơn giản, có tải từ frame không dùng). **Option B:** ID/mask phần cứng trong HOH config (giảm tải, cần thêm fields). **Agent Rec:** A cho baseline; không copy exact-ID filter của bài can_task rồi làm mất các ID cùng HRH.
- **ISSUE: Controller dùng trên board. Option A:** CAN0 trước, giữ model hỗ trợ nhiều controller (phù hợp BSP hiện tại). **Option B:** CAN0/CAN1 cùng lúc (cần bổ sung và xác nhận BSP CAN1). **Agent Rec:** A cho kiểm chứng đầu tiên; không bỏ validation multi-controller.
- **ISSUE: Frame format. Option A:** Classical CAN standard data frames (nhỏ, giống reference hiện có). **Option B:** hỗ trợ thêm extended/FD/remote (thêm types và policy). **Agent Rec:** A làm baseline đề xuất, nhưng assignment không chốt riêng phạm vi format này.

[NXP AN5413](https://www.nxp.com/docs/en/application-note/AN5413.pdf), phần CAN 2.0, có ví dụ chuẩn bị clock, bit timing, Message Buffers, masks, pins và polling cờ Rx/Tx. Dùng làm tham khảo phần cứng; offset/mask/thứ tự truy cập phải đối chiếu S32K144 reference manual và chế độ thực sự chọn. Không sao chép nguyên driver can_task: driver đó gắn CAN0, gọi CanUpper và callback Tx dùng HOH.

## Tiêu chí kiểm chứng khi triển khai

Unit tests deterministic với hardware fake/backend có thể điều khiển:

1. Init: config hợp lệ; baudrate 0; HOH/controller trùng; ControllerRef không tồn tại; MB sai/xung đột; timeout. Config lỗi bị chặn trước hardware access; init lỗi không READY.
2. Write: trước init, NULL PDU, HRH truyền vào HTH, ID/length sai, NULL data với length > 0; không thay runtime/tài nguyên đang gửi.
3. Accepted request giữ nguyên bytes và swPduHandle sau khi caller thay buffer/xóa U; request thứ hai trả BUSY đến khi polling giải quyết request thứ nhất.
4. Completion gọi đúng handle đúng một lần; không gọi trước completion; callback có thể gửi request tiếp theo mà state không bị mất.
5. CAN0/CAN1 cùng MB8 có state độc lập; HTH2 phải thao tác CAN1; HRH3 phải được chuyển nguyên tới CanIf.
6. Rx đúng byte order và length; format/DLC sai và overrun có sự kiện lỗi; frame hợp lệ chuyển một lần với đúng HRH + CAN ID.
7. Hardware error không tạo success confirmation; recovery chỉ release sau khi hardware an toàn.

Integration: CanIf chuyển CAN_BUSY/CAN_NOT_OK thành E_NOT_OK; COM giữ U và retry tick kế tiếp; accepted frame vẫn chứa U ở snapshot trước khi COM clear. Lịch khuyến nghị mỗi 1 ms: CAN Write polling → CAN Read polling → COM Tx main function.

Sau host tests: board loopback rồi test CAN vật lý giữa ECU với bitrate/format/CAN ID/DLC/byte packing đã thống nhất. Chưa chạy build, tests hoặc board trong lượt thiết kế này.
