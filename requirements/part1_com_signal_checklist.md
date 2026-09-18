# Checklist Part 1 — COM Signal Communication Model

Trạng thái: **chưa đánh giá**. Chỉ đánh dấu `[x]` khi đã ghi đường dẫn cấu hình/mã nguồn, tên test, kết quả mong đợi và kết quả thực tế (hoặc log/ảnh trace) vào phần **Bằng chứng** tương ứng. Các mô hình và ví dụ trong tài liệu không tự chứng minh rằng firmware hiện tại đã đạt.

Nguồn: **A** = [Assignment Part 1](assignment_part1_com_signal.md) (yêu cầu chính); **N** = [Part 1 Architecture Notes](part1_architecture_notes.md) (giải thích thiết kế); **S** = [COM Internal Scheduler Tip](Com_Internal_Scheduler_Tip.txt) (khuyến nghị bố trí lịch). Số `§` chỉ mục trong A/N. Các mục ghi **Khuyến nghị** không phải điều kiện bắt buộc của Part 1. Một bằng chứng hợp lệ có thể là test deterministic trên host, trace debugger/board, bảng cấu hình đối chiếu, sơ đồ hoặc đoạn giải thích có thể review; cần ghi rõ test chạy với cấu hình nào.

## 1. Phạm vi, sở hữu và định danh

- [ ] **P01 — Giữ đúng chuỗi Tx:** Signal → Signal Group → I-PDU → PduR → CanIf Tx L-PDU → CAN ID + HTH → CanDrv Hardware Object → Controller. **Bằng chứng:** sơ đồ cấu hình với ID/handle thực tế và một trace Tx xuyên tầng. **Nguồn:** A §1, §39.
- [ ] **P02 — Giữ đúng chuỗi Rx:** Controller → Rx Hardware Object/HRH → HRH + CAN ID → CanIf Rx L-PDU → PduR → I-PDU → Signal Group → Signal. **Bằng chứng:** sơ đồ cấu hình và một trace Rx có giá trị Signal được giải mã. **Nguồn:** A §31, §39.
- [ ] **P03 — Tách quyền sở hữu cấu hình:** COM sở hữu Signal/Group/I-PDU, slot/Update Bit và lịch; PduR sở hữu route; CanIf sở hữu ánh xạ CAN ID và L-PDU; CanDrv sở hữu HOH/Controller/tài nguyên vật lý. **Bằng chứng:** bảng ownership dẫn tới struct/config API từng module; review không thấy tầng khác sửa dữ liệu thuộc sở hữu COM. **Nguồn:** A §4–6; N §21–23.
- [ ] **P04 — Mỗi Signal thuộc đúng một Group; mỗi Group thuộc đúng một I-PDU; mỗi I-PDU chứa đúng một Group.** Tra cứu Signal → Group → I-PDU, không bắt buộc Signal có `IPduRef` trực tiếp. **Bằng chứng:** bảng liên kết cấu hình và test từ chối thiếu/trùng liên kết. **Nguồn:** A §2, §9–10, §36; N §2–3.
- [ ] **P05 — Phân biệt local PduId và GlobalPduId:** local handle chỉ có nghĩa trong module; GlobalPduId là định danh logic để tương quan/trace toàn hệ thống. **Bằng chứng:** bảng đối chiếu local handle ở COM/PduR/CanIf với GlobalPduId cho ít nhất một luồng Tx và một luồng Rx. **Nguồn:** A §3.5, §10, §17; N §30.
- [ ] **P06 — Direct CAN Binding bắt buộc:** một GlobalPduId logic ánh xạ duy nhất tới CanIf L-PDU/CAN ID tương ứng; GlobalPduId được suy ra từ cấu hình, không chèn thêm byte vào payload CAN. **Bằng chứng:** bảng binding hai chiều và test/trace dựng lại GlobalPduId từ TxPduId hoặc `(HRH, CAN ID)`, kèm kiểm tra DLC/payload. **Nguồn:** A §17–18, §24, §36; N §31–33.
- [ ] **P07 — Không đưa thuật ngữ mailbox/tài nguyên vật lý vào API tầng trên:** HTH/HRH là handle tới HOH, từ đó CanDrv xác định Controller; PduR không đọc Signal, U bit, HTH/HRH hoặc Controller. **Bằng chứng:** review header/config và trace route chỉ dùng local PDU handles. **Nguồn:** A §17, §19–24; N §23–28.

