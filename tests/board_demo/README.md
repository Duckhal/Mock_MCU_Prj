# Two-board COM LED application

`src/main.c` is the system entry point. It initializes hardware and the
communication stack, initializes `app/app.c`, starts SysTick, optionally runs
the CanTp loopback, and dispatches the 1 ms scheduler. The compile-time switch
`SYSTEM_RUN_CANTP_LOOPBACK_TEST` defaults to `0U`; set it to `1U` only when the
startup self-test is required. Inspect `g_CanTpLoopbackTestResult`; PASS is 2
and FAIL is 3.

`SYSTEM_ENABLE_UART_CANTP_LOOPBACK` defaults to `0U`, so CAN0 returns to normal
mode after the startup self-test and can use the external bus. Set it to `1U`
only for the optional single-board internal-loopback fixture. In that fixture,
leave the board in its default Rx role: UART input is allowed to enter CanTp,
and the reassembled internal-loopback N-SDU is written back to the same UART.

`app/app.c` owns the two-board behavior. Flash the same firmware on both
S32K144 boards. Both start in Rx with all LEDs off. SW2/PTC12 changes
the role between Rx and Tx. In Tx,
the red LED stays on and each SW3/PTC13 press updates the COM LED command
through 0 (off), 1 (green), 2 (blue), 3 (green and blue). In Rx, the board
waits for a valid COM indication and applies the received command to its LEDs.
Both switches are active-low with a 20 ms debounce.

The Rx application consumes a command only when that Signal's Update Bit is
set. Periodic copies with Update Bit zero still refresh the COM I-PDU buffer,
but they do not repeat the LED operation or its `RX LED ...` UART line.

The 1 ms system loop calls `Can_MainFunction_Write()`,
`Can_MainFunction_Read()`, `CanTp_MainFunction()`, `App_MainFunction()`, and
finally `Com_MainFunctionTx()` when the application enables COM Tx. COM sends
the configured I-PDU periodically (10 ms period, 1 ms initial offset), so a
SW3 press updates the next scheduled transmission; it does not send one frame
immediately. No COM Tx scheduling runs in Rx. An already accepted CAN request
may finish after switching to Rx.

The production stack uses CAN0 at 500 kbit/s, standard CAN ID `0x100`, DLC 8.
The application uses the existing COM Gear signal slot for the LED command.
With the current COM configuration the encoded command is in payload byte 2:
`(command << 1) | update_bit`. The Update Bit is set when the command changes
and cleared once the lower layer accepts that transmission. Bytes 0, 1 and
3 through 7 are zero when other VehicleStatus signals stay at their defaults.
The logical GlobalPduId `0x0010` is selected by the static PduR/CanIf route;
it is not a payload byte. A peer board must agree on this COM packing.

Connect the boards' CAN transceivers on CAN_H/CAN_L with common ground and
appropriate termination. The BSP uses CAN0 on PTE4/PTE5 and wakes its
transceiver. For each PC log, connect a 3.3 V USB-UART adapter's RX to PTC7
(LPUART1_TX) and its ground to board ground. Use 115200 baud, 8N1. UART prints
mode and logical LED commands; it does not print the raw CAN frame. Debugger
variables `g_SystemStatus`, `g_SystemProcessedTicks`, `g_AppModeTx`, and the
app counters show system and application state.

For the multi-board CanTp text flow, connect every USB-UART adapter TX to PTC6
(`LPUART1_RX`), adapter RX to PTC7 (`LPUART1_TX`), and connect grounds. Use
115200 baud, 8 data bits, no parity, one stop bit, and no flow control. All
boards start in Rx. Press SW2 once on exactly one board to select Tx; its red
LED turns on. Leave every other board in Rx.

Text entered on the Tx board's PC is closed after a 20 ms idle gap, or split
immediately at 62 bytes, then follows this route:

```text
PC -> UART -> app -> NodeApp -> PduR -> CanTp -> CanIf -> CanDrv -> CAN 0x650
CAN 0x650/0x658 -> CanDrv -> CanIf -> CanTp -> PduR -> NodeApp -> app -> UART -> PC
```

Every Rx board reassembles the same N-SDU and writes the exact bytes to its
own UART. CR/LF bytes remain payload bytes; the application adds no prefix,
suffix, or terminator. UART input on Rx boards is discarded, and received
N-SDUs on the Tx board are consumed without being printed. The Tx board waits
for the local final CanTp confirmation. A transport abort prints
`CANTP TX FAILED`; an application timeout after 500 ms prints
`CANTP TX TIMEOUT`. Both remain recoverable application events, so the 1 ms
scheduler and SW2/SW3 handling continue to run.

All receivers use Data ID `0x650`, FC ID `0x658`, BS 4, and STmin 5 ms. Their
FC payload is therefore identical. This classroom shared-ID profile requires
exactly one data sender at a time and identical CanTp configuration on every
receiver. Physical multi-board behavior still needs validation with the
actual transceivers and bus timing.

While one transfer is active, the UART ring retains subsequent input. Inspect
`g_AppUartRxOverflows`, `g_AppCanTpTxPending`, `g_AppCanTpTxCompleted`,
`g_AppCanTpTxFailures`, `g_AppCanTpRxUartDeliveries`,
`g_AppCanTpRxIgnored`, and `g_AppCanTpInternalLoopback` in the debugger. For a
failed segmented transfer, also inspect `CanTp_LastTxAbortReason` and
`CanTp_LastRxAbortReason`.

Before selecting a UART message, the application validates its command,
switch, and lifecycle state. A corrupted command is rejected before any
out-of-range message-table access. Inspect `g_AppRuntimeStatus`,
`g_AppStateErrorMask`, `g_AppLastInvalidTxCommand`, and
`g_AppStateCorruptionCount` if the system stops with an application runtime
failure.

Host tests in this directory cover button behavior, application-owned CanTp
messages, COM Tx/Rx packing, and the production CanIf ID.
`tests/com_stack/test_main_scheduler.c` covers tick ordering. These tests do
not establish physical bus behavior or timing jitter.

Run the app regression and UART/CanTp multi-board behavior tests from the
repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tests/board_demo/run_tests.ps1
```

The reproducible log is written to
`build/uart_cantp_echo/verification.log`.
