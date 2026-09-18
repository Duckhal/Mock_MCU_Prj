# Two-board TC-003 LED test

`src/main.c` has one compile-time selector:

| `BOARD_MODE` | Image | Behavior |
|---:|---|---|
| `0` | Loopback | Calls `CanLoopbackTest_Run()` from `src/can_loopback_test.c` for the 27 CanDrv/CanIf/PduR cases and COM test. |
| `1` | Board A, Tx | SW2/PTC12 cycles blue, red, green, off and sends one command per debounced press. |
| `2` | Board B, Rx | Applies validated commands to the RGB LED. |

The current source default is `BOARD_MODE=2` (Rx) when no build symbol is set.
In S32 Design Studio, create two build configurations from Debug_FLASH. Add
`BOARD_MODE=1` to the Tx configuration's C compiler defined symbols and
`BOARD_MODE=2` to the Rx configuration. Rebuild each configuration so the IDE
regenerates the source list including `src/can_loopback_test.c`, then flash the
matching image to each board.
The prebuilt standalone validation images are
`build/board_demo/1/board_mode_1.elf` and
`build/board_demo/2/board_mode_2.elf` when that build output exists locally.

Both images use CAN0 at 500 kbit/s, standard ID `0x321`, DLC 3. The wire
payload is `CA command sequence`; commands `01`, `02`, `03`, `00` mean blue,
red, green, off. This preserves the TC-003 payload behavior while using the
current CanIf CAN ID (`0x321` instead of the old reference driver's `0x123`).
The three-byte test uses CanIf directly; the current COM VehicleStatus I-PDU
is eight bytes. PduR's synchronous Rx snapshot supplies the test receiver.
No internal CAN loopback is enabled in modes 1 and 2.

Connect the two boards' CAN transceivers on CAN_H/CAN_L with common ground and
appropriate bus termination. The existing BSP sets CAN0 on PTE4/PTE5 and
wakes the transceiver. For each board's PC log, connect an appropriate 3.3 V
USB-UART adapter's RX to PTC7 (LPUART1_TX) and its ground to board ground.
Use a serial terminal at 115200 baud, 8N1. PTC6 is configured as LPUART1_RX,
but this test does not interpret commands from the PC.

The Tx terminal prints `TX CA 01 01`, `TX ACCEPTED`, then `TX DONE`. The Rx
terminal prints the received bytes such as `RX CA 01 01`. A missing ACK can
produce `TX TIMEOUT`. `g_BoardDemoResult` exposes counters and last payload
in the debugger. Host tests in this directory cover debounce, frame format,
validation, duplicates and LED mapping; they cannot prove physical bus wiring.