## 2. Mô hình COM và kiểm tra cấu hình

- [ ] **C01 — Signal và slot:** mỗi Signal có ID, kiểu dữ liệu, `SlotStartBit`, `SlotLength`; bit 0 của slot là U, các bit còn lại là payload; `PayloadBitLength = SlotLength - 1`. **Bằng chứng:** struct/config và test encode/decode cho slot 8/16/24/32 bit. **Nguồn:** A §7–8; N §4–7.
- [ ] **C02 — SlotStartBit chia hết cho 8.** **Bằng chứng:** test cấu hình hợp lệ và test từ chối start bit lệch byte. **Nguồn:** A §7, §36 COM.
- [ ] **C03 — SlotLength chia hết cho 8.** **Bằng chứng:** test từ chối slot dài 7/9/15 bit. **Nguồn:** A §7, §36 COM.
- [ ] **C04 — SlotLength tối thiểu 8 bit.** **Bằng chứng:** test từ chối 0 bit và chấp nhận 8 bit. **Nguồn:** A §36 COM.
- [ ] **C05 — Các slot trong cùng I-PDU không chồng lấn.** **Bằng chứng:** fixture hai slot giao nhau bị từ chối, hai slot liền kề được chấp nhận. **Nguồn:** A §36 COM.
- [ ] **C06 — Mọi slot nằm trọn trong chiều dài I-PDU.** **Bằng chứng:** test slot kết thúc đúng biên và vượt biên một byte. **Nguồn:** A §36 COM.
- [ ] **C07 — Giá trị Signal vừa `SlotLength - 1` bit payload, dù kiểu C có thể rộng hơn.** **Bằng chứng:** test giới hạn min/max và giá trị vượt ngưỡng, ví dụ slot 16 bit từ chối `0x8000`. **Nguồn:** A §7, §36 COM; N §7.
- [ ] **C08 — Group không rỗng.** **Bằng chứng:** test từ chối Group có 0 Signal. **Nguồn:** A §36 Signal Group.
- [ ] **C09 — Không có Signal thuộc hai Group.** **Bằng chứng:** fixture cùng Signal xuất hiện ở hai Group bị từ chối. **Nguồn:** A §36 Signal Group.
- [ ] **C10 — Không có Group thuộc hai I-PDU.** **Bằng chứng:** fixture cùng Group tham chiếu từ hai I-PDU bị từ chối. **Nguồn:** A §36 Signal Group.
- [ ] **C11 — Mỗi I-PDU có đúng một SignalGroupRef.** **Bằng chứng:** test từ chối I-PDU không có hoặc có nhiều Group. **Nguồn:** A §10, §36 I-PDU.
- [ ] **C12 — I-PDU có local PduId, GlobalPduId, direction, length, SignalGroupRef, period/offset/max_retries cho Tx và runtime buffer.** **Bằng chứng:** bảng/struct cấu hình và test khởi tạo Tx/Rx. **Nguồn:** A §10–11, §35.
- [ ] **C13 — Mỗi I-PDU có GlobalPduId; mỗi định danh logic là duy nhất trong mô hình truyền thông toàn hệ thống.** Có thể có bản đại diện Tx/Rx cho cùng thông điệp logic; kiểm tra uniqueness phải theo thông điệp/binding, không nhầm local handle với định danh logic. **Bằng chứng:** bảng ánh xạ toàn hệ thống, test từ chối hai thông điệp khác nhau dùng cùng GlobalPduId. **Nguồn:** A §3.5, §36 I-PDU; N §30.
- [ ] **C14 — Chu kỳ Tx hợp lệ và là bội số tick COM; offset cũng là bội số tick COM.** **Bằng chứng:** test chu kỳ 0/không chia hết bị từ chối, chu kỳ và offset hợp lệ được đổi thành số tick đúng. **Nguồn:** A §12, §36 I-PDU.
- [ ] **C15 — `max_retries` là số nguyên không âm biểu diễn được bởi bộ đếm runtime.** **Bằng chứng:** test biên 0, tối đa của kiểu đếm và cấu hình vượt giới hạn bị từ chối (nếu parser cho phép). **Nguồn:** A §13–14, §36 I-PDU.
- [ ] **C16 — Tách cấu hình tĩnh khỏi trạng thái động:** period/offset/max_retries là cấu hình; counter/pending/retry_count/buffer là runtime. **Bằng chứng:** đối chiếu struct và test init/reset không làm thay đổi bảng cấu hình. **Nguồn:** A §11, §35; N §8–10.

