# Validation type/config communication stack — Part 1

Ngày chạy: 2026-09-15. Phạm vi chỉ gồm type declarations, const configuration và liên kết Direct CAN Binding của profile Tx hiện tại. Báo cáo này không khẳng định COM, PduR hoặc CanIf runtime đã hoạt động.

## Phạm vi đã tạo

- Shared result/PDU types và COM stats type.
- PduR route/config/status/stats types; Tx route COM I-PDU0 → CanIf Tx L-PDU7 qua route11.
- CanIf Tx/Rx L-PDU/config/status/stats types; Tx mapping Global0x0010 → CAN ID0x321 → HTH0, DLC8.
- `Can_RxPduType` cho adapter training Rx tương lai.
- System matrix constants, COM period10/offset1/retries3 và node root chọn bốn module configs.
- Profile timing: main period1 ms, phase window10 ticks, Tx period chia hết10, offset riêng trong1..9.

Profile hiện tại là Tx-only tại COM/PduR/CanIf. Rx types tồn tại để triển khai receiver profile sau; chưa tạo Rx route trỏ tới một COM Rx I-PDU không tồn tại.

## Test đã chạy

Lệnh:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/host/config/run_tests.ps1
```

Kết quả:

```text
Part 1 type/config tests passed
```

Test compile C99 với `-Wall -Wextra -Werror -pedantic`, sau đó kiểm root pointers, counts, GlobalPduId xuyên COM/PduR/CanIf, local route endpoints, CAN ID, DLC, HTH role, period grid, offset range và offset uniqueness giữa mọi Tx I-PDU.

CAN Driver regression cũng đã chạy sau khi bổ sung `Can_RxPduType`:

```text
CAN host tests passed
```

## Giới hạn bằng chứng

- `arm-none-eabi-gcc` không có trong PATH của shell hiện tại, nên chưa có ARM object/full firmware build cho các file mới.
- Chưa triển khai hoặc chạy logic Init/Tx/Rx/retry của COM, PduR hoặc CanIf.
- Chưa có JSON message matrix của cả ba ECU, receiver profile, board flash hay physical CAN trace.
- Tip phase phân tán nominal due time; retry ở tick kế tiếp vẫn có thể trùng nominal slot khác.
