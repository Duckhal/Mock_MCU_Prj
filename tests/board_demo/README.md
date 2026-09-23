# Three-board application demo

The firmware has one role fixed at compile time. Select exactly one value in
`app/app.h`, rebuild, then flash that image:

```c
#define APP_BOARD_ROLE APP_ROLE_MASTER
/* or APP_ROLE_SLAVE1 */
/* or APP_ROLE_SLAVE2 */
```

The same selection can be supplied by the build system with
`-DAPP_BOARD_ROLE=APP_ROLE_SLAVE1` or
`-DAPP_BOARD_ROLE=APP_ROLE_SLAVE2`. SW2 and SW3 do not change the role at
runtime.

| Firmware role | COM Tx | COM Rx | CanTp image behavior | UART |
| --- | --- | --- | --- | --- |
| `APP_ROLE_MASTER` | KeepAlive, CAN ID `0x100`, 10 ms COM period | Slave 1/2 status on `0x201`/`0x202` | Reads one PC image, splits it into N-SDUs of at most 62 bytes and transmits them | Image input and slave online/offline reports |
| `APP_ROLE_SLAVE1` | Slave 1 status, CAN ID `0x201`, 500 ms | KeepAlive `0x100` | Receives and reassembles image chunks | Exact received image bytes |
| `APP_ROLE_SLAVE2` | Slave 2 status, CAN ID `0x202`, 500 ms | KeepAlive `0x100` | Data N-PDU reception disabled | No image data |

All roles use CAN0 at 500 kbit/s with standard 11-bit identifiers and DLC 8.
The KeepAlive payload is:

```text
byte 0: AliveCounter
byte 1: KeepAliveRateLevel (0..6)
byte 2..7: zero
```

On the Master, ADC0_SE12 maps the complete 12-bit range evenly to seven rate
levels. The application updates `AliveCounter` at 500, 200, 100, 50, 20, 10,
or 5 ms. COM still transmits its current snapshot every 10 ms. A Slave only
refreshes its Master watchdog when `AliveCounter` changes. Levels 0..6 map to
blue-LED full cycles of 1500, 1000, 800, 600, 400, 300, and 200 ms. The LED is
forced off after a 2000 ms Master timeout.

For image transfer, send this binary stream to the Master UART at 115200 8N1:

```text
[uint16 image length, little endian][raw ASCII image bytes]
```

The Master preserves each chunk until CanTp returns a final result. It retries
a failed chunk up to three times, drops only that chunk after retry exhaustion,
and continues the current image. The CanTp Data/FC CAN IDs remain `0x650` and
`0x658`, with BS 4 and STmin 5 ms.

`src/main.c` initializes the stack and runs this 1 ms order:

```text
Can_MainFunction_Write
Can_MainFunction_Read
CanTp_MainFunction
App_MainFunction
Com_MainFunctionTx
```

Run deterministic host tests and strict ARM compilation from the repository
root:

```powershell
powershell -ExecutionPolicy Bypass -File tests/board_demo/run_tests.ps1
```

The log is written to `build/uart_cantp_echo/verification.log`. Host tests and
compiler checks do not replace a three-board CAN wiring and timing test.