## 3. COM Tx, scheduler và Update Bit

- [ ] **T01 — `Com_SendSignal()` tra Signal → Group → I-PDU, kiểm tra ID/con trỏ/giá trị, ghi `(Value << 1) | 1` vào slot và không tự phát CAN.** **Bằng chứng:** test invalid input/range và test buffer có U=1 nhưng chưa gọi PduR/CanIf trước tick due. **Nguồn:** A §15; N §8.
- [ ] **T02 — `Com_MainFunctionTx()` được gọi mỗi 1 ms.** **Bằng chứng:** mã scheduler 1 ms và trace/timestamp liên tiếp hoặc test giả lập đúng một lần cho mỗi tick. **Nguồn:** A §12, §14 COM-DYN-01; N §9–10.
- [ ] **T03 — Init mỗi Tx I-PDU với `counter=initialOffsetTicks`, `pending=false`, `retry_count=0`.** **Bằng chứng:** test snapshot runtime ngay sau `Com_Init`. **Nguồn:** A §13.
- [ ] **T04 — Mỗi tick giảm counter nếu >0; khi tới 0 thì đặt pending nếu chưa pending, reset retry_count cho occurrence mới và nạp lại period dù occurrence cũ còn pending.** **Bằng chứng:** bảng trace tick qua offset, kỳ kế tiếp và trường hợp BUSY kéo dài qua mốc period. **Nguồn:** A §13–14 COM-DYN-02/03/07/08.
- [ ] **T05 — Mỗi I-PDU pending được thử gửi đúng một lần trong mỗi lần gọi MainFunctionTx; thất bại thử lại ở tick kế tiếp.** **Bằng chứng:** fake PduR đếm call theo tick BUSY→BUSY→OK, không có vòng chờ. **Nguồn:** A §13–14 COM-DYN-04/05.
- [ ] **T06 — Xử lý I-PDU theo thứ tự cấu hình và tiếp tục I-PDU sau khi một PDU thất bại.** **Bằng chứng:** test hai PDU cùng due, PDU đầu BUSY và PDU sau vẫn có transmit attempt cùng tick theo đúng thứ tự. **Nguồn:** A §14 COM-DYN-05/06; N §12, §14.
- [ ] **T07 — Trạng thái pending gộp các kỳ bỏ lỡ, không tạo hàng đợi; cập nhật Signal mới trong lúc pending được dùng cho lần chấp nhận tiếp theo.** **Bằng chứng:** test BUSY qua nhiều kỳ, thay Signal giữa chừng, chỉ một request thành công với giá trị mới nhất. **Nguồn:** A §14 COM-DYN-07/11; N §16.
- [ ] **T08 — Lịch định kỳ không trôi theo lúc request được chấp nhận:** ví dụ kỳ 10 ms, request ở 10 ms BUSY rồi được chấp nhận 12 ms, kỳ tiếp theo vẫn 20 ms. **Bằng chứng:** trace due/accept với đồng hồ giả. **Nguồn:** A §14 COM-DYN-08; N §17.
- [ ] **T09 — `max_retries=N` cho tối đa `1+N` lần thử; lần thử đầu không tính retry.** **Bằng chứng:** test N=0 và N=3 đếm số lần gọi PduR khi luôn E_NOT_OK. **Nguồn:** A §14 COM-DYN-09; N §15.
- [ ] **T10 — Hết retry chỉ bỏ occurrence hiện tại, đặt pending=false/retry_count=0; I-PDU vẫn hoạt động ở kỳ nominal sau.** **Bằng chứng:** trace BUSY đến drop rồi tiếp tục đến kỳ kế tiếp; có counter/log drop. **Nguồn:** A §14 COM-DYN-10, §25.
- [ ] **T11 — Khi PduR chấp nhận (`E_OK`), xóa pending/retry_count và bit U của mọi slot thuộc I-PDU, giữ nguyên payload.** **Bằng chứng:** byte snapshot trước/sau E_OK với nhiều Signal; test xác nhận payload không đổi. **Nguồn:** A §13, §16; N §18.
- [ ] **T12 — Khi E_NOT_OK hoặc drop, giữ nguyên U bit và buffer để lần sau dùng dữ liệu mới nhất.** **Bằng chứng:** byte snapshot sau BUSY/drop và test kỳ nominal sau. **Nguồn:** A §16, §25, §14 COM-DYN-11.
- [ ] **T13 — Phân biệt request được Can_Write nhận với Tx vật lý hoàn tất và với xác nhận từ ECU đích.** **Bằng chứng:** trace trạng thái request/TxConfirmation tách biệt; tài liệu test không coi `E_OK` là remote ACK. **Nguồn:** A §26, §28–29; N §19.

