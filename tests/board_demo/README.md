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

`app/app.c` owns the two-board behavior. Each firmware image has one fixed
compile-time role. In `app/app.h`, leave `APP_BOARD_ROLE` as `APP_ROLE_RX` for
receiver boards or set it to `APP_ROLE_TX` for the sender image, then rebuild
and flash that image. SW2 is not read and the role cannot be changed by a
button while running. In Tx, the red LED stays on and ADC0_SE12 on PTC14
samples the potentiometer in 12-bit mode. The application maps the sample to
one LED command:

| ADC value | Mode | State | Receiver behavior |
| --- | ---: | ---: | --- |
| 0-999 | 0 | 0 | Blue LED off |
| 1000-1999 | 1 | 1 | Blue LED on 500 ms, off 500 ms |
| 2000-2999 | 2 | 1 | Blue LED on 1000 ms, off 1000 ms |
| 3000-3999 | 3 | 1 | Blue LED on 2000 ms, off 2000 ms |
| 4000-4095 | 0 | 1 | Blue LED continuously on |

The logical command is refreshed with `Com_SendSignal()` every 10 ms while
the board is in Tx. The Rx application consumes each accepted raw COM I-PDU.
Repeated frames carrying the same blinking command do not restart its timer.

The 1 ms system loop calls `Can_MainFunction_Write()`,
`Can_MainFunction_Read()`, `CanTp_MainFunction()`, `App_MainFunction()`, and
finally `Com_MainFunctionTx()` when the application enables COM Tx. COM sends
the configured I-PDU periodically with a 10 ms period and 1 ms initial offset.
No COM Tx scheduling runs in an Rx image.

The production stack uses CAN0 at 500 kbit/s, standard CAN ID `0x100`, DLC 8.
Its payload is `[globalID][mode][state][00][00][00][00][00]`. Define
`COM_GLOBAL_ID_ENABLED` as `0U` to transmit and accept `globalID=0x00`; this is
the default. Define it as `1U` to encode the Tx LED Signal ID in byte 0. Mode
and state occupy raw bytes 1 and 2 without an Update Bit. The logical
GlobalPduId `0x0010` still selects the static PduR/CanIf route and is separate
from payload byte 0. Every peer must use the same Global ID setting and frame
layout.

Connect the boards' CAN transceivers on CAN_H/CAN_L with common ground and
appropriate termination. The BSP uses CAN0 on PTE4/PTE5 and wakes its
transceiver. For each PC log, connect a 3.3 V USB-UART adapter's RX to PTC7
(LPUART1_TX) and its ground to board ground. Use 115200 baud, 8N1. The COM LED
path writes no UART text. UART is reserved for CanTp payload input and output.
Debugger variables `g_SystemStatus`, `g_SystemProcessedTicks`, `g_AppModeTx`,
`g_AppAdcValue`, `g_AppLedMode`, `g_AppLedState`, and the app counters show
system and application state.

For the multi-board CanTp text flow, connect every USB-UART adapter TX to PTC6
(`LPUART1_RX`), adapter RX to PTC7 (`LPUART1_TX`), and connect grounds. Use
115200 baud, 8 data bits, no parity, one stop bit, and no flow control. All
boards must use the intended role image. Flash exactly one board with the Tx
image; its red LED turns on. Flash every other board with the Rx image.

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
for the local final CanTp confirmation. A transport abort or application
timeout after 500 ms updates debugger-visible status and counters without
injecting diagnostic text into the UART payload stream. Both remain
recoverable application events, so the 1 ms scheduler continues to run.

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

Before applying a received LED command, the application validates mode, state,
switch, and lifecycle state. In particular, `state=0` is accepted only with
`mode=0`. Inspect `g_AppRuntimeStatus`, `g_AppStateErrorMask`, and
`g_AppStateCorruptionCount` if the system stops with an application runtime
failure.

Host tests in this directory cover every ADC boundary, the 10 ms Signal update,
receiver blink timing, application-owned CanTp messages, and both fixed roles.
`tests/com_stack/test_main_scheduler.c` covers tick ordering. These tests do
not establish physical bus behavior or timing jitter.

Run the app regression and UART/CanTp multi-board behavior tests from the
repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tests/board_demo/run_tests.ps1
```

The reproducible log is written to
`build/uart_cantp_echo/verification.log`.
