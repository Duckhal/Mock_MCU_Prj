# Two-board COM LED application

`src/main.c` is the system entry point. It initializes hardware and the
communication stack, initializes `app/app.c`, starts SysTick, optionally runs
the CanTp loopback, and dispatches the 1 ms scheduler. The compile-time switch
`SYSTEM_RUN_CANTP_LOOPBACK_TEST` enables that startup test by default. Inspect
`g_CanTpLoopbackTestResult`; PASS is 2 and FAIL is 3.

`SYSTEM_ENABLE_UART_CANTP_LOOPBACK` is also enabled by default for the
interactive single-board test. It leaves CAN0 in internal loopback after the
startup self-test. Set this macro to `0U` before using the normal two-board CAN
demo, because internal loopback does not exercise the external CAN bus.

`app/app.c` owns the two-board behavior. Flash the same firmware on both
S32K144 boards. Both start in Rx with all LEDs off. SW2/PTC12 changes
the role between Rx and Tx. In Tx,
the red LED stays on and each SW3/PTC13 press updates the COM LED command
through 0 (off), 1 (green), 2 (blue), 3 (green and blue). In Rx, the board
waits for a valid COM indication and applies the received command to its LEDs.
Both switches are active-low with a 20 ms debounce.

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

For the single-board UART/CanTp echo test, connect the USB-UART adapter TX to
PTC6 (`LPUART1_RX`), adapter RX to PTC7 (`LPUART1_TX`), and connect grounds.
Use 115200 baud, 8 data bits, no parity, one stop bit, and no flow control.
Leave the board in its default Rx mode and send raw text from the PC. After a
20 ms idle gap, or immediately after 62 bytes, the application submits the
chunk through NodeApp -> PduR -> CanTp -> CanIf -> CanDrv. The exact bytes are
written back to UART only after CanTp reassembles the internal-loopback N-SDU.
CR/LF bytes are payload bytes and are echoed unchanged. While one transfer is
active, the UART ring can retain subsequent input; inspect
`g_AppUartRxOverflows` if the PC sends a long burst faster than CAN can drain.
The application stops with `APP_RUNTIME_CANTP_ECHO_TIMEOUT` if no complete
loopback response arrives within 500 ms.

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

Run the app regression and UART echo tests from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tests/board_demo/run_tests.ps1
```

The reproducible log is written to
`build/uart_cantp_echo/verification.log`.