## 4. PduR, CanIf và CanDrv

- [ ] **R01 — PduR route chỉ dùng local source/destination PDU handles, không sửa payload; source PDU tồn tại.** **Bằng chứng:** test route Tx/Rx với byte payload so sánh nguyên vẹn và fixture source không tồn tại bị từ chối. **Nguồn:** A §17, §36 PduR.
- [ ] **R02 — Destination PDU của từng route tồn tại.** **Bằng chứng:** fixture route destination sai bị từ chối. **Nguồn:** A §36 PduR.
- [ ] **R03 — Mỗi route truy được đúng một GlobalPduId; Direct Binding không có mapping mơ hồ từ GlobalPduId tới CanIf L-PDU/CAN ID.** **Bằng chứng:** bảng route/binding và test từ chối thiếu hoặc trùng mapping. **Nguồn:** A §17, §36 PduR.
- [ ] **R04 — CanIf Tx ánh xạ TxPduId → CAN ID + HTH rồi gọi Can_Write bằng HTH; CAN ID hợp lệ.** **Bằng chứng:** test ghi nhận tham số Can_Write; fixture CAN ID ngoài profile được từ chối. **Nguồn:** A §18, §36 CanIf Tx.
- [ ] **R05 — HthRef tồn tại và trỏ tới HOH kiểu Tx.** **Bằng chứng:** test HTH thiếu, HTH kiểu Rx bị từ chối; cấu hình hợp lệ chọn đúng Controller. **Nguồn:** A §19–20, §36 CanIf Tx.
- [ ] **R06 — CanIf Rx tra khóa `(HRH, CAN ID)` duy nhất để tìm Rx L-PDU và chuyển payload lên PduR.** **Bằng chứng:** test cùng HRH nhận nhiều CAN ID, và trùng cặp HRH/CAN ID bị từ chối. **Nguồn:** A §23–24, §36 CanIf Rx.
- [ ] **R07 — CAN ID của Rx hợp lệ; HrhRef tồn tại và trỏ tới HOH kiểu Rx.** **Bằng chứng:** fixture CAN ID sai/HRH thiếu/HRH kiểu Tx bị từ chối. **Nguồn:** A §36 CanIf Rx.
- [ ] **R08 — Driver báo Rx qua `CanIf_RxIndication(Hrh, RxPdu)`, trong đó RxPdu có CAN ID, length, data; HRH xác định Controller nên API training không cần ControllerId riêng.** **Bằng chứng:** test callback từ hai HRH/controller giả lập và kiểm tra tuple/payload. **Nguồn:** A §21–24; N §26–27.
- [ ] **R09 — Trong một CanDrv instance, CanObjectId duy nhất cho cả HTH và HRH trong cùng không gian ID.** **Bằng chứng:** test config trùng Tx–Tx, Rx–Rx, Tx–Rx đều bị từ chối. **Nguồn:** A §33, §36 CanDrv.
- [ ] **R10 — Mỗi HOH tham chiếu đúng một Controller; HOH xác định duy nhất Hardware Object và Controller.** **Bằng chứng:** test controller thiếu/trùng tham chiếu và lookup HOH ID ra đúng object/controller. **Nguồn:** A §19–20, §33, §36 CanDrv.
- [ ] **R11 — Mô hình hỗ trợ nhiều Controller: HTH/HRH của hai Controller khác nhau vẫn resolve duy nhất, kể cả khi bản phần cứng đang triển khai chỉ có CAN0.** **Bằng chứng:** bảng/fixture hai controller với HTH/HRH riêng và test lookup; ghi rõ giới hạn runtime trên board nếu chưa hỗ trợ CAN1. **Nguồn:** A §20, §38; N §24–26.
- [ ] **R12 — Khi Can_Write trả CAN_BUSY, CanIf trả E_NOT_OK và PduR truyền nguyên thất bại lên COM.** **Bằng chứng:** test fault injection CAN_BUSY ở driver và assert kết quả từng tầng; COM còn pending/U=1. **Nguồn:** A §25.
- [ ] **R13 — Nếu Tx completion dùng polling, `Can_MainFunction_Write()` nhận ra hoàn tất và gọi `CanIf_TxConfirmation()` cho đúng request; Rx polling được phục vụ nếu dùng.** **Bằng chứng:** test fake hardware accepted→busy→completed và callback đúng một lần; trace Rx polling nếu áp dụng. **Nguồn:** A §26–27; N §19–20.

## 5. Tích hợp, lịch tham khảo và trace

- [ ] **I01 — Có ca tích hợp Tx từ Com_SendSignal qua PduR/CanIf/CanDrv đến CAN ID+HTH+Controller đúng cấu hình.** **Bằng chứng:** test full-stack hoặc board trace gồm local handles, GlobalPduId, CAN ID và byte payload. **Nguồn:** A §30, §39.
- [ ] **I02 — Có ca tích hợp Rx từ HRH+CAN ID qua CanIf/PduR/COM đến Signal, kiểm tra U bit và payload.** **Bằng chứng:** inject frame rồi đọc Signal, đối chiếu dữ liệu từng tầng. **Nguồn:** A §31, §39.
- [ ] **I03 — Có trace end-to-end GlobalPduId cho Tx và Rx dù ID này không xuất hiện trong payload Direct Binding.** **Bằng chứng:** bảng `GlobalPduId ↔ local handles ↔ (HRH, CAN ID)/HTH` và log của một frame hai chiều. **Nguồn:** A §3.5, §17–18, §24, §38; N §30–34.
- [ ] **I04 — Khuyến nghị:** với driver polling, gọi `Can_MainFunction_Write()`, `Can_MainFunction_Read()` nếu cần, rồi `Com_MainFunctionTx()` trong tick 1 ms để giải phóng tài nguyên Tx trước lần retry. **Bằng chứng:** mã main loop và trace thứ tự lời gọi một tick; nếu chọn thứ tự khác, ghi lý do/ảnh hưởng. **Nguồn:** A §27; N §20.
- [ ] **I05 — Khuyến nghị từ scheduler tip:** chọn chu kỳ là bội của khoảng chung (ví dụ 10 ms) và offset khác nhau 1–9 ms trong cửa sổ đó để tách phase due. **Bằng chứng:** bảng cấu hình period/offset và bảng due cho các I-PDU; ví dụ A=20/1, B=20/3, C=30/5 có due 1/21/41, 3/23/43, 5/35/65 ms. **Nguồn:** S; A §12; N §11.
- [ ] **I06 — Khuyến nghị từ scheduler tip:** kiểm tra offset là phase ban đầu, không phải thứ tự lần truyền; retry có thể phát sinh cùng tick với PDU khác nên bố trí phase không bảo đảm tránh mọi tranh chấp. **Bằng chứng:** trace lịch nominal và test BUSY tạo retry trùng due PDU khác, cả hai vẫn được xử lý không chặn. **Nguồn:** S; A §14 COM-DYN-05/08.

## 6. Hồ sơ bàn giao bắt buộc theo A §38

Các mục dưới đây kiểm tra **sự hiện diện của hiện vật để review**; các mục trên kiểm tra tính đúng của hành vi và validation. Có thể dùng chung một file/sơ đồ cho nhiều deliverable nếu nó chỉ ra rõ từng nội dung.

- [ ] **D01 — Signal model.** **Bằng chứng:** đường dẫn định nghĩa kiểu và bảng Signal ID/kiểu/Group.
- [ ] **D02 — Signal Slot model.** **Bằng chứng:** sơ đồ slot, U/payload bit, start/length và ví dụ byte encode.
- [ ] **D03 — Signal Group model.** **Bằng chứng:** bảng Group → danh sách Signal và ràng buộc không rỗng.
- [ ] **D04 — I-PDU model có GlobalPduId.** **Bằng chứng:** bảng I-PDU ID, Global ID, direction, length, Group và buffer.
- [ ] **D05 — Direct CAN Binding map.** **Bằng chứng:** bảng GlobalPduId ↔ CanIf L-PDU ↔ CAN ID cho từng thông điệp.
- [ ] **D06 — Cấu hình period/offset/max_retries.** **Bằng chứng:** bảng giá trị Tx I-PDU và tick chuyển đổi.
- [ ] **D07 — COM Tx runtime state model.** **Bằng chứng:** sơ đồ/bảng counter, pending, retry_count, buffer và chuyển trạng thái.
- [ ] **D08 — PduR route model.** **Bằng chứng:** bảng source/destination local handle và GlobalPduId tương ứng.
- [ ] **D09 — CanIf Tx L-PDU model.** **Bằng chứng:** bảng TxPduId, CAN ID, HTH.
- [ ] **D10 — CanIf Rx L-PDU model.** **Bằng chứng:** bảng RxPduId, HRH, CAN ID.
- [ ] **D11 — CanDrv Hardware Object model.** **Bằng chứng:** bảng HOH ID/type/Controller và tài nguyên vật lý.
- [ ] **D12 — Multiple Controller model.** **Bằng chứng:** sơ đồ hoặc fixture ít nhất hai Controller với HOH riêng, nêu phạm vi runtime đã chạy.
- [ ] **D13 — Configuration Ownership View.** **Bằng chứng:** sơ đồ/bảng mỗi module sở hữu trường cấu hình nào.
- [ ] **D14 — Building Block View.** **Bằng chứng:** sơ đồ các module và giao diện Tx/Rx giữa chúng.
- [ ] **D15 — Tx Dynamic Behavior View.** **Bằng chứng:** sequence/state diagram cho due, BUSY, retry, E_OK, confirmation.
- [ ] **D16 — Rx Runtime View.** **Bằng chứng:** sequence diagram từ Controller/HRH tới Signal.
- [ ] **D17 — CAN_BUSY bounded retry/drop behavior.** **Bằng chứng:** test N=0/N>0 với số attempt, pending, U bit và kỳ sau.
- [ ] **D18 — End-to-End Tx/Rx Trace View dùng GlobalPduId.** **Bằng chứng:** một trace Tx và một trace Rx, có đủ ánh xạ local IDs/CAN ID/HOH.
- [ ] **D19 — Validation report.** **Bằng chứng:** báo cáo liệt kê toàn bộ mục C/R validation, test pass/fail, config được thử, giới hạn chưa kiểm tra trên board và đường dẫn log.

## 7. Tiêu chí giải thích theo A §39

- [ ] **A01 — Giải thích ownership:** ai sở hữu ComSignal, ComIPdu, CanIfTxPdu, CAN ID mapping, HTH/HRH, CanController; vì sao CanIf chỉ tham chiếu HTH. **Bằng chứng:** bản trả lời ngắn cho từng câu, đối chiếu bảng P03 và API/config thực tế.
- [ ] **A02 — Giải thích định danh và Rx:** HTH/HRH suy ra Controller thế nào, vì sao Rx API không cần ControllerId, vì sao `(HRH,CAN ID)` nhận diện L-PDU và dựng lại GlobalPduId; phân biệt Global ID với local handles và cách Direct Binding giữ Global ID ngoài payload. **Bằng chứng:** ví dụ cụ thể bằng ID trong cấu hình và một trace Rx.
- [ ] **A03 — Giải thích COM timing:** vì sao SendSignal không truyền ngay, vì sao I-PDU là đơn vị lập lịch, retry tick sau/không chặn/có giới hạn, drop đúng occurrence nào, vì sao giữ U bit, gộp missed occurrences và không trôi lịch khi CAN_BUSY. **Bằng chứng:** bảng tick với tối thiểu hai I-PDU và câu trả lời đối chiếu T01–T12.
- [ ] **A04 — Trình bày được Tx và Rx resolution chain đầy đủ.** **Bằng chứng:** một walkthrough Tx và một walkthrough Rx ghi từng local ID, GlobalPduId, CAN ID, HTH/HRH, Controller, Signal tương ứng.

Ngoài phạm vi bắt buộc Part 1: multiplexed Global PDU/nhúng GlobalPduId lên wire, CanTp và truyền bản tin lớn, deadline monitoring, CanIf Tx buffering và việc sao chép chính xác API AUTOSAR Classic. Chỉ thêm nếu có yêu cầu riêng; không dùng các tính năng này để thay cho Direct CAN Binding hoặc hành vi retry nêu trên. Nguồn: A §2, §17, §37; N §32, §35.
